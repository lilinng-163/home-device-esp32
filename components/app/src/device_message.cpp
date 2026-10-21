#include "device_message.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static bool valid_name(const char *name)
{
    if (name == nullptr || name[0] == '\0' || strlen(name) > 64)
    {
        return false;
    }

    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(name); *p; p++)
    {
        if (*p < 32 || *p == 127)
        {
            return false;
        }
    }
    return true;
}

bool read_device_message(const cJSON *root, device_message &message)
{
    message = {};
    if (!cJSON_IsObject(root) || cJSON_GetArraySize(root) != 3)
    {
        return false;
    }

    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "device_id");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsString(version) || strcmp(version->valuestring, DEVICE_MESSAGE_VERSION) != 0 ||
        !cJSON_IsObject(data) || cJSON_GetArraySize(data) < 1 || cJSON_GetArraySize(data) > 64)
    {
        return false;
    }

    std::string device_id;
    if (cJSON_IsString(id) && valid_name(id->valuestring))
    {
        device_id = id->valuestring;
    }
    else if (cJSON_IsNumber(id) && std::isfinite(id->valuedouble) && id->valuedouble > 0 &&
             id->valuedouble <= 9007199254740991.0 && std::floor(id->valuedouble) == id->valuedouble)
    {
        // cJSON 使用 double；限制在精确整数范围内，数字 1 与字符串 "1" 对应同一设备。
        char number[32];
        snprintf(number, sizeof(number), "%.0f", id->valuedouble);
        device_id = number;
    }
    else
    {
        return false;
    }

    for (const cJSON *field = data->child; field != nullptr; field = field->next)
    {
        if (!valid_name(field->string))
        {
            return false;
        }
    }

    message.device_id = device_id;
    message.data = data;
    return true;
}

bool make_device_message(int device_id, const cJSON *data, std::string &payload)
{
    payload.clear();
    if (device_id <= 0 || !cJSON_IsObject(data))
    {
        return false;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == nullptr)
    {
        return false;
    }

    bool ready = cJSON_AddStringToObject(root, "version", DEVICE_MESSAGE_VERSION) != nullptr &&
                 cJSON_AddNumberToObject(root, "device_id", device_id) != nullptr;
    cJSON *copy = ready ? cJSON_Duplicate(data, true) : nullptr;
    if (copy != nullptr && !cJSON_AddItemToObject(root, "data", copy))
    {
        cJSON_Delete(copy);
    }

    device_message message;
    char *text = read_device_message(root, message) ? cJSON_PrintUnformatted(root) : nullptr;
    bool success = text != nullptr;
    if (success)
    {
        payload = text;
    }

    cJSON_free(text);
    cJSON_Delete(root);
    return success;
}
