#pragma once

#include <cstdint>

#include <driver/i2c_master.h>
#include <esp_io_expander.h>

// IO 扩展芯片接口：上层只认它，不认具体芯片
class io_ex
{
public:
    virtual ~io_ex() = default;

    virtual int set_dir(uint8_t bit, bool input) = 0;    // true=输入, false=输出
    virtual int write_pin(uint8_t bit, bool level) = 0;  // 写某一位
    virtual int read_pin(uint8_t bit) = 0;               // 返回 0/1，出错 -1
};

// XL9555：16 位 I2C IO 扩展（基于 espressif/esp_io_expander 框架）
// 位编号：0..7 = port0(IO0_0..IO0_7)，8..15 = port1(IO1_0..IO1_7)
//
// DNESP32S3 上的用途（bit）：
//   2=SPK_EN  3=BEEP  8=LCD_BL  9=CT_RST  10=SLCD_RST  11=SLCD_PWR
class xl9555 : public io_ex
{
public:
    static constexpr uint32_t DEFAULT_ADDR = 0x20;   // A0/A1/A2 接地

    int init(i2c_master_bus_handle_t bus, uint32_t addr = DEFAULT_ADDR);

    int set_dir(uint8_t bit, bool input) override;
    int write_pin(uint8_t bit, bool level) override;
    int read_pin(uint8_t bit) override;

private:
    esp_io_expander_handle_t handle_ = nullptr;
};
