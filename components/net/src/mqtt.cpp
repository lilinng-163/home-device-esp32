#include <esp_log.h>

#include "mqtt.h"

static constexpr const char *TAG = "mqtt";

// 默认的最小事件回调（自由函数，不是类成员）；ctx = mqtt*，内部只调公开接口
static void default_mqtt_event(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)base;
    mqtt *self = static_cast<mqtt *>(ctx);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        self->mark_connected();
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
    {
        self->mark_disconnected();
        break;
    }
    case MQTT_EVENT_DATA:
    {
        self->forward_message(event->topic, event->topic_len, event->data, event->data_len);
        break;
    }
    case MQTT_EVENT_SUBSCRIBED:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    }
    case MQTT_EVENT_UNSUBSCRIBED:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    }
    case MQTT_EVENT_PUBLISHED:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    }
    case MQTT_EVENT_ERROR:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    }
    default:
    {
        ESP_LOGI(TAG, "Other event id:%d", (int)event->event_id);
        break;
    }
    }
}

// ---- 公开接口（默认回调通过它们干活）----
void mqtt::mark_connected(void)
{
    connected_ = true;
    resubscribe_all();
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED, resub %d topics", (int)topics_.size());
}

void mqtt::mark_disconnected(void)
{
    connected_ = false;
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
}

void mqtt::forward_message(const char *topic, int tlen, const char *data, int dlen)
{
    if (!msg_cb_)
        return;

    std::string t(topic, tlen);
    std::string p(data, dlen);
    msg_cb_(msg_user_, t.c_str(), p.c_str());
}

// ---- 生命周期 ----
void mqtt::easy_start(esp_mqtt_client_config_t &cfg)
{
    init(cfg, nullptr);
    start();
}

void mqtt::init(esp_mqtt_client_config_t &cfg, mqtt_cb_t cb)
{
    cfg_ = cfg;
    handle_ = esp_mqtt_client_init(&cfg_);

    // 外部传了回调就用外部的，否则用默认的；上下文统一传 this
    mqtt_cb_t handler = (cb != nullptr) ? cb : default_mqtt_event;
    esp_mqtt_client_register_event(handle_, MQTT_EVENT_ANY, handler, this);
    ESP_LOGI(TAG, "esp_mqtt_client_register_event");
}

void mqtt::set_event_cb(mqtt_cb_t cb, void *ctx)
{
    // 外部想替换事件回调时调用；ctx 不传就用 this
    esp_mqtt_client_register_event(handle_, MQTT_EVENT_ANY, cb, ctx ? ctx : this);
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
    if (connected_)                      // 已连才真正发往 broker
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
