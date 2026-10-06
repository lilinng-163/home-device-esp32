#include <string>
#include <cstring>

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
#include "wakeup_board.h"

static const char *TAG = "wakeup";

static TaskHandle_t afe_task_handle;
static TaskHandle_t xfei_task_handle;
static StreamBufferHandle_t s_sb;      // AFE 输出 → 讯飞任务

typedef struct
{
    wake_word  *wk;
    codec      *mic;
    std::string appid;
    std::string apikey;
    std::string apisecret;
} wakeup_ctx;
static wakeup_ctx s_ctx;

// ---------------- 共享的“语音状态”（两个任务都看它）----------------
enum class VoiceState : uint8_t
{
    IDLE,          // 等唤醒
    RECOGNIZING,   // 已唤醒：采集/上传中
};
static volatile VoiceState s_vstate = VoiceState::IDLE;

// ---------------- AFE：读咪头 → feed → fetch → 入队 ----------------
static void afe_task(void *parameters)
{
    auto *ctx = static_cast<wakeup_ctx *>(parameters);
    int16_t *buf = ctx->wk->new_buf();
    int bytes = ctx->wk->feed_byte_size();
    int64_t t0 = 0;

    while (1)
    {
        if (ctx->mic->read(buf, bytes) <= 0)
            continue;
        ctx->wk->feed(buf);

        afe_fetch_result_t *res = ctx->wk->fetch();
        if (!res || res->ret_value != ESP_OK)
        {
        continue;
        }
        
        switch (s_vstate)
        {
            case VoiceState::IDLE:
            {
                if (res->wakeup_state == WAKENET_DETECTED)
                {
                    ESP_LOGI(TAG, "===> wakeup! word_index=%d", res->wake_word_index);
                    s_vstate = VoiceState::RECOGNIZING;
                    t0 = esp_timer_get_time();
                }
                break;
            }
        case VoiceState::RECOGNIZING:
        {
            if (res->wakeup_state == WAKENET_DETECTED)   // 窗口内又说了唤醒词 → 重置窗口（打断/接着说）
            {
                ESP_LOGI(TAG, "re-wake -> reset window");
                t0 = esp_timer_get_time();
            }

            if (res->data && res->data_size > 0)
                xStreamBufferSend(s_sb, res->data, res->data_size, 0);   // 满就丢，不阻塞

            if (esp_timer_get_time() - t0 > 8 * 1000 * 1000)             // 采集窗口最长 8 秒
                s_vstate = VoiceState::IDLE;
            break;
        }
        }
    }
}

// ---------------- 讯飞：会话状态机（连接/流式/收尾）----------------
enum class Session : uint8_t
{
    CLOSED,    // 未开会话
    OPEN,      // 会话中：连接 + 流式上传
    DRAIN,     // 采集已结束：把缓冲排空再收尾
};

static void xfei_task(void *parameters)
{
    auto *ctx = static_cast<wakeup_ctx *>(parameters);
    xfei_dictation xfei(ctx->appid, ctx->apikey, ctx->apisecret);
    uint8_t buf[2048];

    Session session = Session::CLOSED;
    bool    first   = true;

    while (1)
    {
        switch (session)
        {
        case Session::CLOSED:
        {
            if (s_vstate == VoiceState::RECOGNIZING)
            {
                if (!time_is_ready())          // 时钟没同步好 → 先补，否则签名 date 错会 403
                {
                    ESP_LOGW(TAG, "time not ready, sync before connect");
                    time_sync();
                }
                ESP_LOGI(TAG, "wake -> connect xfei");
                xfei.close();
                xfei.easy_start();
                session = Session::OPEN;
                first = true;
                vTaskDelay(pdMS_TO_TICKS(300));   // 等连上再发
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            break;
        }
        case Session::OPEN:
        {
            if (s_vstate == VoiceState::IDLE)      // 采集结束 → 转收尾
            {
                session = Session::DRAIN;
                break;
            }

            if (xfei.is_connected())
            {
                size_t n = xStreamBufferReceive(s_sb, buf, sizeof(buf), pdMS_TO_TICKS(50));
                if (n > 0)
                {
                    xfei.request(buf, (int)n, first ? 0 : 1);
                    first = false;
                    vTaskDelay(pdMS_TO_TICKS(30));   // 按音频速率节流
                }
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(20));       // 还没连上：不取，缓冲继续堆
            }
            break;
        }
        case Session::DRAIN:
        {
            if (!xfei.is_connected())
            {
                session = Session::CLOSED;
                break;
            }

            size_t n = xStreamBufferReceive(s_sb, buf, sizeof(buf), pdMS_TO_TICKS(20));
            if (n > 0)
            {
                xfei.request(buf, (int)n, first ? 0 : 1);
                first = false;
            }
            else
            {
                xfei.request(nullptr, 0, 2);          // 末帧
                ESP_LOGI(TAG, "session end -> idle");
                session = Session::CLOSED;
            }
            break;
        }
        }
    }
}

void wakeup_board_start(wake_word &wk, codec &mic,
                        const std::string &appid,
                        const std::string &apikey,
                        const std::string &apisecret)
{
    s_ctx.wk = &wk;
    s_ctx.mic = &mic;
    s_ctx.appid = appid;
    s_ctx.apikey = apikey;
    s_ctx.apisecret = apisecret;

    s_sb = xStreamBufferCreateWithCaps(256 * 1024, 1, MALLOC_CAP_SPIRAM);
    if (s_sb == nullptr)
    {
        ESP_LOGE(TAG, "stream buffer create failed");
        return;
    }

    // 任务栈放 PSRAM（内部 RAM 不够）
    BaseType_t r1 = xTaskCreatePinnedToCoreWithCaps(afe_task,  "afe_task",  8 * 1024, &s_ctx,  5, &afe_task_handle,  0, MALLOC_CAP_SPIRAM);
    BaseType_t r2 = xTaskCreatePinnedToCoreWithCaps(xfei_task, "xfei_task", 8 * 1024, &s_ctx, 4, &xfei_task_handle, 1, MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "tasks: afe=%d xfei=%d", (int)r1, (int)r2);
}
