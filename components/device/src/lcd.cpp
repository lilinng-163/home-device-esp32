#include "lcd.h"

#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_st7789.h>
#include <esp_log.h>

static const char *TAG = "lcd";

int lcd::init(const lcd_config_t &cfg)
{
    cfg_ = cfg;

    // 1) 初始化 SPI 总线
    spi_bus_config_t bus = {};
    bus.sclk_io_num   = cfg_.pin_clk;
    bus.mosi_io_num   = cfg_.pin_mosi;
    bus.miso_io_num   = -1;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = (int)(cfg_.h_res * 40 * (cfg_.bpp / 8));
    ESP_ERROR_CHECK(spi_bus_initialize(cfg_.host, &bus, SPI_DMA_CH_AUTO));

    // 2) 把 LCD 挂到 SPI 总线上（panel IO）
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.dc_gpio_num       = cfg_.pin_dc;
    io_cfg.cs_gpio_num       = cfg_.pin_cs;
    io_cfg.pclk_hz           = cfg_.pclk_hz;
    io_cfg.lcd_cmd_bits      = cfg_.cmd_bits;
    io_cfg.lcd_param_bits    = cfg_.param_bits;
    io_cfg.spi_mode          = cfg_.spi_mode;
    io_cfg.trans_queue_depth = cfg_.trans_queue_depth;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)cfg_.host, &io_cfg, &io_));

    // 3) 安装 ST7789 面板驱动
    esp_lcd_panel_dev_config_t dev_cfg = {};
    dev_cfg.reset_gpio_num = cfg_.pin_rst;
    dev_cfg.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
    dev_cfg.bits_per_pixel = cfg_.bpp;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_, &dev_cfg, &panel_));

    // 4) 面板初始化序列
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, true, true));   // 竖屏翻转
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "ST7789 ready (%dx%d)", cfg_.h_res, cfg_.v_res);
    return 0;
}
