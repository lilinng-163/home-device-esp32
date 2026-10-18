#pragma once
#include <string>
#include <cstddef>
#include <cstdint>

#include <esp_err.h>
#include <esp_http_client.h>

class openai_llm
{
  public:
    ~openai_llm()
    {
        cleanup();
    }
    openai_llm() = default;
    openai_llm(const openai_llm &) = delete;
    openai_llm &operator=(const openai_llm &) = delete;

    struct llm_req_options
    {
        std::string url;
        std::string model;
        std::string api_key;
        std::string system_prompt;
        bool thinking_enabled = false;
    };

    void init(const llm_req_options &options);
    void set_api_key(const std::string &key);
    esp_err_t request(const std::string &user_text);
    esp_err_t get_content(std::string &content) const; // 解析 choices[0].message.content
    void cleanup(void);

    std::string response; // 最近一次响应的原始 body（常驻成员，不放到静态全局）
    static const constexpr size_t response_limit = 16 * 1024;

  private:
    static esp_err_t event_handler(esp_http_client_event_t *evt);
    esp_err_t build_body(const std::string &user_text, std::string &body) const;

    esp_http_client_handle_t handle_ = nullptr;
    llm_req_options options_;
    bool response_overflow_ = false;
    int64_t last_done_us_ = 0;
};
