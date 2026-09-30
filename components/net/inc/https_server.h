#pragma once

#include <string>
#include <cstring>

#include <esp_event.h>
#include <esp_https_server.h>

using https_cb_t = void(*)(void*, esp_event_base_t, int32_t, void *);

using https_user_cb_t = void(*)(esp_https_server_user_cb_arg_t *);

class https_server
{
public:
    void init(httpd_uri_t &root, https_cb_t cb, https_user_cb_t https_user_cb);
private:
    static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void https_server_user_callback(esp_https_server_user_cb_arg_t *user_cb);
    httpd_handle_t server_;
    httpd_ssl_config conf_;
    https_cb_t cb_;
    httpd_uri_t root_;
    https_user_cb_t https_user_cb_;
};