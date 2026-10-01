#include <string>
#include <cstring>
#include <cstdio>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <vector>
#include <cJSON.h>

#include "key.h"
#include "wifi.h"
#include "mqtt.h"
#include "mqtt_custom.h"
#include "https_server.h"

static const char *TAG = "app";

static bool enter_ap = false;

// AP 模式的默认热点（与 NVS 里的 STA 配置无关）
static const std::string ap_ssid = "esp32";
static const std::string ap_pass = "esp32-lilinng";

// 主题：存字符串（不要存 cJSON*，那棵树会被 cJSON_Delete 释放）
static std::vector<std::string> publish_set;
static std::vector<std::string> subscribe_set;

// objcopy 生成的符号
extern const unsigned char _binary_index_html_start[];
extern const unsigned char _binary_index_html_end[];

// "/" 同时处理 GET（发页面）与 POST（收配置 JSON）
static esp_err_t config_page_handler(httpd_req_t *req)
{
    if (req->method == HTTP_POST)
    {
        char buf[2048];
        int total = req->content_len;
        if (total >= (int)sizeof(buf)) total = sizeof(buf) - 1;

        int got = 0;
        while (got < total)
        {
            int r = httpd_req_recv(req, buf + got, total - got);
            if (r <= 0)                     // 出错/超时
            {
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
            got += r;
        }
        buf[got] = '\0';

        nvs_handle_t h;
        if (nvs_open("cfg", NVS_READWRITE, &h) == ESP_OK)
        {
            nvs_set_str(h, "json", buf);
            nvs_commit(h);
            nvs_close(h);
        }

        ESP_LOGI(TAG, "config: %s", buf);    // 看现象：把配置打印出来

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // GET → 发 objcopy 嵌进来的 index.html
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)_binary_index_html_start,
                    _binary_index_html_end - _binary_index_html_start);
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

    // 开机按键判断是否 ap：延时 2 秒后再判一次
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

    if(enter_ap)    // 配置模式：起热点 + https 配网页
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

        static httpd_uri_t root = {};
        root.uri = "/";
        root.method = (httpd_method_t)HTTP_ANY;
        root.handler = config_page_handler;

        static https_server srv;
        srv.init(root, nullptr, nullptr);
        ESP_LOGI(TAG, "https config server on https://192.168.4.1/");
    }
    else    // normal entry：从 NVS 读配置，连 STA + MQTT
    {
        static wifi_sta wifi;
        wifi_config_t wifi_cfg;
        memset(&wifi_cfg, 0, sizeof(wifi_cfg));

        static mqtt client;
        esp_mqtt_client_config_t mqtt_cfg;
        memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));

        // 这些 std::string 要活到 client.easy_start()，所以放在本作用域
        std::string s_uri, s_cid, s_user, s_pass;

        nvs_handle_t h;
        if(nvs_open("cfg", NVS_READONLY, &h) == ESP_OK)
        {
            size_t len = 0;
            if(nvs_get_str(h, "json", nullptr, &len) == ESP_OK)
            {
                char *json = new char[len];
                nvs_get_str(h, "json", json, &len);

                cJSON *root = cJSON_Parse(json);
                if(root)
                {
                    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
                    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
                    cJSON *publish = cJSON_GetObjectItem(root, "publish");
                    cJSON *subscribe = cJSON_GetObjectItem(root, "subscribe");

                    if(cJSON_IsObject(wifi))
                    {
                        cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
                        cJSON *pass = cJSON_GetObjectItem(wifi, "pass");
                        if(cJSON_IsString(ssid))
                            snprintf((char*)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid),
                                     "%s", ssid->valuestring);
                        if(cJSON_IsString(pass))
                            snprintf((char*)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password),
                                     "%s", pass->valuestring);
                    }

                    if(cJSON_IsObject(mqtt))
                    {
                        cJSON *host = cJSON_GetObjectItem(mqtt, "host");
                        cJSON *port = cJSON_GetObjectItem(mqtt, "port");
                        cJSON *cid  = cJSON_GetObjectItem(mqtt, "client_id");
                        cJSON *user = cJSON_GetObjectItem(mqtt, "username");
                        cJSON *pw   = cJSON_GetObjectItem(mqtt, "password");

                        if(cJSON_IsString(cid))  s_cid  = cid->valuestring;
                        if(cJSON_IsString(user)) s_user = user->valuestring;
                        if(cJSON_IsString(pw))   s_pass = pw->valuestring;
                        if(cJSON_IsString(host))
                        {
                            int p = cJSON_IsNumber(port) ? port->valueint : 1883;
                            s_uri = "mqtt://" + std::string(host->valuestring)
                                    + ":" + std::to_string(p);
                        }
                    }

                    if(cJSON_IsArray(publish))
                    {
                        int n = cJSON_GetArraySize(publish);
                        for(int i = 0; i < n; i++)
                        {
                            cJSON *e = cJSON_GetArrayItem(publish, i);
                            if(cJSON_IsString(e)) publish_set.push_back(e->valuestring);
                        }
                    }
                    if(cJSON_IsArray(subscribe))
                    {
                        int n = cJSON_GetArraySize(subscribe);
                        for(int i = 0; i < n; i++)
                        {
                            cJSON *e = cJSON_GetArrayItem(subscribe, i);
                            if(cJSON_IsString(e))
                            {
                                subscribe_set.push_back(e->valuestring);
                                client.subscribe(e->valuestring, 1);   // subscribe 会拷贝主题
                            }
                        }
                    }

                    cJSON_Delete(root);   // 值都已拷进 wifi_cfg / s_* / vector
                }
                delete[] json;
            }
            nvs_close(h);
        }

        // 此时 s_* 仍存活，交给 mqtt_cfg
        if(!s_uri.empty())  mqtt_cfg.broker.address.uri = s_uri.c_str();
        if(!s_cid.empty())  mqtt_cfg.credentials.client_id = s_cid.c_str();
        if(!s_user.empty()) mqtt_cfg.credentials.username = s_user.c_str();
        if(!s_pass.empty()) mqtt_cfg.credentials.authentication.password = s_pass.c_str();

        wifi.easy_start(wifi_cfg);

        vTaskDelay(pdMS_TO_TICKS(5000));

        mqtt_app_start(client);      // 建队列/任务 + 注册消息回调（app 层）
        client.easy_start(mqtt_cfg); // s_* / mqtt_cfg 都还活着
    }

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
