#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <mutex>

#include <lvgl.h>

#include "lv_disp.h"
#include "ui.h"

namespace
{
constexpr int width = 240;
constexpr int height = 320;
std::array<uint16_t, width * height> framebuffer;
std::array<uint16_t, width * 20> draw_buffer;
std::mutex display_mutex;

void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    auto *source = reinterpret_cast<uint16_t *>(pixels);
    for (int y = area->y1; y <= area->y2; y++)
    {
        for (int x = area->x1; x <= area->x2; x++)
        {
            framebuffer[y * width + x] = *source++;
        }
    }
    lv_display_flush_ready(display);
}

// 输出真实 LVGL 帧缓冲，不用另一份网页或绘图代码模拟屏幕。
void save_frame(const char *path)
{
    lv_tick_inc(200);
    lv_timer_handler();
    lv_refr_now(nullptr);

    constexpr uint32_t image_bytes = width * height * 3;
    const uint32_t header[] = {
        54 + image_bytes, 0, 54, 40, width, height, 0x00180001, 0, image_bytes, 0, 0, 0, 0};
    std::ofstream file(path, std::ios::binary);
    file.write("BM", 2);
    file.write(reinterpret_cast<const char *>(header), sizeof(header));
    for (int y = height - 1; y >= 0; y--)
    {
        for (int x = 0; x < width; x++)
        {
            uint16_t pixel = framebuffer[y * width + x];
            char rgb[] = {static_cast<char>((pixel & 31) * 255 / 31),
                          static_cast<char>(((pixel >> 5) & 63) * 255 / 63),
                          static_cast<char>((pixel >> 11) * 255 / 31)};
            file.write(rgb, sizeof(rgb));
        }
    }
    std::printf("rendered %s\n", path);
}
} // namespace

std::mutex &lvgl_mutex()
{
    return display_mutex;
}

int main()
{
    lv_init();
    lv_display_t *display = lv_display_create(width, height);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(
        display, draw_buffer.data(), nullptr, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);

    ui_show_dashboard();
    ui_set_network(true, true);
    ui_set_led(true, false);
    ui_set_voice(ui_voice_state::ready);
    save_frame("ready.bmp");

    ui_set_voice(ui_voice_state::listening);
    save_frame("listening.bmp");

    ui_set_voice(ui_voice_state::thinking, "请帮我打开房间里的灯。");
    ui_set_voice(ui_voice_state::recognizing);
    ui_finish_listening();
    save_frame("thinking.bmp");

    ui_set_led(true, true);
    ui_set_voice(ui_voice_state::success);
    save_frame("led-on.bmp");

    ui_set_voice(ui_voice_state::ignored);
    save_frame("ignored.bmp");

    ui_set_network(false, false);
    ui_set_voice(ui_voice_state::error);
    save_frame("error.bmp");

    ui_show_provisioning("esp32");
    save_frame("provisioning.bmp");

    // 页面重建后旧定时器不能引用已删除控件。
    ui_show_dashboard();
    ui_set_led(true, false);
    ui_set_voice(ui_voice_state::ready);
    save_frame("rebuilt.bmp");
    return 0;
}
