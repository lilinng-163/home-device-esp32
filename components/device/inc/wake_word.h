#pragma once
#include <string>
#include <cstring>

#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <esp_afe_config.h>

class wake_word
{
public:
    void init(void);
    int16_t *new_buf(void);
    void feed(const int16_t *buf);
    afe_fetch_result_t *fetch(void);
    esp_afe_sr_data_t *get_data(void) {return afe_data;}
    void destory(void) {afe_handle->destroy(afe_data);}
private:
    const esp_afe_sr_iface_t *afe_handle;
    esp_afe_sr_data_t *afe_data;
    afe_fetch_result_t *res;
};