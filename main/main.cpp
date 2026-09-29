#include <string>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include "wifi.h"
#include "mqtt.h"
#include "mqtt_custom.h"

static const std::string ssid = "lilinng";
static const std::string password = "WYX103729";

extern "C" void app_main(void)
{
    auto ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // wifi
    wifi_sta wifi;
    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    memcpy(wifi_cfg.sta.ssid, ssid.c_str(), ssid.size());
    memcpy(wifi_cfg.sta.password, password.c_str(), password.size());
    wifi.easy_start(wifi_cfg);

    vTaskDelay(pdMS_TO_TICKS(5000));

    // mqtt
    esp_mqtt_client_config_t mqtt_cfg;
    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    mqtt_cfg.broker.address.uri = "mqtt://47.113.149.47:1883";
    mqtt_cfg.credentials.client_id = "esp32-node";  // 自身id
    mqtt_cfg.credentials.username = "esp32-1";
    mqtt_cfg.credentials.authentication.password = "esp32-1";
    mqtt client;
    mqtt_app_start(client);                    // 建队列/任务 + 注册消息回调（app 层）
    client.easy_start(mqtt_cfg);
    client.subscribe("home/room1/device1/cmd", 1);   // 订阅下行主题（进集合，连上后自动补订）

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
