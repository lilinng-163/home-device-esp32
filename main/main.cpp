#include <string>
#include <cstring>
#include <cstdio>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_log.h>

#include "key.h"
#include "wifi.h"
#include "mqtt.h"
#include "mqtt_custom.h"
#include "https_server.h"

static const char *TAG = "app";

static bool enter_ap = false;

static const std::string ssid = "esp32";
static const std::string password = "esp32-lilinng";

// 配网页面 + 保存（同一个 "/"，GET 带 ?ssid=..&pass=.. 就当提交）
static esp_err_t config_page_handler(httpd_req_t *req)
{
    if (httpd_req_get_url_query_len(req) > 0)
    {
        char query[256] = {};
        char new_ssid[64] = {};
        char new_pass[64] = {};
        httpd_req_get_url_query_str(req, query, sizeof(query));
        httpd_query_key_value(query, "ssid", new_ssid, sizeof(new_ssid));
        httpd_query_key_value(query, "pass", new_pass, sizeof(new_pass));

        ESP_LOGI(TAG, "got ssid=%s pass=%s", new_ssid, new_pass);

        // 把收到的值回显到网页上（看现象用）
        char page[256];
        snprintf(page, sizeof(page),
                 "<html><body><h3>recv</h3>ssid=%s<br>pass=%s</body></html>",
                 new_ssid, new_pass);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    const char *html =
        "<html><body>"
        "<h3>ESP32 配网</h3>"
        "<form method=\"get\" action=\"/\">"
        "WiFi: <input name=\"ssid\"><br><br>"
        "密码: <input name=\"pass\"><br><br>"
        "<button>保存</button>"
        "</form></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

extern "C" void app_main(void)
{
    auto ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 开机按键判断是否ap 延时2秒判断key状态
    gpio_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pin_bit_mask = (1ULL << GPIO_NUM_5);
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    key init_key;
    init_key.init(GPIO_NUM_5, cfg);
    if(init_key.get_status())
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if(init_key.get_status())
        {
            enter_ap = true;
        }
    }

    if(enter_ap)    // 配置模式
    {
        wifi_ap wifi;
        wifi_config_t wifi_cfg;
        memset(&wifi_cfg, 0, sizeof(wifi_cfg));
        memcpy(wifi_cfg.ap.ssid, ssid.c_str(), ssid.size());
        wifi_cfg.ap.ssid_len = strlen(ssid.c_str());
        wifi_cfg.ap.channel = 1;
        memcpy(wifi_cfg.ap.password, password.c_str(), password.size());
        wifi_cfg.ap.max_connection = 5;
        wifi_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;

        wifi.easy_start(wifi_cfg);

        // ★ 起 https 配网服务
        static httpd_uri_t root = {};
        root.uri = "/";
        root.method = HTTP_GET;
        root.handler = config_page_handler;

        https_server srv;
        srv.init(root, nullptr, nullptr);
        ESP_LOGI(TAG, "https config server on https://192.168.4.1/");
    }
    else    // normal entry
    {
        // wifi_sta
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
        client.subscribe("home/room1/device1/cmd", 1);   // 订阅下行主题
    }

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
