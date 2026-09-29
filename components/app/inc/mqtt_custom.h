#pragma once
#include "mqtt.h"

// 建队列+任务、注册消息回调；设备相关的处理都在 app 层
void mqtt_app_start(mqtt &client);
