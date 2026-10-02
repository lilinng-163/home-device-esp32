#pragma once

#include <string>
#include <esp_http_server.h>

// 配网页：GET 发页面（注入当前配置）/ POST 存配置到 NVS
httpd_uri_t &web_config_uri(void);

// 当前配置：GET 返回内存里的配置 JSON
httpd_uri_t &web_config_data_uri(void);

// 开机读出 NVS 后由 main 调一次，之后网页都走这份内存缓存
void web_config_set_config(const std::string &json);
