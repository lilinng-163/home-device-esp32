#include <string>
#include <cstring>
#include <cctype>
#include <vector>

#include <esp_log.h>
#include <esp_websocket_client.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>
#include <esp_memory_utils.h>
#include <psa/crypto.h>
#include <mbedtls/base64.h>
#include <cJSON.h>
#include <esp_transport_ssl.h>
#include <esp_transport_ws.h>
#include <lwip/sockets.h>
#include <lwip/tcp.h>

#include "time_sync.h"
#include "xfei.h"

static const constexpr char *TAG = "xfei";
static const constexpr char *XFEI_HOST = "iat-api.xfyun.cn";

// base64（mbedtls 不写结尾 '\0'，包一层返回 std::string）
static std::string base64_encode(const uint8_t *data, size_t len)
{
    std::vector<unsigned char> out(4 * ((len + 2) / 3) + 4, 0);
    size_t olen = 0;
    if (mbedtls_base64_encode(out.data(), out.size(), &olen, data, len) != 0)
    {
        return "";
    }
    return std::string(reinterpret_cast<char *>(out.data()), olen);
}

// URL 百分号编码（base64 的 = + / 等都会转成 %XX）
static std::string url_encode(const std::string &s)
{
    static const char *hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            o += (char)c;
        }
        else
        {
            o += '%';
            o += hex[c >> 4];
            o += hex[c & 0x0F];
        }
    }
    return o;
}

// 默认最小 websocket 事件回调（自由函数，ctx = xfei_dictation*）
static void
default_websocket_event(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)base;
    xfei_dictation *self = static_cast<xfei_dictation *>(ctx);
    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
    {
        self->on_connected();
        break;
    }
    case WEBSOCKET_EVENT_DISCONNECTED:
    {
        self->on_disconnected();
        break;
    }
    case WEBSOCKET_EVENT_DATA:
    {
        self->on_data(event_data);
        break;
    }
    case WEBSOCKET_EVENT_ERROR:
    {
        self->on_error();
        break;
    }
    case WEBSOCKET_EVENT_FINISH:
    {
        self->on_finish();
        break;
    }
    default:
    {
        break;
    }
    }
}

xfei_dictation::xfei_dictation(const std::string &appid,
                               const std::string &apikey,
                               const std::string &apisecret)
    : appid_(appid), apikey_(apikey), apisecret_(apisecret)
{
}

std::string xfei_dictation::compute_authorization(void)
{
    date_ = time_rfc1123(); // 签名和 URL 共用同一个 date

    std::string signature_origin = "host: " + std::string(XFEI_HOST) +
                                   "\n"
                                   "date: " +
                                   date_ +
                                   "\n"
                                   "GET /v2/iat HTTP/1.1";

    psa_crypto_init(); // 用 PSA 前必须初始化

    // 普通软件密钥：key = apisecret 字符串本身（不是 eFuse）
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, apisecret_.size() * 8);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_lifetime(&attr, PSA_KEY_LIFETIME_VOLATILE);

    psa_key_id_t key_id = 0;
    psa_status_t st = psa_import_key(
        &attr, reinterpret_cast<const uint8_t *>(apisecret_.data()), apisecret_.size(), &key_id);

    uint8_t sha[32] = {0};
    size_t sha_len = 0;
    if (st == PSA_SUCCESS)
    {
        st = psa_mac_compute(key_id,
                             PSA_ALG_HMAC(PSA_ALG_SHA_256),
                             reinterpret_cast<const uint8_t *>(signature_origin.data()),
                             signature_origin.size(),
                             sha,
                             sizeof(sha),
                             &sha_len);
        psa_destroy_key(key_id);
    }
    psa_reset_key_attributes(&attr);

    if (st != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "hmac fail: %d", (int)st);
        return "";
    }

    // signature = base64(HMAC-SHA256)
    std::string signature = base64_encode(sha, sha_len);

    // authorization_origin
    std::string auth_origin = "api_key=\"" + apikey_ +
                              "\","
                              " algorithm=\"hmac-sha256\","
                              " headers=\"host date request-line\","
                              " signature=\"" +
                              signature + "\"";

    // authorization = base64(authorization_origin)
    return base64_encode(reinterpret_cast<const uint8_t *>(auth_origin.data()), auth_origin.size());
}

void xfei_dictation::init(xfei_dictation_cb_t cb)
{
    close(); // 已有客户端先释放，避免重复 init 泄漏

    std::string authorization = compute_authorization(); // 同时填好 date_

    // 组 wss 地址：authorization/date/host 三处都要 URL 编码
    uri_ = "wss://" + std::string(XFEI_HOST) +
           "/v2/iat"
           "?authorization=" +
           url_encode(authorization) + "&date=" + url_encode(date_) +
           "&host=" + url_encode(XFEI_HOST);

    // 显式持有标准 TLS/WS transport，才能在握手后配置流式音频的 TCP_NODELAY。
    // WS transport 不拥有 TLS parent；列表统一释放，顺序与官方客户端一致。
    transports_ = esp_transport_list_init();
    if (transports_ == nullptr)
    {
        ESP_LOGE(TAG, "transport list init fail");
        return;
    }

    esp_transport_handle_t tls = esp_transport_ssl_init();
    if (tls == nullptr)
    {
        close();
        return;
    }
    if (esp_transport_list_add(transports_, tls, "tls") != ESP_OK)
    {
        esp_transport_destroy(tls);
        close();
        return;
    }
    esp_transport_ssl_crt_bundle_attach(tls, esp_crt_bundle_attach);

    ws_transport_ = esp_transport_ws_init(tls);
    if (ws_transport_ == nullptr)
    {
        close();
        return;
    }
    if (esp_transport_list_add(transports_, ws_transport_, "wss") != ESP_OK)
    {
        esp_transport_destroy(ws_transport_);
        ws_transport_ = nullptr;
        close();
        return;
    }
    esp_transport_set_default_port(ws_transport_, 443);

    std::string path = uri_.substr(uri_.find("/v2/iat"));
    esp_transport_ws_config_t ws_config = {};
    ws_config.ws_path = path.c_str();
    ws_config.propagate_control_frames = true;
    if (esp_transport_ws_set_config(ws_transport_, &ws_config) != ESP_OK)
    {
        close();
        return;
    }

    memset(&cfg_, 0, sizeof(cfg_));
    cfg_.uri = uri_.c_str();
    cfg_.ext_transport = ws_transport_;
    cfg_.network_timeout_ms = 10000;
    // sdkconfig 将大于 4KB 的普通分配优先放入 PSRAM；8KB 收发缓冲不再挤占任务栈。
    // 一帧 4096B PCM 编码后约 5.5KB，8KB 可避免拆成多段 WS/TLS 小包。
    cfg_.buffer_size = 8 * 1024;
    cfg_.task_stack = 12 * 1024;
    cfg_.crt_bundle_attach = esp_crt_bundle_attach; // wss 用系统根证书校验
    cfg_.reconnect_timeout_ms = 5000;
    cfg_.disable_auto_reconnect = true; // 每唤醒手动连，别自己乱重连

    ESP_LOGD(TAG,
             "heap: internal=%u KB, spiram=%u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

    handle_ = esp_websocket_client_init(&cfg_);
    if (handle_ == nullptr)
    {
        ESP_LOGE(TAG, "websocket init fail");
        return;
    }
    ESP_LOGI(TAG,
             "ws task stack=%d, internal=%uKB largest=%uKB spiram=%uKB",
             cfg_.task_stack,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

    xfei_dictation_cb_t handler = (cb != nullptr) ? cb : default_websocket_event;
    esp_websocket_register_events(handle_, WEBSOCKET_EVENT_ANY, handler, this);
}

void xfei_dictation::set_event_cb(xfei_dictation_cb_t cb, void *ctx)
{
    if (handle_ == nullptr)
    {
        return;
    }
    esp_websocket_register_events(handle_, WEBSOCKET_EVENT_ANY, cb, ctx ? ctx : this);
}

int xfei_dictation::easy_start(void)
{
    init(nullptr);
    auto ret = start();
    if (ret != 0)
    {
        return -1;
    }
    return 0;
}

int xfei_dictation::start(void)
{
    if (handle_ == nullptr)
    {
        ESP_LOGE(TAG, "start before init");
        return -1;
    }

    reset_session();

    auto ret = esp_websocket_client_start(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "websocket start fail: %s", esp_err_to_name(ret));
        esp_websocket_client_stop(handle_);
        esp_websocket_client_destroy(handle_);
        handle_ = nullptr;
        return -1;
    }
    return 0;
}

// 发一帧讯飞 iat 请求：pcm 为 16k/mono/16bit 的原始 PCM；status 0=首 1=中 2=末
bool xfei_dictation::request(const uint8_t *pcm, int len, int status)
{
    std::string audio;
    if (status != 2 && pcm != nullptr && len > 0)
    {
        audio = base64_encode(pcm, len);
    }

    std::string data = "\"data\":{\"status\":" + std::to_string(status) +
                       ",\"format\":\"audio/L16;rate=16000\",\"encoding\":\"raw\"," +
                       "\"audio\":\"" + audio + "\"}";

    std::string frame;
    if (status == 0)
    {
        std::string biz = "\"business\":{\"language\":\"zh_cn\",\"domain\":\"iat\","
                          "\"accent\":\"mandarin\",\"vad_eos\":2000}";
        frame = "{\"common\":{\"app_id\":\"" + appid_ + "\"}," + biz + "," + data + "}";
    }
    else
    {
        frame = "{" + data + "}";
    }

    if (!is_connected())
    {
        return false; // 未连接就别发，避免阻塞
    }

    int sent =
        esp_websocket_client_send_text(handle_, frame.c_str(), frame.size(), pdMS_TO_TICKS(1000));
    if (sent < 0 || (size_t)sent != frame.size())
    {
        ESP_LOGE(TAG, "send fail: status=%d sent=%d frame=%d", status, sent, (int)frame.size());
        return false;
    }

    ESP_LOGD(
        TAG, "send frame status=%d audio_bytes=%d frame_len=%d", status, len, (int)frame.size());
    return true;
}

void xfei_dictation::set_on_data_cb(xfei_dictation_on_data_cb_t cb)
{
    on_data_cb_ = cb;
}

void xfei_dictation::set_final_cb(xfei_dictation_final_cb_t cb)
{
    final_cb_ = cb;
}

void xfei_dictation::reset_pending(void)
{
    text_.clear();
    ws_buf_.clear();
    ws_open_ = false;
    ws_frame_base_ = 0;
}

void xfei_dictation::reject_session(void)
{
    reset_pending();
    ws_bad_ = true;
    session_failed_.store(true);
}

void xfei_dictation::reset_session(void)
{
    reset_pending();
    ws_bad_ = false;
    final_received_.store(false);
    session_failed_.store(false);
}

void xfei_dictation::on_connected(void)
{
    reset_pending();

    // WS header 与 payload 是两次 TCP 写。关闭 Nagle，避免小 header 等待延迟 ACK。
    int socket = esp_transport_get_socket(ws_transport_);
    int enabled = 1;
    if (socket < 0 || setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) != 0)
    {
        ESP_LOGE(TAG, "TCP_NODELAY setup fail");
        reject_session();
        return;
    }

    ESP_LOGI(TAG,
             "ws connected, internal=%uKB spiram=%uKB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

    TaskHandle_t ws_task = xTaskGetHandle("websocket_task");
    if (ws_task != nullptr)
    {
        uint8_t *stack = pxTaskGetStackStart(ws_task);
        ESP_LOGI(TAG,
                 "ws stack ext=%d free=%u bytes",
                 (int)esp_ptr_external_ram(stack),
                 (unsigned)uxTaskGetStackHighWaterMark(ws_task));
    }
}

void xfei_dictation::on_disconnected(void)
{
    ESP_LOGW(TAG, "ws disconnected");
    reject_session();
}

void xfei_dictation::on_data(void *event_data)
{
    if (on_data_cb_)
    {
        on_data_cb_(event_data); // 兼容旧接口：只同步回调，不要保存 event_data
    }

    auto *d = static_cast<esp_websocket_event_data_t *>(event_data);
    if (d == nullptr || d->data_ptr == nullptr || d->data_len < 0 || ws_bad_ ||
        final_received_.load())
    {
        return;
    }
    if (d->op_code != 0x1 && d->op_code != 0x0) // 只处理 text / continuation
    {
        return;
    }

    // payload_offset 是当前帧内偏移；FIN 只表示最后一帧，不表示最后一个接收块。
    if (d->payload_offset == 0)
    {
        if (d->op_code == 0x1)
        {
            if (ws_open_)
            {
                reject_session();
                return;
            }
            ws_buf_.clear();
            ws_open_ = true;
        }
        else if (!ws_open_)
        {
            reject_session();
            return;
        }
        ws_frame_base_ = ws_buf_.size();
    }
    if (d->payload_offset < 0 || !ws_open_ ||
        ws_buf_.size() != ws_frame_base_ + (size_t)d->payload_offset)
    {
        reject_session();
        return;
    }

    if (d->payload_len < 0 || d->payload_len > (int)ws_max || d->payload_offset > d->payload_len ||
        d->data_len > d->payload_len - d->payload_offset)
    {
        ESP_LOGW(TAG, "ws payload too long: %d", d->payload_len);
        reject_session();
        return;
    }
    if (ws_buf_.size() + (size_t)d->data_len > ws_max)
    {
        ESP_LOGW(TAG, "ws buffer overflow");
        reject_session();
        return;
    }

    ws_buf_.append(d->data_ptr, d->data_len);

    if (d->payload_offset + d->data_len < d->payload_len || !d->fin)
    {
        return;
    }

    std::string msg = ws_buf_;
    ws_buf_.clear();
    ws_open_ = false;
    handle_message(msg);
}

void xfei_dictation::handle_message(const std::string &msg)
{
    cJSON *root = cJSON_ParseWithLength(msg.data(), msg.size());
    if (root == nullptr)
    {
        ESP_LOGW(TAG, "bad json");
        reject_session();
        return;
    }

    cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(code) || code->valueint != 0)
    {
        ESP_LOGW(TAG, "xfei code=%d", cJSON_IsNumber(code) ? code->valueint : -1);
        reject_session();
        cJSON_Delete(root);
        return;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data))
    {
        ESP_LOGW(TAG, "no data object");
        reject_session();
        cJSON_Delete(root);
        return;
    }

    bool bad = false;
    cJSON *result = cJSON_GetObjectItemCaseSensitive(data, "result");
    if (result != nullptr && !cJSON_IsObject(result))
    {
        bad = true;
    }
    else if (cJSON_IsObject(result))
    {
        cJSON *ws = cJSON_GetObjectItemCaseSensitive(result, "ws");
        if (ws != nullptr && !cJSON_IsArray(ws))
        {
            bad = true;
        }
        else if (cJSON_IsArray(ws))
        {
            for (int i = 0, n = cJSON_GetArraySize(ws); i < n && !bad; i++)
            {
                cJSON *one = cJSON_GetArrayItem(ws, i);
                if (!cJSON_IsObject(one))
                {
                    bad = true;
                    break;
                }

                cJSON *cw = cJSON_GetObjectItemCaseSensitive(one, "cw");
                if (cw != nullptr && !cJSON_IsArray(cw))
                {
                    bad = true;
                    break;
                }
                if (!cJSON_IsArray(cw) || cJSON_GetArraySize(cw) <= 0)
                {
                    continue;
                }

                // 每个 ws 只取第一候选
                cJSON *first = cJSON_GetArrayItem(cw, 0);
                cJSON *w = cJSON_GetObjectItemCaseSensitive(first, "w");
                if (!cJSON_IsString(w))
                {
                    bad = true;
                    break;
                }
                text_ += w->valuestring;
            }
        }
    }

    cJSON *status = cJSON_GetObjectItemCaseSensitive(data, "status");
    if (!cJSON_IsNumber(status))
    {
        bad = true;
    }

    if (bad)
    {
        ESP_LOGW(TAG, "bad xfei message");
        reject_session();
        cJSON_Delete(root);
        return;
    }

    if (text_.size() > text_max)
    {
        ESP_LOGW(TAG, "text too long");
        reject_session();
        cJSON_Delete(root);
        return;
    }

    if (status->valueint == 2)
    {
        if (text_.empty())
        {
            ESP_LOGW(TAG, "empty final text");
            reject_session();
            final_received_.store(true); // 服务器已收尾，别让状态机在 OPEN 里等满窗口
        }
        else
        {
            std::string sentence = text_;
            text_.clear();
            final_received_.store(true);
            ESP_LOGI(TAG, "final: %s", sentence.c_str());
            if (deliver_results_ && final_cb_)
            {
                final_cb_(sentence); // 回调里只做固定长度拷贝入队
            }
        }
    }
    else if (status->valueint != 0 && status->valueint != 1)
    {
        ESP_LOGW(TAG, "bad status=%d", status->valueint);
        reject_session();
    }

    cJSON_Delete(root);
}

void xfei_dictation::on_error(void)
{
    ESP_LOGE(TAG, "ws error");
    reject_session();
}

void xfei_dictation::on_finish(void)
{
    ESP_LOGI(TAG, "ws finish, stack_min=%u bytes",
             (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    reject_session();
}

void xfei_dictation::close(void)
{
    if (handle_ != nullptr)
    {
        esp_websocket_client_stop(handle_);
        esp_websocket_client_destroy(handle_);
        handle_ = nullptr;
    }
    // 外部 transport 的生命周期由本类负责，必须等 WS 任务退出后才能销毁。
    if (transports_ != nullptr)
    {
        esp_transport_list_destroy(transports_);
        transports_ = nullptr;
        ws_transport_ = nullptr;
    }
    reset_session();
}

xfei_dictation::~xfei_dictation(void)
{
    close();
}
