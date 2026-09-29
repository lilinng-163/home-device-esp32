#pragma once
#include <print>
#include <string>
#include <cstdint>

#include <esp_event.h>
#include <esp_wifi.h>


using sta_cb_t = void(*)(void*, esp_event_base_t, int32_t, void*);

class wifi_sta
{
public:
    void easy_start(wifi_config_t wifi_cfg);
    void init(wifi_config_t wifi_cfg, sta_cb_t cb);
    void start(void);
    void set_init_cfg(wifi_init_config_t &cfg)
    {
        cfg_ = cfg;
        is_change = true;
    }

private:
    sta_cb_t cb_;
    wifi_init_config_t cfg_ = {};
    wifi_config_t wifi_cfg_ = {};
    bool is_change = false;
};