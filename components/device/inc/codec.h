#pragma once

#include <cstdint>

#include <driver/i2c_master.h>

// 音频编解码器封装（当前实现：ES8388，芯片细节只在 codec.cpp 里）
class codec
{
public:
    // i2c_bus: 已初始化的 I2C 总线；其余为 I2S 引脚
    int init(i2c_master_bus_handle_t i2c_bus,
             int mclk, int bclk, int ws, int dout, int din);

    int open(int sample_rate, int channels, int bits);   // 按采样参数打开
    int read(void *buf, int len);                        // 收 PCM（麦克风）
    int write(const void *buf, int len);                 // 放 PCM（扬声器）
    int set_vol(int vol);
    int close();

private:
    void *tx_  = nullptr;   // i2s_chan_handle_t
    void *rx_  = nullptr;
    void *dev_ = nullptr;   // esp_codec_dev_handle_t
};
