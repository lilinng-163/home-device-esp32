#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include <atomic>
#include <functional>

#include <esp_websocket_client.h>
#include <esp_event.h>

using xfei_dictation_cb_t = void (*)(void *ctx, esp_event_base_t, int32_t, void *);
using xfei_dictation_on_data_cb_t = void (*)(void *);
using xfei_dictation_final_cb_t = std::function<void(const std::string &)>;

class xfei_dictation
{
public:
    xfei_dictation(const std::string &appid,
                   const std::string &apikey,
                   const std::string &apisecret);
    int easy_start(void);
    void init(xfei_dictation_cb_t cb = nullptr);
    void set_event_cb(xfei_dictation_cb_t cb, void *ctx = nullptr); // 外部替换
    int start(void);
    void close(void);                                      // 释放 WS 客户端（下次可重新连）
    bool request(const uint8_t *pcm, int len, int status); // 发一帧：0=首 1=中 2=末；失败返回 false
    bool is_connected(void)
    {
        return handle_ != nullptr && esp_websocket_client_is_connected(handle_);
    }
    esp_websocket_client_handle_t &get_instance(void)
    {
        return handle_;
    }

    // 旧接口：收到原始 WS 事件时同步调用，调用方不能保存 event_data 指针
    void set_on_data_cb(xfei_dictation_on_data_cb_t cb);
    // 新接口：一句话完整结束（status==2）时回调一次，回调里只做拷贝/入队
    void set_final_cb(xfei_dictation_final_cb_t cb);
    bool final_received(void) const
    {
        return final_received_.load();
    }
    bool session_failed(void) const
    {
        return session_failed_.load();
    }
    // 诊断会话只验证传输，不把测试音频的结果交给设备控制任务。
    void set_result_delivery(bool enabled)
    {
        deliver_results_ = enabled;
    }
    void on_connected(void);
    void on_disconnected(void);
    void on_data(void *event_data);
    void on_error(void);
    void on_finish(void);
    ~xfei_dictation(void);

private:
    // 返回 authorization（base64），并把本次的 date 存进 date_
    std::string compute_authorization(void);
    void handle_message(const std::string &msg);
    void reset_pending(void);  // 丢掉半句/半帧，不碰 final_received_
    void reject_session(void); // 一旦缺帧或出错，本轮后续文本也不能提交
    void reset_session(void);  // 新会话：连 final_received_ 一起清

    static const constexpr size_t ws_max = 8 * 1024;
    static const constexpr size_t text_max = 4 * 1024;

    esp_websocket_client_config_t cfg_ = {};
    esp_websocket_client_handle_t handle_ = nullptr;
    esp_transport_list_handle_t transports_ = nullptr;
    esp_transport_handle_t ws_transport_ = nullptr;
    std::string uri_;   // cfg_.uri 指向它，必须常驻
    std::string date_;  // 签名与 URL 共用的 date
    std::string appid_; // 讯飞 APPID（common.app_id）
    std::string apikey_;
    std::string apisecret_;
    std::string text_;   // 累积识别文本（拼 ws[].cw[0].w）
    std::string ws_buf_; // 当前 WS 消息重组缓冲
    bool ws_bad_ = false;
    bool ws_open_ = false;
    size_t ws_frame_base_ = 0;
    xfei_dictation_on_data_cb_t on_data_cb_ = nullptr;
    xfei_dictation_final_cb_t final_cb_ = nullptr;
    std::atomic<bool> final_received_{false};
    std::atomic<bool> session_failed_{false};
    bool deliver_results_ = true;
};
