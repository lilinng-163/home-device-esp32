#include "mqtt_custom.h"

#include <string>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <cJSON.h>

static constexpr const char *TAG = "mqtt_app";

// 队列里传的消息（定长，才能进 FreeRTOS 队列）
struct mqtt_msg
{
    char topic[128];
    char payload[512];
};

static QueueHandle_t s_queue = nullptr;

// mqtt 收到消息 → 回调这里 → 只入队（轻量，不阻塞 mqtt 任务）
static void on_mqtt_msg(void *user, const char *topic, const char *payload)
{
    (void)user;
    mqtt_msg m = {};
    strncpy(m.topic,   topic,   sizeof(m.topic)   - 1);
    strncpy(m.payload, payload, sizeof(m.payload) - 1);
    xQueueSend(s_queue, &m, 0);
}

// 消费者任务：出队 → 解析 → 分发（重活在这，可 include 设备头）
static void mqtt_process_thread(void *param)
{
    (void)param;
    mqtt_msg m;
    while (1)
    {
        if (xQueueReceive(s_queue, &m, portMAX_DELAY) == pdTRUE)
        {
            if (std::string(m.topic) == "home/room1/device1/cmd")
            {
                cJSON *root = cJSON_Parse(m.payload);
                if (root)
                {
                    cJSON *led = cJSON_GetObjectItem(root, "led");
                    if (cJSON_IsNumber(led))
                    {
                        // led_set(led->valueint);   // ← 设备操作放这层（app 依赖 device）
                        ESP_LOGI(TAG, "led = %d", led->valueint);
                    }
                    cJSON_Delete(root);
                }
            }
        }
    }
}

void mqtt_app_start(mqtt &client)
{
    s_queue = xQueueCreate(10, sizeof(mqtt_msg));
    client.set_msg_cb(on_mqtt_msg, nullptr);          // 注册消息回调

    xTaskCreate(mqtt_process_thread, "mqtt_process", 8192, nullptr, 4, nullptr);
    ESP_LOGI(TAG, "mqtt app started");
}
