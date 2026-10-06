#include <esp_tls.h>
#include <esp_log.h>

#include "https_server.h"

static const constexpr char *TAG = "https_server";

// ---------- 默认最小事件回调（自由函数，ctx = https_server*）----------
static void default_https_event(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)ctx;
    if (base != ESP_HTTPS_SERVER_EVENT)
        return;

    switch (event_id)
    {
    case HTTPS_SERVER_EVENT_ERROR:
    {
        auto *e = (esp_tls_last_error_t *)event_data;
        ESP_LOGE(TAG, "error: %s, tls_err=%d", esp_err_to_name(e->last_error), e->esp_tls_error_code);
        break;
    }
    case HTTPS_SERVER_EVENT_START:
    {
        ESP_LOGI(TAG, "server start");
        break;
    }
    case HTTPS_SERVER_EVENT_ON_CONNECTED:
    {
        ESP_LOGI(TAG, "client connected");
        break;
    }
    case HTTPS_SERVER_EVENT_ON_DATA:
    {
        ESP_LOGI(TAG, "data");
        break;
    }
    case HTTPS_SERVER_EVENT_SENT_DATA:
    {
        ESP_LOGI(TAG, "data sent");
        break;
    }
    case HTTPS_SERVER_EVENT_DISCONNECTED:
    {
        ESP_LOGI(TAG, "client disconnected");
        break;
    }
    case HTTPS_SERVER_EVENT_STOP:
    {
        ESP_LOGI(TAG, "server stop");
        break;
    }
    default:
    {
        break;
    }
    }
}

// ---------- 默认最小 user_cb（这个回调没有 ctx，只能打日志）----------
static void default_https_user_cb(esp_https_server_user_cb_arg_t *user_cb)
{
    switch (user_cb->user_cb_state)
    {
    case HTTPD_SSL_USER_CB_SESS_CREATE:
    {
        ESP_LOGD(TAG, "session create");
        break;
    }
    case HTTPD_SSL_USER_CB_SESS_CLOSE:
    {
        ESP_LOGD(TAG, "session close");
        break;
    }
    case HTTPD_SSL_USER_CB_SESS_ERROR:
    {
        ESP_LOGW(TAG, "handshake failed");
        break;
    }
    default:
    {
        break;
    }
    }
}

void https_server::init(httpd_uri_t &root, https_cb_t cb, https_user_cb_t https_user_cb)
{
    root_ = root;

    // 外部传了就用外部的，否则用默认
    https_cb_t      event_cb = (cb != nullptr) ? cb : default_https_event;
    https_user_cb_t user_cb  = (https_user_cb != nullptr) ? https_user_cb : default_https_user_cb;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    conf_ = HTTPD_SSL_CONFIG_DEFAULT();
#pragma GCC diagnostic pop

    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[]  asm("_binary_servercert_pem_end");
    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");

    // 经典单向 TLS：只给服务器证书 + 私钥，别设 cacert（那是双向 TLS 用的）
    conf_.servercert = servercert_start;
    conf_.servercert_len = servercert_end - servercert_start;
    conf_.prvtkey_pem = prvtkey_pem_start;
    conf_.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    conf_.user_cb = user_cb;

    esp_err_t ret = httpd_ssl_start(&server_, &conf_);
    if (ESP_OK != ret)
    {
        ESP_LOGI(TAG, "Error starting server!");
    }

    // 事件回调，上下文传 this
    esp_event_handler_register(ESP_HTTPS_SERVER_EVENT, ESP_EVENT_ANY_ID, event_cb, this);

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server_, &root);
}

void https_server::add_uri(httpd_uri_t &uri)
{
    httpd_register_uri_handler(server_, &uri);
}
