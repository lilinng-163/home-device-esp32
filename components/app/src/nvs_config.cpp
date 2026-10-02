#include "nvs_config.h"

#include <nvs_flash.h>
#include <cJSON.h>

bool nvs_config_load(app_config &cfg, std::string *raw_json)
{
    nvs_handle_t h;
    if (nvs_open("cfg", NVS_READONLY, &h) != ESP_OK)
    {
        if (raw_json) *raw_json = "{}";
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
            raw.pop_back();
        if (raw_json) *raw_json = raw;

        cJSON *root = cJSON_Parse(raw.c_str());
        if (root)
        {
            cJSON *wifi      = cJSON_GetObjectItem(root, "wifi");
            cJSON *mqtt      = cJSON_GetObjectItem(root, "mqtt");
            cJSON *publish   = cJSON_GetObjectItem(root, "publish");
            cJSON *subscribe = cJSON_GetObjectItem(root, "subscribe");

            if (cJSON_IsObject(wifi))
            {
                cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
                cJSON *pass = cJSON_GetObjectItem(wifi, "pass");
                if (cJSON_IsString(ssid)) cfg.ssid = ssid->valuestring;
                if (cJSON_IsString(pass)) cfg.pass = pass->valuestring;
            }

            if (cJSON_IsObject(mqtt))
            {
                cJSON *host = cJSON_GetObjectItem(mqtt, "host");
                cJSON *port = cJSON_GetObjectItem(mqtt, "port");
                cJSON *cid  = cJSON_GetObjectItem(mqtt, "client_id");
                cJSON *user = cJSON_GetObjectItem(mqtt, "username");
                cJSON *pw   = cJSON_GetObjectItem(mqtt, "password");
                if (cJSON_IsString(host)) cfg.host = host->valuestring;
                if (cJSON_IsNumber(port)) cfg.port = port->valueint;
                if (cJSON_IsString(cid))  cfg.client_id = cid->valuestring;
                if (cJSON_IsString(user)) cfg.username = user->valuestring;
                if (cJSON_IsString(pw))   cfg.password = pw->valuestring;
            }

            if (cJSON_IsArray(publish))
                for (int i = 0, n = cJSON_GetArraySize(publish); i < n; i++)
                {
                    cJSON *e = cJSON_GetArrayItem(publish, i);
                    if (cJSON_IsString(e)) cfg.publish.push_back(e->valuestring);
                }

            if (cJSON_IsArray(subscribe))
                for (int i = 0, n = cJSON_GetArraySize(subscribe); i < n; i++)
                {
                    cJSON *e = cJSON_GetArrayItem(subscribe, i);
                    if (cJSON_IsString(e)) cfg.subscribe.push_back(e->valuestring);
                }

            cJSON_Delete(root);
            ok = true;
        }
    }
    else
    {
        if (raw_json) *raw_json = "{}";
    }

    nvs_close(h);
    return ok;
}
