#include "sensor_report.h"
#include "device_message.h"

#include <cmath>

#include <cJSON.h>

bool make_sensor_report(int device_id, double temperature, double humidity, std::string &payload)
{
    payload.clear();
    if (device_id <= 0 || !std::isfinite(temperature) || !std::isfinite(humidity) || humidity < 0 ||
        humidity > 100)
    {
        return false;
    }

    cJSON *data = cJSON_CreateObject();
    if (data == nullptr)
    {
        return false;
    }

    // 这里只组装传感器数据，公共外层交给统一的报文函数。
    bool ready = cJSON_AddNumberToObject(data, "temperature", temperature) != nullptr &&
                 cJSON_AddNumberToObject(data, "humidity", humidity) != nullptr;
    bool success = ready && make_device_message(device_id, data, payload);
    cJSON_Delete(data);
    return success;
}
