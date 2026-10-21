#include "mqtt_custom.h"
#include "device_message.h"

#include <string>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cJSON.h>

static constexpr const char *TAG = "mqtt_app";

// 队列里传的消息（定长，才能进 FreeRTOS 队列）
struct mqtt_msg
{
    char topic[128];
    char payload[512];
};

static QueueHandle_t s_queue = nullptr;
static std::string s_device_id;
static std::string s_command_topic;

// mqtt 收到消息 → 回调这里 → 只入队（轻量，不阻塞 mqtt 任务）
static void on_mqtt_msg(void *user, const char *topic, const char *payload)
{
    (void)user;
    // 不截断 JSON；超过当前设备队列容量时直接拒绝。
    if (topic == nullptr || payload == nullptr || strlen(topic) >= sizeof(mqtt_msg::topic) ||
        strlen(payload) >= sizeof(mqtt_msg::payload))
    {
        ESP_LOGW(TAG, "message too large or empty pointer");
        return;
    }

    mqtt_msg m = {};
    strncpy(m.topic,   topic,   sizeof(m.topic)   - 1);
    strncpy(m.payload, payload, sizeof(m.payload) - 1);
    if (xQueueSend(s_queue, &m, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "message queue full");
    }
}

// 消费者任务：出队 → 解析 → 分发（重活在这，可 include 设备头）
static void mqtt_process_thread(void *param)
{
    mqtt *client = reinterpret_cast<mqtt *>(param);
    mqtt_msg m;
    while (1)
    {
        if (xQueueReceive(s_queue, &m, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "queue recv");
            if (std::string(m.topic) == s_command_topic)
            {
                cJSON *root = cJSON_ParseWithOpts(m.payload, nullptr, true);
                device_message message;
                if (!read_device_message(root, message))
                {
                    ESP_LOGW(TAG, "invalid device message");
                }
                else if (message.device_id != s_device_id)
                {
                    ESP_LOGW(TAG, "device_id mismatch");
                }
                else if (client->handle_json_ && client->handle_json_(message.data) != 0)
                {
                    ESP_LOGW(TAG, "unsupported or invalid command data");
                }

                // JSON 树始终由消费者释放，业务只借用 data。
                cJSON_Delete(root);
            }
        }
    }
}

void mqtt_app_start(mqtt &client, int device_id)
{
    if (device_id <= 0 || s_queue != nullptr)
    {
        ESP_LOGE(TAG, "invalid device_id or app already started");
        return;
    }

    s_device_id = std::to_string(device_id);
    s_command_topic = "home/room1/device" + s_device_id + "/cmd";
    s_queue = xQueueCreate(10, sizeof(mqtt_msg));
    if (s_queue == nullptr)
    {
        ESP_LOGE(TAG, "queue create fail");
        return;
    }

    // 应用层只消费队列并解析 JSON，栈放 PSRAM，给 WS/TLS 留出内部连续内存。
    // MQTT 库自己的网络任务不受影响，仍沿用组件的内存配置。
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(mqtt_process_thread,
                                                   "mqtt_process",
                                                   8192,
                                                   &client,
                                                   4,
                                                   nullptr,
                                                   tskNO_AFFINITY,
                                                   MALLOC_CAP_SPIRAM);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "process task create fail");
        vQueueDelete(s_queue);
        s_queue = nullptr;
        return;
    }

    client.set_msg_cb(on_mqtt_msg, nullptr);
    client.subscribe(s_command_topic, 1);
    ESP_LOGI(TAG, "mqtt app started");
}
