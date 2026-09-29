#include <esp_log.h>

#include "mqtt.h"

static constexpr const char *TAG = "mqtt";

std::vector<std::string> v1 =  {"home/device/cmd"};

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    mqtt *self = static_cast<mqtt *>(handler_args);
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:  //已连接
        //连接成功后订阅主题
        self->subscribe(v1, 1);
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:   //断连
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:   //下行数据
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt::easy_start(esp_mqtt_client_config_t &cfg)
{
    init(cfg, nullptr);
    start();
}
void mqtt::init(esp_mqtt_client_config_t &cfg, mqtt_cb_t cb)
{
    cfg_ = cfg;
    cb_ = mqtt_event_handler;
    if(cb != nullptr)
    {
        cb_ = cb;
    }
    handle_ = esp_mqtt_client_init(&cfg_);
    esp_mqtt_client_register_event(handle_, MQTT_EVENT_ANY, cb_, this);
    ESP_LOGI(TAG, "esp_mqtt_client_register_event");
}
void mqtt::start(void)
{
    esp_mqtt_client_start(handle_);
    ESP_LOGI(TAG, "mqtt start");
}

void mqtt::subscribe(std::vector<std::string> &topic, int qos)
{
    for(auto val : topic)
    {
        auto ret = esp_mqtt_client_subscribe_single(handle_, val.c_str(), qos);
        if(ret < 0)
        {
            ESP_LOGI(TAG, "subscribe fail: %s", val.c_str());
        }
    }
}

void mqtt::publish(std::string &topic, std::string &payload, int qos)
{
    auto ret = esp_mqtt_client_publish(handle_, topic.c_str(), payload.c_str(), payload.size(), qos, 0);
    if(ret < 0)
    {
        ESP_LOGI(TAG, "publish fail");
    }
}