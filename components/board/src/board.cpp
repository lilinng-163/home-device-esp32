#include "board.h"
#include "sdkconfig.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "lcd.h"
#include "io_ex.h"
#include "key.h"
#include "codec.h"
#include "led.h"

const board_t &board()
{
    static const board_t b = {
        CONFIG_BOARD_I2C_SCL,            // i2c0_scl
        CONFIG_BOARD_I2C_SDA,            // i2c0_sda
        CONFIG_BOARD_XL9555_ADDR,         // xl9555_addr
        CONFIG_BOARD_XL9555_LCD_PWR_BIT,  // lcd_pwr_bit
        CONFIG_BOARD_XL9555_LCD_RST_BIT,  // lcd_rst_bit
        CONFIG_BOARD_XL9555_LCD_BL_BIT,   // lcd_bl_bit
        CONFIG_BOARD_LCD_SCK,             // lcd_sck
        CONFIG_BOARD_LCD_MOSI,            // lcd_mosi
        CONFIG_BOARD_LCD_CS,              // lcd_cs
        CONFIG_BOARD_LCD_DC,              // lcd_dc
        CONFIG_BOARD_KEY_GPIO,            // key_gpio
        CONFIG_BOARD_LED1,
        CONFIG_BOARD_ES8388_I2C_SCL,      // es8388_i2c_scl
        CONFIG_BOARD_ES8388_I2C_SDA,      // es8388_i2c_sda
        CONFIG_BOARD_ES8388_I2S_MCLK,     // es8388_mclk
        CONFIG_BOARD_ES8388_I2S_SCK,      // es8388_sck
        CONFIG_BOARD_ES8388_I2S_LRCK,     // es8388_lrck
        CONFIG_BOARD_ES8388_I2S_SDIN,     // es8388_sdin
        CONFIG_BOARD_ES8388_I2S_SDOUT,    // es8388_sdout
    };
    return b;
}

static i2c_master_bus_handle_t s_i2c0 = nullptr;

static xl9555 s_ioex;
static lcd    s_lcd;
static key    s_key;
static codec  s_codec;
static led    s_led1;

void board_init()
{
    const board_t &b = board();

#if CONFIG_BOARD_I2C && CONFIG_BOARD_AUDIO_ES8388 && CONFIG_BOARD_XL9555 && CONFIG_BOARD_LCD && CONFIG_BOARD_KEY
    // 1) 建 I2C0 总线
    i2c_master_bus_config_t ic = {};
    ic.i2c_port = I2C_NUM_0;
    ic.scl_io_num = (gpio_num_t)b.i2c0_scl;
    ic.sda_io_num = (gpio_num_t)b.i2c0_sda;
    ic.clk_source = I2C_CLK_SRC_DEFAULT;
    ic.glitch_ignore_cnt = 7;
    ic.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&ic, &s_i2c0));

    // 2) “XL9555 挂在 i2c0 上”
    s_ioex.init(s_i2c0, b.xl9555_addr);

    // 3) 面板电/复位/背光脚设为输出
    s_ioex.set_dir(b.lcd_pwr_bit, false);
    s_ioex.set_dir(b.lcd_rst_bit, false);
    s_ioex.set_dir(b.lcd_bl_bit,  false);

    // 4) 面板上电 + 复位脉冲
    s_ioex.write_pin(b.lcd_pwr_bit, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    s_ioex.write_pin(b.lcd_rst_bit, false);
    vTaskDelay(pdMS_TO_TICKS(20));
    s_ioex.write_pin(b.lcd_rst_bit, true);
    vTaskDelay(pdMS_TO_TICKS(120));

    // 5) ES8388 音频编解码器（挂在同一条 I2C0 上）
    // 方向：SDIN 是芯片 DAC 数据输入 → 接 ESP 的 dout；SDOUT 是芯片 ADC 数据输出 → 接 ESP 的 din
    s_codec.init(s_i2c0, b.es8388_mclk, b.es8388_sck, b.es8388_lrck,
                 b.es8388_sdin /*dout*/, b.es8388_sdout /*din*/);

    // 6) LCD（引脚给它，它自己建 SPI）
    lcd_config_t lc;
    lc.pin_clk  = (gpio_num_t)b.lcd_sck;
    lc.pin_mosi = (gpio_num_t)b.lcd_mosi;
    lc.pin_cs   = (gpio_num_t)b.lcd_cs;
    lc.pin_dc   = (gpio_num_t)b.lcd_dc;
    lc.pin_rst  = GPIO_NUM_NC;
    s_lcd.init(lc);

    // 6) LCD 背光接到 xl9555 的位
    int bl_bit = b.lcd_bl_bit;
    s_lcd.set_backlight([bl_bit](bool on){ s_ioex.write_pin(bl_bit, on); });
    s_lcd.backlight(true);

    // 7) 按键
    gpio_config_t kc = {};
    kc.mode = GPIO_MODE_INPUT;
    kc.pin_bit_mask = (1ULL << b.key_gpio);
    kc.pull_up_en = GPIO_PULLUP_ENABLE;
    s_key.init((gpio_num_t)b.key_gpio, kc);

    // 8) led
    gpio_config_t ledc = {};
    ledc.mode = GPIO_MODE_OUTPUT_OD;
    ledc.pin_bit_mask = (1ULL << b.led1_gpio);
    ledc.pull_up_en = GPIO_PULLUP_ENABLE;
    s_led1.init((gpio_num_t)b.led1_gpio, ledc);
#endif
}

lcd    &board_lcd()  { return s_lcd;  }
io_ex  &board_ioex() { return s_ioex; }
key    &board_key()  { return s_key;  }
codec  &board_codec(){ return s_codec; }
led    &board_led1()  { return s_led1;  }
