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

void wifi_ap::WIFI_AP_CallBack(void *event_handler_arg,esp_event_base_t event_base,int32_t event_id,void *event_data)
{
	wifi_ap *self = static_cast<wifi_ap *>(event_handler_arg);
	//连接事件
	if(event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
	{
		ip_event_ap_staipassigned_t* event_info = (ip_event_ap_staipassigned_t*)event_data;
		ESP_LOGI("wifi_ap", "connected");
	}
	//断连事件
	if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
	{
		wifi_event_ap_stadisconnected_t* event_info = (wifi_event_ap_stadisconnected_t*)event_data;
	}
}

void wifi_ap::easy_start(wifi_config_t &wifi_cfg)
{
	init(wifi_cfg, nullptr);
	start();
}

void wifi_ap::init(wifi_config_t &wifi_cfg, ap_cb_t cb)
{
	wifi_cfg_ = wifi_cfg;
	cb_ = WIFI_AP_CallBack;
	if(cb != nullptr)
	{
		cb_ = cb;
	}
	ESP_ERROR_CHECK(esp_netif_init());

	//创建默认事件循环,用于处理wifi事件
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	
	//监听WIFI_EVENT,触发后进入回调函数
	/*注册设备连接的回调函数*/
	esp_event_handler_instance_register(IP_EVENT,
										IP_EVENT_AP_STAIPASSIGNED,
										cb_,
										this,
										NULL);
	/*注册设备断开的回调函数*/
	esp_event_handler_instance_register(WIFI_EVENT,
										WIFI_EVENT_AP_STADISCONNECTED,
										cb_,
										this,
										NULL);
	

	//使用默认ap模式创建esp_netif对象，将此对象连接到wifi并注册默认wifi处理程序
	//esp_netif_ap可以用于修改ap设置
	esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

	//初始化wifi配置
	if(!is_change)
	{
		cfg_ = WIFI_INIT_CONFIG_DEFAULT();
	}

	//初始化wifi,分配资源(控制结构，RX/TX缓冲区,NVS结构,启动wifi任务)
	/********调用此api才能调用其他wifi api********/
	esp_wifi_init(&cfg_);

	/**************************************wifi配置阶段********************************************************/
	
	//设置WiFi模式
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

	//设置wifi模式具体配置
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&wifi_cfg_));
}

void wifi_ap::start(void)
{
	ESP_ERROR_CHECK(esp_wifi_start());
}
