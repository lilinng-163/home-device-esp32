#include "codec.h"

#include <driver/i2s_std.h>
#include <esp_log.h>

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"   // audio_codec_new_* / es8388_codec_new / ES8388_CODEC_DEFAULT_ADDR

static const char *TAG = "codec";

int codec::init(i2c_master_bus_handle_t i2c_bus,
                int mclk, int bclk, int ws, int dout, int din)
{
    // 1) 建 I2S 通道（ESP32 作 master）
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_chan_handle_t tx = nullptr;
    i2s_chan_handle_t rx = nullptr;
    if (i2s_new_channel(&chan_cfg, &tx, &rx) != ESP_OK)
        return -1;
    tx_ = tx;
    rx_ = rx;

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000);
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = (gpio_num_t)mclk;
    std_cfg.gpio_cfg.bclk = (gpio_num_t)bclk;
    std_cfg.gpio_cfg.ws   = (gpio_num_t)ws;
    std_cfg.gpio_cfg.dout = (gpio_num_t)dout;
    std_cfg.gpio_cfg.din  = (gpio_num_t)din;
    i2s_channel_init_std_mode(tx, &std_cfg);
    i2s_channel_init_std_mode(rx, &std_cfg);

    // 2) 数据接口（I2S）
    audio_codec_i2s_cfg_t i2s_cfg = {};
    i2s_cfg.port      = I2S_NUM_0;
    i2s_cfg.rx_handle = rx;
    i2s_cfg.tx_handle = tx;
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    // 3) 控制接口（I2C）
    audio_codec_i2c_cfg_t i2c_cfg = {};
    i2c_cfg.port       = I2C_NUM_0;
    i2c_cfg.addr       = ES8388_CODEC_DEFAULT_ADDR;
    i2c_cfg.bus_handle = i2c_bus;
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    // 4) ES8388（具体芯片只在这一处）
    es8388_codec_cfg_t es_cfg = {};
    es_cfg.ctrl_if     = ctrl_if;
    es_cfg.gpio_if     = audio_codec_new_gpio();
    es_cfg.codec_mode  = ESP_CODEC_DEV_WORK_MODE_ADC;   // 只做录音
    es_cfg.master_mode = false;                          // ESP 是 I2S master
    es_cfg.pa_pin      = -1;
    const audio_codec_if_t *codec_if = es8388_codec_new(&es_cfg);

    // 5) 上层句柄
    esp_codec_dev_cfg_t dev_cfg = {};
    dev_cfg.codec_if = codec_if;
    dev_cfg.data_if  = data_if;
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_ = esp_codec_dev_new(&dev_cfg);

    if (!dev_)
    {
        ESP_LOGE(TAG, "esp_codec_dev_new failed");
        return -1;
    }
    ESP_LOGI(TAG, "ES8388 ready");
    return 0;
}

int codec::open(int sample_rate, int channels, int bits)
{
    if (!dev_)
        return -1;

    esp_codec_dev_sample_info_t fs = {};
    fs.sample_rate     = sample_rate;
    fs.channel         = channels;
    fs.bits_per_sample = bits;

    return (esp_codec_dev_open((esp_codec_dev_handle_t)dev_, &fs) == 0) ? 0 : -1;
}

int codec::read(void *buf, int len)
{
    if (!dev_)
        return -1;
    if (esp_codec_dev_read((esp_codec_dev_handle_t)dev_, buf, len) != ESP_CODEC_DEV_OK)
        return -1;
    return len;
}

int codec::write(const void *buf, int len)
{
    if (!dev_)
        return -1;
    if (esp_codec_dev_write((esp_codec_dev_handle_t)dev_, (void *)buf, len) != ESP_CODEC_DEV_OK)
        return -1;
    return len;
}

int codec::set_vol(int vol)
{
    return esp_codec_dev_set_out_vol((esp_codec_dev_handle_t)dev_, vol);
}

int codec::close()
{
    if (!dev_)
        return -1;
    esp_codec_dev_close((esp_codec_dev_handle_t)dev_);
    return 0;
}
