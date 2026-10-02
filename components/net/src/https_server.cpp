#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <esp_tls.h>
#include <esp_key_config.h>
#include <esp_log.h>

#include "https_server.h"

static const constexpr char *TAG = "https_server";

void https_server::event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTPS_SERVER_EVENT) {
        if (event_id == HTTPS_SERVER_EVENT_ERROR) {
            esp_tls_last_error_t *last_error = (esp_tls_last_error_t *) event_data;
            ESP_LOGE(TAG, "Error event triggered: last_error = %s, last_tls_err = %d, tls_flag = %d", esp_err_to_name(last_error->last_error), last_error->esp_tls_error_code, last_error->esp_tls_flags);
        }
        else if(event_id == HTTPS_SERVER_EVENT_START)
        {
        }
        else if(event_id == HTTPS_SERVER_EVENT_ON_CONNECTED)
        {
        }
        else if(event_id == HTTPS_SERVER_EVENT_ON_DATA)
        {
        }
        else if(event_id == HTTPS_SERVER_EVENT_SENT_DATA)
        {
        }
        else if(event_id == HTTPS_SERVER_EVENT_DISCONNECTED)
        {
        }
        else if(event_id == HTTPS_SERVER_EVENT_STOP)
        {
        }
    }
}

void https_server::https_server_user_callback(esp_https_server_user_cb_arg_t *user_cb)
{
    ESP_LOGI(TAG, "User callback invoked!");
    int sockfd = -1;
    esp_err_t esp_ret = ESP_OK;
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
    mbedtls_ssl_context *ssl_ctx = NULL;
#endif
    switch(user_cb->user_cb_state) {
        case HTTPD_SSL_USER_CB_SESS_CREATE:
            ESP_LOGD(TAG, "At session creation");

            esp_ret = esp_tls_get_conn_sockfd(user_cb->tls, &sockfd);
            if (esp_ret != ESP_OK) {
                ESP_LOGE(TAG, "Error in obtaining the sockfd from tls context");
                break;
            }
            ESP_LOGI(TAG, "Socket FD: %d", sockfd);
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
            ssl_ctx = (mbedtls_ssl_context *) esp_tls_get_ssl_context(user_cb->tls);
            if (ssl_ctx == NULL) {
                ESP_LOGE(TAG, "Error in obtaining ssl context");
                break;
            }
            ESP_LOGI(TAG, "Current Ciphersuite: %s", mbedtls_ssl_get_ciphersuite(ssl_ctx));
#endif
            break;

        case HTTPD_SSL_USER_CB_SESS_CLOSE:
            ESP_LOGD(TAG, "At session close");
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
            ssl_ctx = (mbedtls_ssl_context *) esp_tls_get_ssl_context(user_cb->tls);
            if (ssl_ctx == NULL) {
                ESP_LOGE(TAG, "Error in obtaining ssl context");
                break;
            }
#endif
            break;

        case HTTPD_SSL_USER_CB_SESS_ERROR:
            ESP_LOGD(TAG, "At session error (handshake failed)");
            esp_ret = esp_tls_get_conn_sockfd(user_cb->tls, &sockfd);
            if (esp_ret == ESP_OK && sockfd >= 0) {
                struct sockaddr_storage addr;
                socklen_t len = sizeof(addr);
                if (getpeername(sockfd, (struct sockaddr*)&addr, &len) == 0) {
                    char ip_str[INET6_ADDRSTRLEN];
                    if (addr.ss_family == AF_INET) {
                        inet_ntop(AF_INET, &((struct sockaddr_in*)&addr)->sin_addr, ip_str, sizeof(ip_str));
                    } else if (addr.ss_family == AF_INET6) {
                        inet_ntop(AF_INET6, &((struct sockaddr_in6*)&addr)->sin6_addr, ip_str, sizeof(ip_str));
                    } else {
                        strcpy(ip_str, "unknown");
                    }
                    ESP_LOGW(TAG, "SSL handshake failed from client IP: %s", ip_str);
                }
            }
            break;

        default:
            ESP_LOGE(TAG, "Illegal state!");
            return;
    }
}

void https_server::init(httpd_uri_t &root, https_cb_t cb, https_user_cb_t https_user_cb)
{
    root_ = root;
    cb_ = event_handler;
    https_user_cb_ = https_server_user_callback;
    if(cb != nullptr)
    {
        cb_ = cb;
    }
    if(https_user_cb != nullptr)
    {
        https_user_cb_ = https_user_cb;
    }

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

    // 不写死密码套件、不强制 TLS 版本，用 mbedtls 默认（含 GCM/ChaCha20）

    conf_.user_cb = https_user_cb_;

    esp_err_t ret = httpd_ssl_start(&server_, &conf_);
    if (ESP_OK != ret) {
        ESP_LOGI(TAG, "Error starting server!");
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server_, &root);
}

void https_server::add_uri(httpd_uri_t &uri)
{
    httpd_register_uri_handler(server_, &uri);
}
