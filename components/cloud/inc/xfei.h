#pragma once
#include <string>
#include <vector>

#include <esp_websocket_client.h>
#include <esp_event.h>

using xfei_dictation_cb_t = void (*)(void *ctx, esp_event_base_t, int32_t, void *);
using xfei_dictation_on_data_cb_t = void (*)(void *);

class xfei_dictation
{
public:
    int easy_start(std::string &apikey, std::string &apisecret);
    void init(std::string &apikey, std::string &apisecret, xfei_dictation_cb_t cb = nullptr);
    void set_event_cb(xfei_dictation_cb_t cb, void *ctx = nullptr);   // 外部替换
    int start(void);
    void request(const uint8_t *pcm, int len, int status);   // 发一帧：0=首 1=中 2=末
    void set_appid(const std::string &appid) { appid_ = appid; }
    bool is_connected(void) { return esp_websocket_client_is_connected(handle_); }
    esp_websocket_client_handle_t &get_instance(void) { return handle_; }

    void set_on_data_cb(xfei_dictation_on_data_cb_t cb);
    void on_connected(void);
    void on_disconnected(void);
    void on_data(void *event_data);
    void on_error(void);
    void on_finish(void);
    ~xfei_dictation(void);

private:
    // 返回 authorization（base64），并把本次的 date 存进 date_
    std::string compute_authorization(std::string &apikey, std::string &apisecret);

    esp_websocket_client_config_t cfg_ = {};
    esp_websocket_client_handle_t handle_ = nullptr;
    std::string uri_;                 // cfg_.uri 指向它，必须常驻
    std::string date_;                // 签名与 URL 共用的 date
    std::string appid_;               // 讯飞 APPID（common.app_id）
    std::string text_;                // 累积识别文本（拼 ws[].cw[].w）
    xfei_dictation_on_data_cb_t on_data_cb_ = nullptr;
};
