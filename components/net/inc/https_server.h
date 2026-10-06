#pragma once

#include <string>
#include <cstring>

#include <esp_event.h>
#include <esp_https_server.h>

using https_cb_t      = void (*)(void *ctx, esp_event_base_t, int32_t, void *);
using https_user_cb_t = void (*)(esp_https_server_user_cb_arg_t *);

class https_server
{
public:
    // cb / https_user_cb 传 nullptr 就用默认的最小回调
    void init(httpd_uri_t &root, https_cb_t cb = nullptr, https_user_cb_t https_user_cb = nullptr);
    void add_uri(httpd_uri_t &uri);

private:
    httpd_handle_t server_ = nullptr;
    httpd_ssl_config conf_ = {};
    httpd_uri_t root_ = {};
};
