#include <string>
#include <cstring>
#include <cstdio>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_log.h>

#include "wifi.h"
#include "mqtt.h"
#include "mqtt_custom.h"
#include "nvs_config.h"
#include "web_config.h"
#include "board.h"
#include "key.h"
#include "lcd.h"
#include "lv_disp.h"
#include "ui.h"
#include "https_server.h"

static const char *TAG = "app";

// AP 模式的默认热点（与 NVS 里的 STA 配置无关）
static const std::string ap_ssid = "esp32";
static const std::string ap_pass = "esp32-lilinng";

extern "C" void app_main(void)
{
    auto ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ---- 硬件：板级一次性初始化（建总线 + 挂芯片/外设 + 接线）----
    board_init();

    // ---- LVGL：把板子上的 LCD 接上 ----
    lcd &screen = board_lcd();
    lvgl_port_init(screen);
    lvgl_task_start();

    // ---- 不管 AP / STA，开机先读一次 NVS ----
    app_config cfg;
    std::string cfg_raw;
    if(!nvs_config_load(cfg, &cfg_raw))
    {
        ESP_LOGW(TAG, "no config in nvs");
    }
    web_config_set_config(cfg_raw);   // 网页回显用这份内存，不再读 NVS

    // ---- 按键：按住 2 秒才算数 ----
    key &init_key = board_key();
    bool enter_ap = false;
    if(init_key.get_status())
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        enter_ap = init_key.get_status();
    }

    if(enter_ap)    // AP：热点 + https 配网页
    {
        static wifi_ap wifi;
        wifi_config_t wifi_cfg;
        memset(&wifi_cfg, 0, sizeof(wifi_cfg));
        memcpy(wifi_cfg.ap.ssid, ap_ssid.c_str(), ap_ssid.size());
        wifi_cfg.ap.ssid_len = strlen(ap_ssid.c_str());
        wifi_cfg.ap.channel = 1;
        memcpy(wifi_cfg.ap.password, ap_pass.c_str(), ap_pass.size());
        wifi_cfg.ap.max_connection = 5;
        wifi_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;

        wifi.easy_start(wifi_cfg);

        static https_server srv;
        srv.init(web_config_uri(), nullptr, nullptr);
        srv.add_uri(web_config_data_uri());     // GET /config → 当前配置回显
        ESP_LOGI(TAG, "https config server on https://192.168.4.1/");
    }
    else    // STA：显示配置到 LCD + 连 STA/MQTT
    {
        show_sta_paramters(cfg);

        static wifi_sta wifi;
        wifi_config_t wifi_cfg;
        memset(&wifi_cfg, 0, sizeof(wifi_cfg));
        snprintf((char*)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), "%s", cfg.ssid.c_str());
        snprintf((char*)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s", cfg.pass.c_str());

        static mqtt client;
        esp_mqtt_client_config_t mqtt_cfg;
        memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));

        std::string uri = "mqtt://" + cfg.host + ":" + std::to_string(cfg.port);
        mqtt_cfg.broker.address.uri = uri.c_str();
        if(!cfg.client_id.empty()) mqtt_cfg.credentials.client_id = cfg.client_id.c_str();
        if(!cfg.username.empty())  mqtt_cfg.credentials.username  = cfg.username.c_str();
        if(!cfg.password.empty())  mqtt_cfg.credentials.authentication.password = cfg.password.c_str();

        wifi.easy_start(wifi_cfg);

        vTaskDelay(pdMS_TO_TICKS(5000));

        for(auto &t : cfg.subscribe) client.subscribe(t.c_str(), 1);   // subscribe 会拷贝主题
        mqtt_app_start(client);      // 建队列/任务 + 注册消息回调（app 层）
        client.easy_start(mqtt_cfg);
    }

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
