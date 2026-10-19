#pragma once

#include <cstdint>

enum class ui_voice_state : uint8_t
{
    starting,
    ready,
    listening,
    recognizing,
    thinking,
    success,
    ignored,
    error,
};

// 页面只在启动时创建一次；240 × 320，无触摸操作。
void ui_show_dashboard(void);
void ui_show_provisioning(const char *ssid);

// 业务任务只提交状态副本，LVGL 定时器统一刷新控件，不阻塞音频和网络回调。
void ui_set_network(bool wifi_connected, bool mqtt_connected);
void ui_set_led(bool available, bool on);
void ui_set_voice(ui_voice_state state, const char *text = nullptr);

// ASR 收尾不能覆盖已经开始的 LLM 请求或执行结果。
void ui_finish_listening(void);
