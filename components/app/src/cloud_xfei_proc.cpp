#include <string>
#include <cstring>
#include <cstdio>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "cloud_xfei_proc.h"
#include "device_command.h"
#include "llm.h"
#include "wakeup_board.h"
#include "ui.h"

static const constexpr char *TAG = "cloud_xfei_proc";
static const constexpr size_t text_max = 1024;
static const constexpr UBaseType_t queue_len = 4;

static TaskHandle_t handle = nullptr;
static TaskHandle_t serial_handle = nullptr;
static QueueHandle_t queue_handle = nullptr;
static cloud_llm_config s_llm;
static bool s_started = false;

static const char *command_name(device_command cmd)
{
    switch (cmd)
    {
    case device_command::led_on:
        return "led on";
    case device_command::led_off:
        return "led off";
    default:
        return "none";
    }
}

// 固定长度文本入队：WS 回调和串口任务都只做拷贝，不在这里发网络请求
static void enqueue_text(const char *text, size_t len)
{
    if (queue_handle == nullptr || text == nullptr || len == 0)
    {
        return;
    }
    if (len >= text_max)
    {
        ESP_LOGW(TAG, "text too long: %u", (unsigned)len);
        return;
    }

    char item[text_max] = {};
    memcpy(item, text, len);
    item[len] = '\0';
    ui_set_voice(ui_voice_state::thinking, text);
    if (xQueueSend(queue_handle, item, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "queue full, drop text");
        ui_set_voice(ui_voice_state::error);
    }
}

// LLM 结果和串口注入都走这里：严格解析，解析失败绝不改 LED
static void handle_device_json(const char *json, size_t len, bool injected)
{
    device_command cmd = device_command::none;
    if (!device_command_parse(json, len, cmd))
    {
        ESP_LOGW(TAG, "%s json rejected", injected ? "injected" : "llm");
        ui_set_voice(ui_voice_state::error);
        return;
    }

    ESP_LOGI(TAG, "%s json -> %s", injected ? "injected" : "llm", command_name(cmd));
    device_command_execute(cmd);

    if (cmd == device_command::none)
    {
        ui_set_voice(ui_voice_state::ignored);
    }
    else
    {
        const char *expected = cmd == device_command::led_on ? "on" : "off";
        bool executed = strcmp(device_command_status(), expected) == 0;
        ui_set_voice(executed ? ui_voice_state::success : ui_voice_state::error);
    }
}

static void cloud_xfei_proc_task(void *parameters)
{
    (void)parameters;

    openai_llm llm;
    openai_llm::llm_req_options options;
    options.url = s_llm.url;
    options.model = s_llm.model;
    options.api_key = s_llm.api_key;
    llm.init(options);

    char text[text_max];
    while (1)
    {
        if (xQueueReceive(queue_handle, text, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        ESP_LOGI(TAG, "voice: %s", text);
        esp_err_t err = llm.request(text);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "llm request fail: %s", esp_err_to_name(err));
            ui_set_voice(ui_voice_state::error);
            continue;
        }

        std::string content;
        err = llm.get_content(content);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "llm content fail: %s", esp_err_to_name(err));
            ui_set_voice(ui_voice_state::error);
            continue;
        }

        handle_device_json(content.data(), content.size(), false);
    }
}

static void trim_line(char *line)
{
    size_t len = strlen(line);
    while (len > 0)
    {
        char c = line[len - 1];
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t')
        {
            break;
        }
        line[--len] = '\0';
    }
}

// 串口行入口：voice <文本> / json <JSON> / status
static void serial_task(void *parameters)
{
    (void)parameters;
    char line[1536];
    size_t used = 0;
    bool overflow = false;
    setvbuf(stdin, nullptr, _IONBF, 0);

    while (1)
    {
        int c = fgetc(stdin);
        if (c == EOF)
        {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // UART 可能分多批到达。保留半行，超长后丢弃到行尾，不能执行后半截。
        if (c != '\n' && c != '\r')
        {
            if (!overflow && used + 1 < sizeof(line) && c != 0)
            {
                line[used++] = (char)c;
            }
            else
            {
                overflow = true;
            }
            continue;
        }
        line[used] = '\0';
        used = 0;
        if (overflow)
        {
            overflow = false;
            ESP_LOGW(TAG, "serial line too long");
            continue;
        }

        trim_line(line);
        char *p = line;
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }
        if (*p == '\0')
        {
            continue;
        }

        if (strcmp(p, "asr_probe") == 0)
        {
            ESP_LOGI(TAG, "asr_probe requested");
            wakeup_asr_probe();
            continue;
        }

        if (strcmp(p, "asr_tx_probe") == 0)
        {
            ESP_LOGI(TAG, "asr_tx_probe requested (no device commands)");
            wakeup_asr_probe(true);
            continue;
        }

        if (strcmp(p, "status") == 0)
        {
            unsigned waiting = queue_handle ? (unsigned)uxQueueMessagesWaiting(queue_handle) : 0;
            ESP_LOGI(TAG,
                     "status: led=%s queue=%u/%u",
                     device_command_status(),
                     waiting,
                     (unsigned)queue_len);
            continue;
        }

        if (strncmp(p, "voice", 5) == 0 && (p[5] == ' ' || p[5] == '\t'))
        {
            char *text = p + 5;
            while (*text == ' ' || *text == '\t')
            {
                text++;
            }
            if (*text == '\0')
            {
                ESP_LOGW(TAG, "empty voice text");
                continue;
            }
            ESP_LOGI(TAG, "injected voice: %s", text);
            enqueue_text(text, strlen(text));
            continue;
        }

        const char *json = nullptr;
        if (strncmp(p, "json", 4) == 0)
        {
            json = p + 4;
            while (*json == ' ' || *json == '\t')
            {
                json++;
            }
        }
        else if (*p == '{')
        {
            json = p;
        }

        if (json != nullptr)
        {
            handle_device_json(json, strlen(json), true);
            continue;
        }

        ESP_LOGW(TAG, "unknown serial command");
    }
}

void cloud_xfei_proc_start(xfei_dictation &xfei, const cloud_llm_config &llm)
{
    if (s_started)
    {
        return;
    }
    s_llm = llm;
    device_command_init();

    queue_handle = xQueueCreate(queue_len, text_max);
    if (queue_handle == nullptr)
    {
        ESP_LOGE(TAG, "queue create fail");
        return;
    }

    xfei.set_final_cb([](const std::string &text) { enqueue_text(text.c_str(), text.size()); });

    // AFE 启动后内部 RAM 较紧，沿用语音任务的 PSRAM 栈；TLS 仍有独立 12KB 栈。
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(cloud_xfei_proc_task,
                                                     "cloud_xfei_proc_task",
                                                     12 * 1024,
                                                     nullptr,
                                                     4,
                                                     &handle,
                                                     tskNO_AFFINITY,
                                                     MALLOC_CAP_SPIRAM);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "create llm task fail");
        xfei.set_final_cb(nullptr);
        vQueueDelete(queue_handle);
        queue_handle = nullptr;
        return;
    }

    s_started = true;

    ret = xTaskCreatePinnedToCoreWithCaps(serial_task,
                                          "voice_serial",
                                          4 * 1024,
                                          nullptr,
                                          3,
                                          &serial_handle,
                                          tskNO_AFFINITY,
                                          MALLOC_CAP_SPIRAM);
    if (ret != pdPASS)
    {
        ESP_LOGW(TAG, "create serial task fail");
    }
}
