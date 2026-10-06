#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>

#include "wifi.h"

static constexpr const char *TAG_STA = "wifi_sta";
static constexpr const char *TAG_AP  = "wifi_ap";

// ---------- wifi_sta：默认最小回调（自由函数，ctx = wifi_sta*）----------
static void default_sta_event(void *ctx, esp_event_base_t base, int32_t id, void *data)
{
    wifi_sta *self = static_cast<wifi_sta *>(ctx);
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        self->connect_now();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        self->handle_disconnected();
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        self->handle_got_ip(data);
}

void wifi_sta::connect_now(void)
{
    ESP_LOGI(TAG_STA, "try to connect...");
    ESP_ERROR_CHECK(esp_wifi_connect());
    overtime_count_ = 0;
}

void wifi_sta::handle_disconnected(void)
{
    ESP_LOGI(TAG_STA, "disconnected, try to reconnect...");
    if (++overtime_count_ < 6)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else
    {
        ESP_LOGW(TAG_STA, "reconnect overtime");
        overtime_count_ = 0;
    }
}

void wifi_sta::handle_got_ip(void *event_data)
{
    auto *info = static_cast<ip_event_got_ip_t *>(event_data);
    ESP_LOGI(TAG_STA, "IP_EVENT_STA_GOT_IP: " IPSTR, IP2STR(&info->ip_info.ip));
}

void wifi_sta::easy_start(wifi_config_t wifi_cfg)
{
    init(wifi_cfg, nullptr);
    start();
}

void wifi_sta::init(wifi_config_t wifi_cfg, sta_cb_t cb)
{
    wifi_cfg_ = wifi_cfg;

    sta_cb_t handler = (cb != nullptr) ? cb : default_sta_event;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 注册事件，上下文传 this
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START,        handler, this, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,   IP_EVENT_STA_GOT_IP,         handler, this, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, handler, this, NULL));

    (void)esp_netif_create_default_wifi_sta();

    if (!is_change)
    {
        cfg_ = WIFI_INIT_CONFIG_DEFAULT();
    }

    ESP_ERROR_CHECK(esp_wifi_init(&cfg_));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg_));
}

void wifi_sta::start(void)
{
    ESP_ERROR_CHECK(esp_wifi_start());
}

// ---------- wifi_ap：默认最小回调（自由函数，ctx = wifi_ap*）----------
static void default_ap_event(void *ctx, esp_event_base_t base, int32_t id, void *data)
{
    (void)data;
    wifi_ap *self = static_cast<wifi_ap *>(ctx);
    if (base == IP_EVENT && id == IP_EVENT_AP_STAIPASSIGNED)
        self->on_sta_connected();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED)
        self->on_sta_disconnected();
}

void wifi_ap::on_sta_connected(void)
{
    ESP_LOGI(TAG_AP, "sta connected");
}

void wifi_ap::on_sta_disconnected(void)
{
    ESP_LOGI(TAG_AP, "sta disconnected");
}

void wifi_ap::easy_start(wifi_config_t &wifi_cfg)
{
    init(wifi_cfg, nullptr);
    start();
}

void wifi_ap::init(wifi_config_t &wifi_cfg, ap_cb_t cb)
{
    wifi_cfg_ = wifi_cfg;

    ap_cb_t handler = (cb != nullptr) ? cb : default_ap_event;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 注册事件，上下文传 this
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,   IP_EVENT_AP_STAIPASSIGNED,      handler, this, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, handler, this, NULL));

    (void)esp_netif_create_default_wifi_ap();

    if (!is_change)
    {
        cfg_ = WIFI_INIT_CONFIG_DEFAULT();
    }

    ESP_ERROR_CHECK(esp_wifi_init(&cfg_));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg_));
}

void wifi_ap::start(void)
{
    ESP_ERROR_CHECK(esp_wifi_start());
}
