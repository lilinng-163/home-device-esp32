#include "lv_disp.h"

#include <mutex>

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "lcd.h"

static std::mutex s_mtx;

std::mutex &lvgl_mutex()
{
    return s_mtx;
}

// 传输完成 → 通知 LVGL（注册在 io() 上）
static bool flush_ready_cb(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *ctx)
{
    lv_display_flush_ready((lv_display_t *)ctx);
    return false;
}

// LVGL 画好一块 → 刷到屏（用 panel()）
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    lv_draw_sw_rgb565_swap(px_map, (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

static void tick_cb(void *)
{
    lv_tick_inc(2);
} // 每 2ms

void lvgl_port_init(lcd &screen)
{
    lv_init();
    lv_display_t *disp = lv_display_create(screen.width(), screen.height());

    size_t sz = (size_t)screen.width() * 20 * sizeof(lv_color16_t);
    void *b1 = heap_caps_malloc(sz, MALLOC_CAP_DMA);
    void *b2 = heap_caps_malloc(sz, MALLOC_CAP_DMA);
    lv_display_set_buffers(disp, b1, b2, sz, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(disp, screen.panel());
    lv_display_set_flush_cb(disp, flush_cb);

    esp_lcd_panel_io_callbacks_t cbs = {};
    cbs.on_color_trans_done = flush_ready_cb;
    esp_lcd_panel_io_register_event_callbacks(screen.io(), &cbs, disp);

    esp_timer_create_args_t targs = {};
    targs.callback = &tick_cb;
    targs.name = "lvgl_tick";
    esp_timer_handle_t tick = nullptr;
    esp_timer_create(&targs, &tick);
    esp_timer_start_periodic(tick, 2000); // 2ms
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (1)
    {
        uint32_t delay_ms;
        {
            std::lock_guard<std::mutex> lk(lvgl_mutex());
            delay_ms = lv_timer_handler();
        }
        if (delay_ms < 5)
        {
            delay_ms = 5;
        }
        if (delay_ms > 500)
        {
            delay_ms = 500;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void lvgl_task_start(void)
{
    xTaskCreate(lvgl_task, "lvgl", 8 * 1024, nullptr, 2, nullptr);
}
