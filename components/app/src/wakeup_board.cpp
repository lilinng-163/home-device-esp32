#include <string>
#include <cstring>
#include <atomic>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_afe_sr_iface.h>
#include <esp_afe_config.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/stream_buffer.h>

#include "wake_word.h"
#include "codec.h"
#include "xfei.h"
#include "time_sync.h"
#include "cloud_xfei_proc.h"
#include "wakeup_board.h"

static const char *TAG = "wakeup";

static TaskHandle_t afe_task_handle;
static TaskHandle_t xfei_task_handle;
static StreamBufferHandle_t s_sb; // AFE 输出 → 讯飞任务

typedef struct
{
    wake_word *wk;
    codec *mic;
    std::string appid;
    std::string apikey;
    std::string apisecret;
    cloud_llm_config llm;
} wakeup_ctx;
static wakeup_ctx s_ctx;

// ---------------- 共享的“语音状态”（两个任务都看它）----------------
enum class VoiceState : uint8_t
{
    IDLE,        // 等唤醒
    RECOGNIZING, // 已唤醒：采集/上传中
    WAITING,     // 等本轮最终结果，暂不接受下一次唤醒
};
static std::atomic<VoiceState> s_vstate{VoiceState::IDLE};
static std::atomic<unsigned> s_probe_request{0};

void wakeup_asr_probe(bool send_silence)
{
    s_probe_request.store(send_silence ? 2 : 1);
}

// 唤醒词尾音先跳过，确认是持续说话后才按静音收尾；保留 8 秒上限
static const constexpr int64_t MAX_RECORD_US = 8 * 1000 * 1000;
static const constexpr int64_t WAKE_GUARD_US = 600 * 1000;
static const constexpr int64_t MIN_SPEECH_US = 250 * 1000;
static const constexpr int64_t TAIL_SILENCE_US = 1500 * 1000;

// ---------------- AFE：读咪头 → feed → fetch → 入队 ----------------
static void afe_task(void *parameters)
{
    auto *ctx = static_cast<wakeup_ctx *>(parameters);
    int16_t *buf = ctx->wk->new_buf();
    int bytes = ctx->wk->feed_byte_size();
    int64_t t0 = 0;
    int64_t speech_start = 0;
    int64_t last_speech = 0;
    bool speech_ok = false;

    while (1)
    {
        if (ctx->mic->read(buf, bytes) <= 0)
        {
            continue;
        }
        ctx->wk->feed(buf);

        afe_fetch_result_t *res = ctx->wk->fetch();
        if (!res || res->ret_value != ESP_OK)
        {
            continue;
        }

        switch (s_vstate.load())
        {
        case VoiceState::IDLE:
        {
            if (res->wakeup_state == WAKENET_DETECTED)
            {
                ESP_LOGI(TAG, "===> wakeup! word_index=%d", res->wake_word_index);
                s_vstate = VoiceState::RECOGNIZING;
                t0 = esp_timer_get_time();
                speech_start = 0;
                last_speech = 0;
                speech_ok = false;
            }
            break;
        }
        case VoiceState::RECOGNIZING:
        {
            if (res->wakeup_state ==
                WAKENET_DETECTED) // 窗口内又说了唤醒词 → 重置窗口（打断/接着说）
            {
                ESP_LOGI(TAG, "re-wake -> reset window");
                t0 = esp_timer_get_time();
                speech_start = 0;
                last_speech = 0;
                speech_ok = false;
            }

            if (res->data && res->data_size > 0)
            {
                xStreamBufferSend(s_sb, res->data, res->data_size, 0); // 满就丢，不阻塞
            }

            int64_t now = esp_timer_get_time();
            bool after_guard = (now - t0 >= WAKE_GUARD_US);

            // 唤醒词尾音也算 VAD speech；跳过开头一小段，并要求持续说话
            if (after_guard && res->vad_state == VAD_SPEECH)
            {
                if (speech_start == 0)
                {
                    speech_start = now;
                }
                last_speech = now;
                if (now - speech_start >= MIN_SPEECH_US)
                {
                    speech_ok = true;
                }
            }

            if (now - t0 > MAX_RECORD_US) // 最长 8 秒兜底
            {
                ESP_LOGI(TAG, "record max window");
                s_vstate = VoiceState::WAITING;
            }
            else if (speech_ok && res->vad_state == VAD_SILENCE &&
                     now - last_speech > TAIL_SILENCE_US) // 确认说话后留足尾音
            {
                ESP_LOGI(TAG,
                         "vad end: speech=%lld ms, tail=%lld ms",
                         (long long)((last_speech - speech_start) / 1000),
                         (long long)((now - last_speech) / 1000));
                s_vstate = VoiceState::WAITING;
            }
            else if (!speech_ok && speech_start != 0 &&
                     now - last_speech > TAIL_SILENCE_US) // 短噪声/唤醒词尾音，不据此收尾
            {
                speech_start = 0;
                last_speech = 0;
            }
            break;
        }
        case VoiceState::WAITING:
            break;
        }
    }
}

// ---------------- 讯飞：会话状态机（连接/流式/收尾）----------------
enum class Session : uint8_t
{
    CLOSED, // 未开会话
    OPEN,   // 会话中：连接 + 流式上传
    DRAIN,  // 采集已结束：把缓冲排空再收尾
    FINISH, // 已发末帧：等最终结果（也可能晚到）
};

static const constexpr int64_t CONNECT_TIMEOUT_US = 10 * 1000 * 1000;
static const constexpr int64_t FINAL_TIMEOUT_US = 3 * 1000 * 1000;

static void xfei_task(void *parameters)
{
    auto *ctx = static_cast<wakeup_ctx *>(parameters);
    xfei_dictation xfei(ctx->appid, ctx->apikey, ctx->apisecret);
    cloud_xfei_proc_start(xfei, ctx->llm); // 先把整句回调接到文本队列

    uint8_t buf[4096]; // 每帧约 128ms PCM，减少 WS/TLS 小包次数

    Session session = Session::CLOSED;
    bool first = true;
    int64_t connect_t0 = 0;
    int64_t finish_t0 = 0;
    uint64_t tx_bytes = 0;
    uint32_t tx_frames = 0;
    int64_t tx_start = 0;
    int64_t tx_last = 0;

    auto note_tx = [&](size_t n)
    {
        if (n == 0)
        {
            return;
        }
        int64_t now = esp_timer_get_time();
        if (tx_start == 0)
        {
            tx_start = now;
        }
        tx_last = now;
        tx_bytes += n;
        tx_frames++;
    };
    auto log_tx = [&]()
    {
        if (tx_frames > 0)
        {
            ESP_LOGI(TAG,
                     "audio tx: bytes=%llu frames=%u send=%lld ms",
                     (unsigned long long)tx_bytes,
                     (unsigned)tx_frames,
                     (long long)((tx_last - tx_start) / 1000));
        }
        tx_bytes = 0;
        tx_frames = 0;
        tx_start = 0;
        tx_last = 0;
    };

    // 只有这个消费者清空音频。AFE 在 WAITING 时不会生产，避免跨任务 reset 缓冲。
    auto finish_session = [&]()
    {
        log_tx();
        s_vstate = VoiceState::WAITING;
        xfei.close();
        while (xStreamBufferReceive(s_sb, buf, sizeof(buf), 0) > 0)
        {
        }
        session = Session::CLOSED;
        s_vstate = VoiceState::IDLE;
    };

    while (1)
    {
        int64_t now = esp_timer_get_time();

        switch (session)
        {
        case Session::CLOSED:
        {
            unsigned probe = s_probe_request.exchange(0);
            if (probe != 0)
            {
                s_vstate = VoiceState::WAITING;
                xfei.set_result_delivery(false);
                if (!time_is_ready())
                {
                    time_sync();
                }

                ESP_LOGI(TAG, "asr_probe: start");
                xfei.close();
                if (xfei.easy_start() != 0)
                {
                    ESP_LOGE(TAG, "asr_probe: create/start fail");
                    finish_session();
                    xfei.set_result_delivery(true);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;
                }

                int64_t probe_t0 = esp_timer_get_time();
                while (!xfei.is_connected() && !xfei.session_failed() &&
                       esp_timer_get_time() - probe_t0 < CONNECT_TIMEOUT_US)
                {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                if (xfei.is_connected() && !xfei.session_failed())
                {
                    ESP_LOGI(TAG, "asr_probe: connected");
                    if (probe == 2)
                    {
                        // 手工发送 1 秒静音，按 40ms/帧验证上传；诊断结果绝不进入 LLM。
                        memset(buf, 0, sizeof(buf));
                        bool sent = true;
                        int64_t max_send_us = 0;
                        int64_t send_t0 = esp_timer_get_time();
                        for (int frame = 0; frame < 25; frame++)
                        {
                            int64_t frame_t0 = esp_timer_get_time();
                            if (!xfei.request(buf, 1280, frame == 0 ? 0 : 1))
                            {
                                sent = false;
                                break;
                            }
                            int64_t send_us = esp_timer_get_time() - frame_t0;
                            if (send_us > max_send_us)
                            {
                                max_send_us = send_us;
                            }
                            int64_t remaining_us =
                                send_t0 + (frame + 1) * 40000 - esp_timer_get_time();
                            if (remaining_us > 0)
                            {
                                vTaskDelay(pdMS_TO_TICKS((remaining_us + 999) / 1000));
                            }
                        }
                        ESP_LOGI(TAG,
                                 "asr_tx_probe: sent=%d elapsed=%lldms max_send=%lldms stack_min=%u",
                                 (int)sent,
                                 (long long)((esp_timer_get_time() - send_t0) / 1000),
                                 (long long)(max_send_us / 1000),
                                 (unsigned)uxTaskGetStackHighWaterMark(nullptr));
                        if (sent && xfei.request(nullptr, 0, 2))
                        {
                            int64_t final_t0 = esp_timer_get_time();
                            while (!xfei.final_received() && !xfei.session_failed() &&
                                   esp_timer_get_time() - final_t0 < FINAL_TIMEOUT_US)
                            {
                                vTaskDelay(pdMS_TO_TICKS(20));
                            }
                            ESP_LOGI(TAG, "asr_tx_probe: final=%d", (int)xfei.final_received());
                        }
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "asr_probe: timeout");
                }
                finish_session();
                xfei.set_result_delivery(true);
                ESP_LOGI(TAG, "asr_probe: done");
                break;
            }

            if (s_vstate != VoiceState::RECOGNIZING)
            {
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            }

            if (!time_is_ready()) // 时钟没同步好 → 先补，否则签名 date 错会 403
            {
                ESP_LOGW(TAG, "time not ready, sync before connect");
                time_sync();
            }

            ESP_LOGI(TAG, "wake -> connect xfei");
            xfei.close();
            if (xfei.easy_start() != 0)
            {
                ESP_LOGE(TAG, "xfei connect fail");
                finish_session();
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            }

            session = Session::OPEN;
            first = true;
            connect_t0 = esp_timer_get_time(); // time_sync/easy_start 可能耗时，重新取
            tx_bytes = 0;
            tx_frames = 0;
            tx_start = 0;
            tx_last = 0;
            break;
        }
        case Session::OPEN:
        {
            if (xfei.final_received()) // server 提前 status=2：别在 OPEN 里无限等
            {
                ESP_LOGI(TAG, "server final -> close session");
                s_vstate = VoiceState::WAITING;
                session = Session::FINISH;
                finish_t0 = now;
                break;
            }

            if (xfei.session_failed())
            {
                ESP_LOGW(TAG, "asr session failed -> stop session");
                finish_session();
                break;
            }

            if (!xfei.is_connected())
            {
                // 短指令可能在 WS 连上之前就说完；先把缓冲留住，等连上再发
                if (now - connect_t0 > CONNECT_TIMEOUT_US)
                {
                    ESP_LOGW(TAG, "xfei connect timeout");
                    finish_session();
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(20)); // 还没连上：不取，缓冲继续堆
                }
                break;
            }

            if (s_vstate != VoiceState::RECOGNIZING) // 采集结束 → 转收尾
            {
                ESP_LOGI(TAG,
                         "drain start: buffered=%u bytes",
                         (unsigned)xStreamBufferBytesAvailable(s_sb));
                session = Session::DRAIN;
                break;
            }

            // 不再人工 delay，按发送能力尽快排空缓冲
            size_t n = xStreamBufferReceive(s_sb, buf, sizeof(buf), pdMS_TO_TICKS(20));
            if (n > 0)
            {
                if (!xfei.request(buf, (int)n, first ? 0 : 1))
                {
                    ESP_LOGE(TAG, "send audio fail -> stop session");
                    finish_session();
                    break;
                }
                note_tx(n);
                first = false;
            }
            break;
        }
        case Session::DRAIN:
        {
            if (xfei.final_received())
            {
                s_vstate = VoiceState::WAITING;
                session = Session::FINISH;
                finish_t0 = now;
                break;
            }

            if (!xfei.is_connected())
            {
                ESP_LOGW(TAG, "ws lost before final");
                finish_session();
                break;
            }

            size_t n = xStreamBufferReceive(s_sb, buf, sizeof(buf), pdMS_TO_TICKS(20));
            if (n > 0)
            {
                if (!xfei.request(buf, (int)n, first ? 0 : 1))
                {
                    ESP_LOGE(TAG, "send audio fail -> stop session");
                    finish_session();
                    break;
                }
                note_tx(n);
                first = false;
            }
            else
            {
                if (first) // 一句音频都没有：先补一个首帧，再发末帧
                {
                    if (!xfei.request(nullptr, 0, 0))
                    {
                        ESP_LOGE(TAG, "send first frame fail");
                        finish_session();
                        break;
                    }
                    first = false;
                }
                if (!xfei.request(nullptr, 0, 2)) // 末帧
                {
                    ESP_LOGE(TAG, "send final frame fail");
                    finish_session();
                    break;
                }
                log_tx();
                ESP_LOGI(TAG, "session end -> wait final");
                s_vstate = VoiceState::WAITING;
                session = Session::FINISH;
                finish_t0 = now;
            }
            break;
        }
        case Session::FINISH:
        {
            // 等 DRAIN 之后晚到的 status=2；超时/断线就彻底关掉，避免旧会话跨到下一次唤醒
            if (xfei.final_received() || !xfei.is_connected() || now - finish_t0 > FINAL_TIMEOUT_US)
            {
                if (!xfei.final_received() && now - finish_t0 > FINAL_TIMEOUT_US)
                {
                    ESP_LOGW(TAG, "final timeout");
                }
                finish_session();
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            break;
        }
        }
    }
}

void wakeup_board_start(wake_word &wk,
                        codec &mic,
                        const std::string &appid,
                        const std::string &apikey,
                        const std::string &apisecret,
                        const cloud_llm_config &llm)
{
    s_ctx.wk = &wk;
    s_ctx.mic = &mic;
    s_ctx.appid = appid;
    s_ctx.apikey = apikey;
    s_ctx.apisecret = apisecret;
    s_ctx.llm = llm;

    s_sb = xStreamBufferCreateWithCaps(256 * 1024, 1, MALLOC_CAP_SPIRAM);
    if (s_sb == nullptr)
    {
        ESP_LOGE(TAG, "stream buffer create failed");
        return;
    }

    // 任务栈放 PSRAM（内部 RAM 不够）
    BaseType_t r1 = xTaskCreatePinnedToCoreWithCaps(
        afe_task, "afe_task", 8 * 1024, &s_ctx, 5, &afe_task_handle, 0, MALLOC_CAP_SPIRAM);
    // 发送任务含 4KB PCM 缓冲和 TLS 写调用链；8KB 实测只余 540B，补足 PSRAM 栈。
    BaseType_t r2 = xTaskCreatePinnedToCoreWithCaps(
        xfei_task, "xfei_task", 12 * 1024, &s_ctx, 4, &xfei_task_handle, 1, MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "tasks: afe=%d xfei=%d", (int)r1, (int)r2);
}
