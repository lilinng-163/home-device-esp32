#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <esp_log.h>

#include "wifi.h"

static constexpr const char *TAG = "wifi_sta";

static void WIFI_STA_CallBack(void *event_handler_arg,esp_event_base_t event_base,int32_t event_id,void *event_data)
{
	static uint8_t overtime_count = 0;
	//wifi_sta模式启动成功
	if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		ESP_LOGI(TAG,"WIFI_EVENT_STA_START");
		//尝试连接
		ESP_ERROR_CHECK(esp_wifi_connect());
		overtime_count = 0;
	}
	//连接失败,尝试重连,失败输出日志
	if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{      
		ESP_LOGI(TAG,"connected failed. try to reconnect...");
		overtime_count++;
		if(overtime_count < 6)
		{
			vTaskDelay(pdMS_TO_TICKS(500));
			ESP_ERROR_CHECK(esp_wifi_connect());
		}
		else
		{
			ESP_LOGI(TAG,"overtime! failed.");
			overtime_count = 0;
		}
	}
	//连接成功
	if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		
		ESP_LOGI(TAG,"IP_EVENT_STA_GOT_IP");
		ip_event_got_ip_t* info = (ip_event_got_ip_t*)event_data;
	}
}

void wifi_sta::easy_start(wifi_config_t wifi_cfg)
{
	init(wifi_cfg, nullptr);
	start();
}

void wifi_sta::init(wifi_config_t wifi_cfg, sta_cb_t cb)
{
	wifi_cfg_ = wifi_cfg;
    cb_ = WIFI_STA_CallBack;
    if(cb != nullptr)
    {
        cb_ = cb;
    }

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
	//注册事件(wifi_sta模式启动成功,事件中等待连接ap)
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, cb_, NULL, NULL));
	//注册事件(是否得到ip)
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, cb_, NULL, NULL));
	//注册事件(连接失败)
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, cb_, NULL, NULL));

    //配置为STA模式
	esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    if(!is_change)
    {
        cfg_ = WIFI_INIT_CONFIG_DEFAULT();

    }

    ESP_ERROR_CHECK(esp_wifi_init(&cfg_));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_cfg_));
}

void wifi_sta::start(void)
{
    ESP_ERROR_CHECK(esp_wifi_start());
}