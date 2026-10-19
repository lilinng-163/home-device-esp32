#include "nvs_config.h"

#include <cstring>

#include <nvs_flash.h>
#include <cJSON.h>

bool nvs_config_load(app_config &cfg, std::string *raw_json)
{
    nvs_handle_t h;
    if (nvs_open("cfg", NVS_READONLY, &h) != ESP_OK)
    {
        if (raw_json)
        {
            *raw_json = "{}";
        }
        return false;
    }

    bool ok = false;
    size_t len = 0;
    if (nvs_get_str(h, "json", nullptr, &len) == ESP_OK && len > 0)
    {
        std::string raw;
        raw.resize(len);
        nvs_get_str(h, "json", raw.data(), &len);
        if (!raw.empty() && raw.back() == '\0')
        {
            raw.pop_back();
        }
        if (raw_json)
        {
            *raw_json = raw;
        }

        cJSON *root = cJSON_Parse(raw.c_str());
        if (root)
        {
            cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
            cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
            cJSON *publish = cJSON_GetObjectItem(root, "publish");
            cJSON *subscribe = cJSON_GetObjectItem(root, "subscribe");

            if (cJSON_IsObject(wifi))
            {
                cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
                cJSON *pass = cJSON_GetObjectItem(wifi, "pass");
                if (cJSON_IsString(ssid))
                {
                    cfg.ssid = ssid->valuestring;
                }
                if (cJSON_IsString(pass))
                {
                    cfg.pass = pass->valuestring;
                }
            }

            if (cJSON_IsObject(mqtt))
            {
                cJSON *host = cJSON_GetObjectItem(mqtt, "host");
                cJSON *port = cJSON_GetObjectItem(mqtt, "port");
                cJSON *cid = cJSON_GetObjectItem(mqtt, "client_id");
                cJSON *user = cJSON_GetObjectItem(mqtt, "username");
                cJSON *pw = cJSON_GetObjectItem(mqtt, "password");
                // 旧示例地址迁移到本次指定的 broker；保留用户另行保存的自定义地址。
                if (cJSON_IsString(host) && host->valuestring[0] != '\0' &&
                    strcmp(host->valuestring, "47.113.149.47") != 0)
                {
                    cfg.host = host->valuestring;
                }
                if (cJSON_IsNumber(port) && port->valueint > 0 && port->valueint <= 65535)
                {
                    cfg.port = port->valueint;
                }
                if (cJSON_IsString(cid))
                {
                    cfg.client_id = cid->valuestring;
                }
                if (cJSON_IsString(user))
                {
                    cfg.username = user->valuestring;
                }
                if (cJSON_IsString(pw))
                {
                    cfg.password = pw->valuestring;
                }
            }

            if (cJSON_IsArray(publish))
            {
                for (int i = 0, n = cJSON_GetArraySize(publish); i < n; i++)
                {
                    cJSON *e = cJSON_GetArrayItem(publish, i);
                    if (cJSON_IsString(e))
                    {
                        cfg.publish.push_back(e->valuestring);
                    }
                }
            }

            if (cJSON_IsArray(subscribe))
            {
                for (int i = 0, n = cJSON_GetArraySize(subscribe); i < n; i++)
                {
                    cJSON *e = cJSON_GetArrayItem(subscribe, i);
                    if (cJSON_IsString(e))
                    {
                        cfg.subscribe.push_back(e->valuestring);
                    }
                }
            }

            // 网页回显使用实际生效的地址，避免下一次保存又带回旧值。
            if (raw_json != nullptr)
            {
                if (!cJSON_IsObject(mqtt))
                {
                    cJSON_DeleteItemFromObject(root, "mqtt");
                    mqtt = cJSON_AddObjectToObject(root, "mqtt");
                }
                cJSON_DeleteItemFromObject(mqtt, "host");
                cJSON_AddStringToObject(mqtt, "host", cfg.host.c_str());
                cJSON_DeleteItemFromObject(mqtt, "port");
                cJSON_AddNumberToObject(mqtt, "port", cfg.port);

                char *effective = cJSON_PrintUnformatted(root);
                if (effective != nullptr)
                {
                    *raw_json = effective;
                    cJSON_free(effective);
                }
            }

            cJSON_Delete(root);
            ok = true;
        }
    }
    else
    {
        if (raw_json)
        {
            *raw_json = "{}";
        }
    }

    nvs_close(h);
    return ok;
}
