#pragma once

#include <string>
#include <vector>

// 一份完整配置（从 NVS 里那份 JSON 解析出来）
struct app_config
{
    std::string ssid;
    std::string pass;

    // EMQX 默认 TCP 监听端口；网页保存的配置仍可覆盖这些缺省值。
    std::string host = "mqtt.example.com";
    int port = 1883;
    std::string client_id;
    std::string username;
    std::string password;
    std::vector<std::string> publish;
    std::vector<std::string> subscribe;
};

// 从 NVS("cfg","json") 读原始 JSON 并解析；成功返回 true
// raw_json 非空时，同时把原始 JSON 拷出来（给网页回显用，避免重复读 NVS）
bool nvs_config_load(app_config &cfg, std::string *raw_json = nullptr);
