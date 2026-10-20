#include "sensor_report.h"

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

    cJSON *root = cJSON_CreateObject();
    if (root == nullptr)
    {
        return false;
    }

    // 外层描述协议与设备，传感器字段集中放在 data 对象中。
    bool ready = cJSON_AddStringToObject(root, "version", "0.0.1") != nullptr &&
                 cJSON_AddNumberToObject(root, "device_id", device_id) != nullptr;
    cJSON *data = ready ? cJSON_AddObjectToObject(root, "data") : nullptr;
    ready = data != nullptr &&
            cJSON_AddNumberToObject(data, "temperature", temperature) != nullptr &&
            cJSON_AddNumberToObject(data, "humidity", humidity) != nullptr;

    char *text = ready ? cJSON_PrintUnformatted(root) : nullptr;
    bool success = text != nullptr;
    if (success)
    {
        payload = text;
    }

    // std::string 已复制正文，及时释放 cJSON 树和序列化缓冲区。
    cJSON_free(text);
    cJSON_Delete(root);
    return success;
}
