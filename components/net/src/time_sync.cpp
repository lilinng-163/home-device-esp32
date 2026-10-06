#include "time_sync.h"

#include <ctime>

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <esp_netif_sntp.h>

static const char *TAG = "time_sync";

bool time_sync(void)
{
    // 只初始化一次；之后 SNTP 会周期自动重同步
    static bool inited = false;
    if (!inited)
    {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        if (esp_netif_sntp_init(&cfg) != ESP_OK)
        {
            ESP_LOGE(TAG, "sntp init failed");
            return false;
        }
        inited = true;
    }

    // 等第一次同步完成（联网后才有意义）
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK)
    {
        ESP_LOGW(TAG, "sntp sync timeout");
        return false;
    }

    ESP_LOGI(TAG, "time synced: %s", time_rfc1123().c_str());
    return true;
}

std::string time_rfc1123(void)
{
    time_t now = time(nullptr);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);   // UTC（不随 TZ 变）

    char buf[64];
    // 默认 C locale → 英文缩写 (Mon / Oct)；别 setlocale 成中文
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm_utc);
    return std::string(buf);
}
