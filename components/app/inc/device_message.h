#pragma once

#include <string>

#include <cJSON.h>

inline constexpr const char *DEVICE_MESSAGE_VERSION = "0.0.1";

struct device_message
{
    std::string device_id;
    const cJSON *data = nullptr;
};

// 只借用 root 中的 data；调用者释放 root 前使用，不在业务回调中删除。
bool read_device_message(const cJSON *root, device_message &message);

// 上报和命令共用外层格式；data 由调用者管理，组包时复制一份。
bool make_device_message(int device_id, const cJSON *data, std::string &payload);
