#include "web_config.h"

#include <string>
#include <nvs_flash.h>
#include <esp_log.h>

// objcopy 生成的符号（本组件 www/index.html）
extern const unsigned char _binary_index_html_start[];
extern const unsigned char _binary_index_html_end[];

static const char *TAG = "web_config";

static std::string s_cfg = "{}";   // 当前配置的内存缓存
static bool        s_cfg_loaded = false;

static std::string nvs_read_config(void)
{
    nvs_handle_t h;
    if (nvs_open("cfg", NVS_READONLY, &h) != ESP_OK)
        return "{}";

    std::string out = "{}";
    size_t len = 0;
    if (nvs_get_str(h, "json", nullptr, &len) == ESP_OK && len > 0)
    {
        out.resize(len);
        nvs_get_str(h, "json", out.data(), &len);
        if (!out.empty() && out.back() == '\0')
            out.pop_back();
    }
    nvs_close(h);
    return out;
}

static void nvs_write_config(const std::string &json)
{
    nvs_handle_t h;
    if (nvs_open("cfg", NVS_READWRITE, &h) != ESP_OK)
        return;
    nvs_set_str(h, "json", json.c_str());
    nvs_commit(h);
    nvs_close(h);
}

// main 开机读过 NVS 后把原始 JSON 交进来
void web_config_set_config(const std::string &json)
{
    s_cfg = json.empty() ? "{}" : json;
    s_cfg_loaded = true;
}

// 兜底：万一没人 set，就在第一次用到时读一次 NVS
static const std::string &cached_config()
{
    if (!s_cfg_loaded)
    {
        s_cfg = nvs_read_config();
        s_cfg_loaded = true;
    }
    return s_cfg;
}

static esp_err_t config_page_handler(httpd_req_t *req)
{
    if (req->method == HTTP_POST)
    {
        char buf[2048];
        int total = req->content_len;
        if (total >= (int)sizeof(buf)) total = sizeof(buf) - 1;

        int got = 0;
        while (got < total)
        {
            int r = httpd_req_recv(req, buf + got, total - got);
            if (r <= 0)                     // 出错/超时
            {
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
            got += r;
        }
        buf[got] = '\0';

        std::string body(buf, got);
        if (body == cached_config())        // 和当前配置一致 → 不写 NVS
        {
            ESP_LOGI(TAG, "config unchanged, skip write");
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_send(req, "unchanged", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }

        nvs_write_config(body);
        s_cfg = body;
        s_cfg_loaded = true;
        ESP_LOGI(TAG, "config saved: %s", body.c_str());

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // GET → 发 index.html，并把当前配置（内存）注入进去
    std::string html((const char *)_binary_index_html_start,
                     (const char *)_binary_index_html_end);

    const std::string &cfg = cached_config();
    std::string esc;
    esc.reserve(cfg.size());
    for (char ch : cfg)
    {
        if (ch == '<') esc += "\\u003c";   // 防 </script> 截断
        else           esc += ch;
    }
    std::string inject = "<script>window.__CFG__=" + esc + ";</script>";
    size_t pos = html.find("<body>");
    if (pos != std::string::npos) html.insert(pos + 6, inject);
    else                          html.insert(0, inject);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.size());
    return ESP_OK;
}

static esp_err_t config_data_handler(httpd_req_t *req)
{
    const std::string &json = cached_config();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());
    return ESP_OK;
}

httpd_uri_t &web_config_uri(void)
{
    static httpd_uri_t uri = []{
        httpd_uri_t u = {};
        u.uri = "/";
        u.method = (httpd_method_t)HTTP_ANY;
        u.handler = config_page_handler;
        return u;
    }();
    return uri;
}

httpd_uri_t &web_config_data_uri(void)
{
    static httpd_uri_t uri = []{
        httpd_uri_t u = {};
        u.uri = "/config";
        u.method = (httpd_method_t)HTTP_GET;
        u.handler = config_data_handler;
        return u;
    }();
    return uri;
}
