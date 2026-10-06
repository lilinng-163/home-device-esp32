#include <string>
#include <cstring>

#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <esp_afe_config.h>

#include "wake_word.h"

// 通道交错的音频数据 16位有符号 16kHz
void wake_word::init(void)
{
    srmodel_list_t *models = esp_srmodel_init("model");
    // 单麦：input_format="M"（M=麦克风, R=回采参考, N=未用）
    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);

    // 获取句柄
    afe_handle = esp_afe_handle_from_config(afe_config);
    // 创建实例
    afe_data = afe_handle->create_from_config(afe_config);

    afe_config_free(afe_config);
}
int16_t *wake_word::new_buf(void)
{
    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    int16_t *feed_buf = new int16_t[feed_chunksize * feed_nch];
    return feed_buf;
}

int wake_word::feed_byte_size(void)
{
    return afe_handle->get_feed_chunksize(afe_data) *
           afe_handle->get_feed_channel_num(afe_data) * sizeof(int16_t);
}
// 规范起见在线程末尾进行delete
void wake_word::feed(const int16_t *buf)
{
    afe_handle->feed(afe_data, buf);
}

afe_fetch_result_t *wake_word::fetch(void)
{
    res = afe_handle->fetch(afe_data);
    return res;
}