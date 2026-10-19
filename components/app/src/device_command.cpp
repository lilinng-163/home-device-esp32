#include <cstring>
#include <cstddef>
#include <string>

#include <cJSON.h>

#include "device_command.h"

bool device_command_parse(const char *json, size_t len, device_command &cmd)
{
    cmd = device_command::none;
    if (json == nullptr || len == 0)
    {
        return false;
    }

    // cJSON 的字符串接口以 NUL 结尾；拒绝嵌入 NUL，避免只校验到值的前半段。
    if (memchr(json, '\0', len) != nullptr ||
        std::string(json, len).find("\\u0000") != std::string::npos)
    {
        return false;
    }

    const char *end = nullptr;
    cJSON *root = cJSON_ParseWithLengthOpts(json, len, &end, false);
    if (root == nullptr)
    {
        return false;
    }

    // 只允许尾部空白，拒绝尾部垃圾
    while (end < json + len && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
    {
        end++;
    }
    if (end != json + len)
    {
        cJSON_Delete(root);
        return false;
    }

    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return false;
    }

    cJSON *device = nullptr;
    cJSON *action = nullptr;
    int fields = 0;
    for (cJSON *item = root->child; item != nullptr; item = item->next)
    {
        if (item->string == nullptr)
        {
            cJSON_Delete(root);
            return false;
        }
        if (strcmp(item->string, "device") == 0 && device == nullptr && cJSON_IsString(item))
        {
            device = item;
        }
        else if (strcmp(item->string, "action") == 0 && action == nullptr && cJSON_IsString(item))
        {
            action = item;
        }
        else
        {
            cJSON_Delete(root); // 多余字段/重复字段/错误类型
            return false;
        }
        fields++;
    }

    if (fields != 2 || device == nullptr || action == nullptr)
    {
        cJSON_Delete(root);
        return false;
    }

    const char *d = device->valuestring;
    const char *a = action->valuestring;
    bool ok = false;
    if (strcmp(d, "led") == 0 && strcmp(a, "on") == 0)
    {
        cmd = device_command::led_on;
        ok = true;
    }
    else if (strcmp(d, "led") == 0 && strcmp(a, "off") == 0)
    {
        cmd = device_command::led_off;
        ok = true;
    }
    else if (strcmp(d, "none") == 0 && strcmp(a, "none") == 0)
    {
        cmd = device_command::none;
        ok = true;
    }

    cJSON_Delete(root);
    return ok;
}

#ifndef ESP_PLATFORM

// 主机侧离线验证只需要纯解析函数
void device_command_init(void)
{
}
void device_command_execute(device_command cmd)
{
    (void)cmd;
}
const char *device_command_status(void)
{
    return "disabled";
}

#else

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <atomic>
#include "sdkconfig.h"
#include "ui.h"

#ifndef CONFIG_APP_LED_GPIO
#define CONFIG_APP_LED_GPIO -1
#endif
#ifndef CONFIG_APP_LED_ACTIVE_LOW
#define CONFIG_APP_LED_ACTIVE_LOW 0
#endif

static const constexpr char *TAG = "device";
static std::atomic<bool> s_ready{false};
static std::atomic<bool> s_on{false};

void device_command_init(void)
{
#if CONFIG_APP_LED_GPIO >= 0
    int gpio_num = CONFIG_APP_LED_GPIO;
    if (gpio_num < 0 || !GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)gpio_num))
    {
        ESP_LOGE(TAG, "invalid led gpio=%d", gpio_num);
        return;
    }

    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << gpio_num;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "led gpio config fail: %s", esp_err_to_name(err));
        return;
    }

    // 上电默认灭灯
    int off_level = CONFIG_APP_LED_ACTIVE_LOW ? 1 : 0;
    gpio_set_level((gpio_num_t)gpio_num, off_level);
    s_ready = true;
    s_on = false;
    ui_set_led(true, false);
    ESP_LOGI(TAG, "led gpio=%d active_low=%d", gpio_num, (int)CONFIG_APP_LED_ACTIVE_LOW);
#else
    ESP_LOGI(TAG, "led disabled (APP_LED_GPIO=-1)");
#endif
}

void device_command_execute(device_command cmd)
{
    if (cmd == device_command::none)
    {
        return;
    }
    if (!s_ready)
    {
        ESP_LOGW(TAG, "led not ready");
        return;
    }

    bool on = (cmd == device_command::led_on);
    int level = CONFIG_APP_LED_ACTIVE_LOW ? (on ? 0 : 1) : (on ? 1 : 0);
    esp_err_t err = gpio_set_level((gpio_num_t)CONFIG_APP_LED_GPIO, level);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "led set fail: %s", esp_err_to_name(err));
        return;
    }

    s_on = on;
    ui_set_led(true, on);
    ESP_LOGI(TAG, "led %s", on ? "on" : "off");
}

const char *device_command_status(void)
{
    if (!s_ready)
    {
        return "disabled";
    }
    return s_on ? "on" : "off";
}

#endif
