#pragma once
#include <string>
#include <cstdint>

#include <esp_event.h>
#include <esp_wifi.h>

using sta_cb_t = void (*)(void *ctx, esp_event_base_t, int32_t, void *);

class wifi_sta
{
public:
    void easy_start(wifi_config_t wifi_cfg);
    void init(wifi_config_t wifi_cfg, sta_cb_t cb = nullptr);
    void start(void);
    void set_init_cfg(wifi_init_config_t &cfg)
    {
        cfg_ = cfg;
        is_change = true;
    }

    // ---- 供（默认/外部）回调使用的公开接口 ----
    void connect_now(void);          // 连/重连（重置计数）
    void handle_disconnected(void);  // 断连：计数 + 重连
    void handle_got_ip(void *event_data);

private:
    wifi_init_config_t cfg_ = {};
    wifi_config_t wifi_cfg_ = {};
    bool is_change = false;
    uint8_t overtime_count_ = 0;
};

using ap_cb_t = void (*)(void *ctx, esp_event_base_t, int32_t, void *);

class wifi_ap
{
public:
    void easy_start(wifi_config_t &wifi_cfg);
    void init(wifi_config_t &wifi_cfg, ap_cb_t cb = nullptr);
    void start(void);
    void set_init_cfg(wifi_init_config_t &cfg)
    {
        cfg_ = cfg;
        is_change = true;
    }

    // ---- 供（默认/外部）回调使用的公开接口 ----
    void on_sta_connected(void);
    void on_sta_disconnected(void);

private:
    wifi_init_config_t cfg_ = {};
    wifi_config_t wifi_cfg_ = {};
    bool is_change = false;
};
