#include <string>
#include <cstring>

#include <esp_afe_sr_iface.h>
#include <esp_afe_config.h>

#include "wake_word.h"
#include "wakeup_board.h"

static TaskHandle_t feed_task_handle;
static TaskHandle_t fetch_task_handle;

void feed_task(void *parameters)
{
    wake_word *wk = reinterpret_cast<wake_word *>(parameters);
    int16_t *buf = wk->new_buf();
    while(1)
    {
        // 音频取数据喂给buf
        
        //
        wk->feed(buf);
    }
    if(buf)
    {
        delete[] buf;
        buf = nullptr;
    }
    vTaskDelete(NULL);
}

void fetch_task(void *parameters)
{
    wake_word *wk = reinterpret_cast<wake_word *>(parameters);
    while(1)
    {

    }
    if(wk->get_data())
    {
        wk->destory();
    }
    vTaskDelete(NULL);
}

void wakeup_board_start(wake_word &wk)
{
    const constexpr uint32_t feed_task_stack_size = 8 * 1024;
    const constexpr uint32_t fetch_task_stack_size = 4 * 1024;
    const constexpr UBaseType_t feed_task_priority = 5;
    const constexpr UBaseType_t fetch_task_priority = 5;
    xTaskCreatePinnedToCore(feed_task, "feed_task", feed_task_stack_size, 
                            reinterpret_cast<void *>(&wk), feed_task_priority, &feed_task_handle, 0);
    xTaskCreatePinnedToCore(fetch_task, "fetch_task", fetch_task_stack_size, 
                            reinterpret_cast<void *>(&wk), fetch_task_priority, &fetch_task_handle, 1);

}