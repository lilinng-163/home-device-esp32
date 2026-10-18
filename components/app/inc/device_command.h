#pragma once
#include <cstddef>

enum class device_command
{
    none,
    led_on,
    led_off,
};

// 纯解析：只接受严格的两个字段和三种取值；失败返回 false，cmd 置 none。
// 该函数不依赖 ESP-IDF，可主机编译做离线验证。
bool device_command_parse(const char *json, size_t len, device_command &cmd);

// 设备侧执行：按 Kconfig 的 APP_LED_GPIO / APP_LED_ACTIVE_LOW 初始化并控制 LED。
void device_command_init(void);
void device_command_execute(device_command cmd);
const char *device_command_status(void);
