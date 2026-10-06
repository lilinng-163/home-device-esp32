#pragma once
#include <string>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "xfei.h"

extern QueueHandle_t queue_handle;

void cloud_xfei_proc_start(xfei_dictation &xfei);
