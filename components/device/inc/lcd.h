#pragma once

#include <functional>

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

// LCD 连接与参数（类型与 ESP-IDF API 对齐；默认引脚对齐参考例程，按接线改）
struct lcd_config_t
{
    spi_host_device_t host = SPI2_HOST;

    gpio_num_t pin_clk  = GPIO_NUM_10;
    gpio_num_t pin_mosi = GPIO_NUM_9;
    gpio_num_t pin_cs   = GPIO_NUM_6;
    gpio_num_t pin_dc   = GPIO_NUM_7;
    gpio_num_t pin_rst  = GPIO_NUM_8;

    int          h_res = 240;
    int          v_res = 320;
    unsigned int pclk_hz = 60 * 1000 * 1000;
    int          cmd_bits = 8;
    int          param_bits = 8;
    uint32_t     bpp = 16;
    int          spi_mode = 0;
    size_t       trans_queue_depth = 10;
};

class lcd
{
public:
    // 背光接口：一个可调用对象，true=亮 false=灭。
    // 接 ESP32 引脚、还是 IO 扩展芯片，都写在你注册进来的这个函数里。
    using backlight_ctrl_t = std::function<void(bool on)>;

    int init(const lcd_config_t &cfg);

    void set_backlight(backlight_ctrl_t ctrl) { backlight_ = std::move(ctrl); }
    void backlight(bool on) { if (backlight_) backlight_(on); }

    esp_lcd_panel_handle_t    panel(void) const { return panel_; }
    esp_lcd_panel_io_handle_t io(void)    const { return io_; }
    int width(void)  const { return cfg_.h_res; }
    int height(void) const { return cfg_.v_res; }

private:
    lcd_config_t cfg_;
    esp_lcd_panel_io_handle_t io_ = nullptr;
    esp_lcd_panel_handle_t    panel_ = nullptr;
    backlight_ctrl_t          backlight_;
};
