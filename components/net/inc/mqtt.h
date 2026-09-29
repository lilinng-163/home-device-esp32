#pragma once
#include <string>
#include <cstdint>
#include <set>

#include <esp_log.h>
#include <esp_event.h>
#include <mqtt_client.h>

using mqtt_cb_t  = void (*)(void*, esp_event_base_t, int32_t, void*);
using msg_cb_t   = void (*)(void *user, const char *topic, const char *payload);   // 消息回调

class mqtt
{
public:
    void easy_start(esp_mqtt_client_config_t &cfg);
    void init(esp_mqtt_client_config_t &cfg, mqtt_cb_t cb);
    void start(void);
    void subscribe(const std::string &topic, int qos);
    void unsubscribe(const std::string &topic);
    void publish(const std::string &topic, const std::string &payload, int qos);

    // 上层注册“收到消息怎么办”，mqtt 只管把消息转发出来
    void set_msg_cb(msg_cb_t cb, void *user)
    {
        msg_cb_ = cb;
        msg_user_ = user;
    }

    esp_mqtt_client_handle_t &get_handle(void) {return handle_;};
private:
    static void event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    void resubscribe_all(void);         // 连上后把集合里的主题全部补订

    esp_mqtt_client_config_t cfg_;
    mqtt_cb_t cb_;
    esp_mqtt_client_handle_t handle_ = nullptr;
    std::set<std::string> topics_;      // 期望订阅的主题集合（唯一）
    bool connected_ = false;
    msg_cb_t msg_cb_ = nullptr;         // 消息回调
    void    *msg_user_ = nullptr;
};
