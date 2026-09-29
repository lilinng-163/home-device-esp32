#include <esp_log.h>

#include "mqtt.h"

static constexpr const char *TAG = "mqtt";

void mqtt::event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)base;
    mqtt *self = static_cast<mqtt *>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:  // 已连接
        self->connected_ = true;
        self->resubscribe_all();          // 用类自己的方法补回订阅
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED, resub %d topics", (int)self->topics_.size());
        break;
    case MQTT_EVENT_DISCONNECTED:   // 断连
        self->connected_ = false;
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
    case MQTT_EVENT_DATA:   // 下行数据：只把消息转发给上层
    {
        std::string topic(event->topic, event->topic_len);
        std::string payload(event->data, event->data_len);
        if (self->msg_cb_)
        {
            self->msg_cb_(self->msg_user_, topic.c_str(), payload.c_str());
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", (int)event->event_id);
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
    cb_ = (cb != nullptr) ? cb : mqtt::event_handler;
    handle_ = esp_mqtt_client_init(&cfg_);
    esp_mqtt_client_register_event(handle_, MQTT_EVENT_ANY, cb_, this);
    ESP_LOGI(TAG, "esp_mqtt_client_register_event");
}

void mqtt::start(void)
{
    esp_mqtt_client_start(handle_);
    ESP_LOGI(TAG, "mqtt start");
}

void mqtt::resubscribe_all(void)
{
    for (const auto &t : topics_)
    {
        esp_mqtt_client_subscribe_single(handle_, t.c_str(), 1);
    }
}

void mqtt::subscribe(const std::string &topic, int qos)
{
    if (topics_.count(topic))            // 已订过，跳过
    {
        return;
    }
    topics_.insert(topic);               // 先记进集合
    if (connected_)                      // 已连才真正发给 broker
    {
        auto ret = esp_mqtt_client_subscribe_single(handle_, topic.c_str(), qos);
        if (ret < 0)
        {
            ESP_LOGE(TAG, "subscribe fail: %s", topic.c_str());
        }
    }
}

void mqtt::unsubscribe(const std::string &topic)
{
    if (!topics_.count(topic))           // 没订过
    {
        return;
    }
    topics_.erase(topic);                // 从集合删
    if (connected_)                      // 已连才真正取消
    {
        auto ret = esp_mqtt_client_unsubscribe(handle_, topic.c_str());
        if (ret < 0)
        {
            ESP_LOGE(TAG, "unsubscribe fail: %s", topic.c_str());
        }
    }
}

void mqtt::publish(const std::string &topic, const std::string &payload, int qos)
{
    auto ret = esp_mqtt_client_publish(handle_, topic.c_str(), payload.c_str(), payload.size(), qos, 0);
    if (ret < 0)
    {
        ESP_LOGE(TAG, "publish fail");
    }
}
