#pragma once

#include <string>

// 组装与 Orange Pi 对应的 0.0.1 版消息；失败时 payload 为空。
bool make_sensor_report(int device_id, double temperature, double humidity, std::string &payload);
