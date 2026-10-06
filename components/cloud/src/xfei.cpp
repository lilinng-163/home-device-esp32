#include <string>
#include <cstring>
#include <cctype>
#include <vector>

#include <esp_log.h>
#include <esp_websocket_client.h>
#include <esp_crt_bundle.h>
#include <psa/crypto.h>
#include <mbedtls/base64.h>
#include <cJSON.h>

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
        return "";
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
static void default_websocket_event(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
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
        auto *data = static_cast<esp_websocket_event_data_t *>(event_data);
        ESP_LOGI(TAG, "recv: %.*s", (int)data->data_len, data->data_ptr ? data->data_ptr : "");
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

std::string xfei_dictation::compute_authorization(std::string &apikey, std::string &apisecret)
{
    date_ = time_rfc1123();   // 签名和 URL 共用同一个 date

    std::string signature_origin = "host: " + std::string(XFEI_HOST) + "\n"
                                   "date: " + date_ + "\n"
                                   "GET /v2/iat HTTP/1.1";

    psa_crypto_init();   // 用 PSA 前必须初始化

    // 普通软件密钥：key = apisecret 字符串本身（不是 eFuse）
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, apisecret.size() * 8);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_lifetime(&attr, PSA_KEY_LIFETIME_VOLATILE);

    psa_key_id_t key_id = 0;
    psa_status_t st = psa_import_key(&attr,
                                     reinterpret_cast<const uint8_t *>(apisecret.data()), apisecret.size(),
                                     &key_id);

    uint8_t sha[32] = {0};
    size_t  sha_len = 0;
    if (st == PSA_SUCCESS)
    {
        st = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                             reinterpret_cast<const uint8_t *>(signature_origin.data()), signature_origin.size(),
                             sha, sizeof(sha), &sha_len);
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
    ESP_LOGI(TAG, "signature: %s", signature.c_str());

    // authorization_origin
    std::string auth_origin = "api_key=\"" + apikey + "\","
                              " algorithm=\"hmac-sha256\","
                              " headers=\"host date request-line\","
                              " signature=\"" + signature + "\"";

    // authorization = base64(authorization_origin)
    std::string authorization = base64_encode(reinterpret_cast<const uint8_t *>(auth_origin.data()), auth_origin.size());
    ESP_LOGI(TAG, "res: %s", authorization.c_str());
    return authorization;
}

void xfei_dictation::init(std::string &apikey, std::string &apisecret, xfei_dictation_cb_t cb)
{
    std::string authorization = compute_authorization(apikey, apisecret);   // 同时填好 date_

    // 组 wss 地址：authorization/date/host 三处都要 URL 编码
    uri_ = "wss://" + std::string(XFEI_HOST) + "/v2/iat"
           "?authorization=" + url_encode(authorization) +
           "&date=" + url_encode(date_) +
           "&host=" + url_encode(XFEI_HOST);

    memset(&cfg_, 0, sizeof(cfg_));
    cfg_.uri = uri_.c_str();
    cfg_.network_timeout_ms = 10000;
    cfg_.crt_bundle_attach = esp_crt_bundle_attach;   // wss 用系统根证书校验
    cfg_.reconnect_timeout_ms = 1000;

    handle_ = esp_websocket_client_init(&cfg_);

    xfei_dictation_cb_t handler = (cb != nullptr) ? cb : default_websocket_event;
    esp_websocket_register_events(handle_, WEBSOCKET_EVENT_ANY, handler, this);
}

void xfei_dictation::set_event_cb(xfei_dictation_cb_t cb, void *ctx)
{
    esp_websocket_register_events(handle_, WEBSOCKET_EVENT_ANY, cb, ctx ? ctx : this);
}

int xfei_dictation::easy_start(std::string &apikey, std::string &apisecret)
{
    compute_authorization(apikey, apisecret);
    init(apikey, apisecret, nullptr);
    auto ret = start();
    if (ret != 0)
    {
        return -1;
    }
    return 0;
}

int xfei_dictation::start(void)
{
    auto ret = esp_websocket_client_start(handle_);
    if (ret != ESP_OK)
    {
        esp_websocket_client_stop(handle_);
        esp_websocket_client_destroy(handle_);
        return -1;
    }
    return 0;
}

// 发一帧讯飞 iat 请求：pcm 为 16k/mono/16bit 的原始 PCM；status 0=首 1=中 2=末
void xfei_dictation::request(const uint8_t *pcm, int len, int status)
{
    std::string audio;
    if (status != 2 && pcm != nullptr && len > 0)
        audio = base64_encode(pcm, len);

    std::string data = "\"data\":{\"status\":" + std::to_string(status) +
                       ",\"format\":\"audio/L16;rate=16000\",\"encoding\":\"raw\"," +
                       "\"audio\":\"" + audio + "\"}";

    std::string frame;
    if (status == 0)
    {
        std::string biz = "\"business\":{\"language\":\"zh_cn\",\"domain\":\"iat\","
                          "\"accent\":\"mandarin\",\"vad_eos\":3000}";
        frame = "{\"common\":{\"app_id\":\"" + appid_ + "\"}," + biz + "," + data + "}";
    }
    else
    {
        frame = "{" + data + "}";
    }

    esp_websocket_client_send_text(handle_, frame.c_str(), frame.size(), 10000);
    ESP_LOGD(TAG, "send frame status=%d audio_bytes=%d frame_len=%d", status, len, (int)frame.size());
}

void xfei_dictation::set_on_data_cb(xfei_dictation_on_data_cb_t cb)
{
    on_data_cb_ = cb;
}

void xfei_dictation::on_connected(void)
{
    ESP_LOGI(TAG, "ws connected");
}

void xfei_dictation::on_disconnected(void)
{
    ESP_LOGW(TAG, "ws disconnected");
}

void xfei_dictation::on_data(void *event_data)
{
    if (on_data_cb_)
    {
        on_data_cb_(event_data);    // 传队列
    }

    auto *d = static_cast<esp_websocket_event_data_t *>(event_data);
    if (!d || !d->data_ptr || d->data_len <= 0)
        return;

    cJSON *root = cJSON_ParseWithLength(d->data_ptr, d->data_len);
    if (!root)
        return;

    cJSON *data = cJSON_GetObjectItem(root, "data");

    // 拼识别文本：data.result.ws[].cw[].w
    cJSON *result = data ? cJSON_GetObjectItem(data, "result") : nullptr;
    cJSON *ws = result ? cJSON_GetObjectItem(result, "ws") : nullptr;
    for (int i = 0, n = cJSON_GetArraySize(ws); i < n; i++)
    {
        cJSON *cw = cJSON_GetObjectItem(cJSON_GetArrayItem(ws, i), "cw");
        for (int j = 0, m = cJSON_GetArraySize(cw); j < m; j++)
        {
            cJSON *w = cJSON_GetObjectItem(cJSON_GetArrayItem(cw, j), "w");
            if (cJSON_IsString(w))
                text_ += w->valuestring;
        }
    }

    // status==2 表示本句结束 → 输出完整文本
    cJSON *status = data ? cJSON_GetObjectItem(data, "status") : nullptr;
    if (cJSON_IsNumber(status) && status->valueint == 2)
    {
        ESP_LOGI(TAG, "===> result: %s", text_.c_str());
        text_.clear();
    }

    cJSON_Delete(root);
}

void xfei_dictation::on_error(void)
{
    ESP_LOGE(TAG, "ws error");
}

void xfei_dictation::on_finish(void)
{
    ESP_LOGI(TAG, "ws finish");
}

xfei_dictation::~xfei_dictation(void)
{
    esp_websocket_client_stop(handle_);
    esp_websocket_client_destroy(handle_);
}
