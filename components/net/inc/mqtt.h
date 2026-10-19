#pragma once
#include <string>
#include <cstdint>
#include <set>
#include <atomic>

#include <esp_log.h>
#include <esp_event.h>
#include <mqtt_client.h>

using mqtt_cb_t = void (*)(void *ctx, esp_event_base_t, int32_t, void *);
using msg_cb_t = void (*)(void *user, const char *topic, const char *payload); // 消息回调

class mqtt
{
  public:
    void easy_start(esp_mqtt_client_config_t &cfg);
    void init(esp_mqtt_client_config_t &cfg, mqtt_cb_t cb = nullptr);
    void start(void);
    void subscribe(const std::string &topic, int qos);
    void unsubscribe(const std::string &topic);
    void publish(const std::string &topic, const std::string &payload, int qos);

    // 外部想换事件回调就传指针（不传 ctx 则用 this 当上下文）
    void set_event_cb(mqtt_cb_t cb, void *ctx = nullptr);

    // 上层注册“收到消息怎么办”，mqtt 只管把消息转发出去
    void set_msg_cb(msg_cb_t cb, void *user)
    {
        msg_cb_ = cb;
        msg_user_ = user;
    }

    esp_mqtt_client_handle_t &get_handle(void)
    {
        return handle_;
    }

    // ---- 供（默认/外部）回调使用的公开接口 ----
    void mark_connected(void);    // 连上：置位 + 补订阅
    void mark_disconnected(void); // 断连：清位
    bool is_connected(void) const
    {
        return connected_;
    }
    void forward_message(const char *topic, int tlen, const char *data, int dlen);

  private:
    void resubscribe_all(void); // 连上后把集合里的主题全部补订

    esp_mqtt_client_config_t cfg_;
    esp_mqtt_client_handle_t handle_ = nullptr;
    std::set<std::string> topics_; // 期望订阅的主题集合（唯一）
    // MQTT 事件任务写入，主任务读取后同步到界面。
    std::atomic<bool> connected_{false};
    msg_cb_t msg_cb_ = nullptr; // 消息回调
    void *msg_user_ = nullptr;
};
