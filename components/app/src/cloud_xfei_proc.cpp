#include <string>
#include <cstring>

#include <esp_log.h>

#include "cloud_xfei_proc.h"

static const constexpr char *TAG = "cloud_xfei_proc";
static TaskHandle_t handle = NULL;

static const constexpr uint32_t size = 1024;
QueueHandle_t queue_handle = NULL;

static void cloud_xfei_proc_task(void *parameters)
{
    xfei_dictation *xfei = static_cast<xfei_dictation *>(parameters);

    static uint8_t buf[size];
    memset(buf, 0, size);
    xfei->set_on_data_cb([](void *event_data)
    {
        xQueueSend(queue_handle, event_data, 0);
    });
    while(1)
    {
        if(xQueueReceive(queue_handle, buf, portMAX_DELAY) == pdTRUE)
        {

        }
    }
}

void cloud_xfei_proc_start(xfei_dictation &xfei)
{
    queue_handle = xQueueCreate(20, size);
    if(!queue_handle)
    {
        ESP_LOGI(TAG, "queue create fail");
        return;
    }
    const constexpr uint32_t stack_size = 6144;
    const constexpr uint32_t priority = 4;
    auto ret = xTaskCreate(static_cast<TaskFunction_t>(cloud_xfei_proc_task),
                "cloud_xfei_proc_task", stack_size, 
                reinterpret_cast<void *>(&xfei), priority, &handle);
    if(ret != pdPASS)
    {
        ESP_LOGI(TAG, "create task fail");
        return;
    }
}