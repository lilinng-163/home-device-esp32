#pragma once
#include <string>
#include <cstdint>
#include <vector>

#include <esp_log.h>
#include <esp_event.h>
#include <mqtt_client.h>

using mqtt_cb_t = void (*)(void*, esp_event_base_t, int32_t, void*);

class mqtt
{
public:
    void easy_start(esp_mqtt_client_config_t &cfg);
    void init(esp_mqtt_client_config_t &cfg, mqtt_cb_t cb);
    void start(void);
    void subscribe(std::vector<std::string> &topic, int qos);
    void publish(std::string &topic, std::string &payload, int qos);
    esp_mqtt_client_handle_t &get_handle(void) {return handle_;};
private:
    esp_mqtt_client_config_t cfg_;
    mqtt_cb_t cb_;
    esp_mqtt_client_handle_t handle_;
};