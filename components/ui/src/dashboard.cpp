#include "ui.h"

#include <cstdio>
#include <cstring>
#include <mutex>

#include <lvgl.h>

#include "lv_disp.h"

LV_FONT_DECLARE(ui_font_chinese_16);

namespace
{
constexpr uint32_t background = 0x101A20;
constexpr uint32_t surface = 0x1B2B32;
constexpr uint32_t foreground = 0xEAF2EE;
constexpr uint32_t muted = 0x8CA19F;
constexpr uint32_t accent = 0xAFE3CA;
constexpr uint32_t warning = 0xF1BE82;

// 这里只保存固定大小的数据，不保存调用方的字符串指针，也不从业务线程操作 LVGL。
struct ui_snapshot
{
    ui_voice_state voice = ui_voice_state::starting;
    bool wifi = false;
    bool mqtt = false;
    bool led_available = false;
    bool led_on = false;
    char transcript[192] = {};
    unsigned revision = 0;
};

std::mutex state_mutex;
ui_snapshot state;
bool provisioning = false;
unsigned rendered_revision = 0;
lv_timer_t *refresh_timer = nullptr;

lv_obj_t *voice_title = nullptr;
lv_obj_t *voice_hint = nullptr;
lv_obj_t *transcript = nullptr;
lv_obj_t *led_value = nullptr;
lv_obj_t *led_dot = nullptr;
lv_obj_t *wifi_value = nullptr;
lv_obj_t *mqtt_value = nullptr;
lv_obj_t *wave_bars[7] = {};

lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(surface), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t *
make_label(lv_obj_t *parent, int x, int y, int width, const char *text, uint32_t color = foreground)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &ui_font_chinese_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

lv_obj_t *prepare_screen(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_t *brand = make_label(screen, 14, 10, 180, "lilinng / VOICE NODE", muted);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_14, 0);
    make_label(screen, 14, 31, 212, provisioning ? "设备配网" : "语音控制");
    return screen;
}

void render_snapshot(const ui_snapshot &current)
{
    const char *title = "正在启动";
    const char *hint = "正在准备语音服务";
    uint32_t color = accent;

    switch (current.voice)
    {
    case ui_voice_state::ready:
        title = "等待唤醒";
        hint = "说「嗨乐鑫」\n然后说出设备指令";
        break;
    case ui_voice_state::listening:
        title = "正在聆听";
        hint = "请说出你的指令\n说完后稍等片刻";
        break;
    case ui_voice_state::recognizing:
        title = "正在识别";
        hint = "已听到，正在识别\n请稍等片刻";
        break;
    case ui_voice_state::thinking:
        title = "正在处理";
        hint = "已收到，正在理解\n你的设备指令";
        break;
    case ui_voice_state::success:
        title = "已执行";
        hint = "灯光状态已更新\n可再次唤醒继续说";
        break;
    case ui_voice_state::ignored:
        title = "没有匹配的操作";
        hint = "请说开灯或关灯\n可再次唤醒重试";
        break;
    case ui_voice_state::error:
        title = "本次未完成";
        hint = "请检查网络连接\n可再次唤醒重试";
        color = warning;
        break;
    default:
        break;
    }

    lv_label_set_text(voice_title, title);
    lv_obj_set_style_text_color(voice_title, lv_color_hex(color), 0);
    lv_label_set_text(voice_hint, hint);
    lv_label_set_text(transcript, current.transcript[0] ? current.transcript : "还没有语音指令");

    const char *led_text = !current.led_available ? "未启用" : current.led_on ? "已开启" : "已关闭";
    uint32_t led_color = current.led_available && current.led_on ? accent : muted;
    lv_label_set_text(led_value, led_text);
    lv_obj_set_style_text_color(led_value, lv_color_hex(led_color), 0);
    lv_obj_set_style_bg_color(led_dot, lv_color_hex(led_color), 0);

    lv_label_set_text(wifi_value, current.wifi ? "Wi-Fi  已连接" : "Wi-Fi  未连接");
    lv_label_set_text(mqtt_value, current.mqtt ? "MQTT  已连接" : "MQTT  未连接");
    lv_obj_set_style_text_color(wifi_value, lv_color_hex(current.wifi ? accent : muted), 0);
    lv_obj_set_style_text_color(mqtt_value, lv_color_hex(current.mqtt ? accent : muted), 0);

    // 只在状态改变时更新波形，不添加常驻动画和额外任务。
    const int heights[] = {12, 22, 34, 44, 30, 20, 12};
    bool active = current.voice == ui_voice_state::listening ||
                  current.voice == ui_voice_state::recognizing ||
                  current.voice == ui_voice_state::thinking;
    for (unsigned i = 0; i < 7; i++)
    {
        int height = active ? heights[i] : 8;
        lv_obj_set_height(wave_bars[i], height);
        lv_obj_set_y(wave_bars[i], (48 - height) / 2);
        lv_obj_set_style_bg_color(wave_bars[i], lv_color_hex(active ? accent : muted), 0);
    }
}

void refresh(lv_timer_t *)
{
    if (provisioning)
    {
        return;
    }

    ui_snapshot current;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (state.revision == rendered_revision)
        {
            return;
        }
        current = state;
    }

    render_snapshot(current);
    rendered_revision = current.revision;
}

// 截断时退到完整 UTF-8 字符边界，不能把半个汉字交给 LVGL。
void copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);
    if (length >= capacity)
    {
        length = capacity - 1;
        while (length > 0 && (static_cast<unsigned char>(source[length]) & 0xC0) == 0x80)
        {
            length--;
        }
    }
    memcpy(destination, source, length);
    destination[length] = '\0';
}
} // namespace

void ui_show_dashboard(void)
{
    lvgl_lock_guard lock(lvgl_mutex());
    provisioning = false;
    lv_obj_t *screen = prepare_screen();

    lv_obj_t *voice = make_panel(screen, 14, 60, 212, 112);
    voice_title = make_label(voice, 14, 12, 180, "正在启动", accent);
    voice_hint = make_label(voice, 14, 43, 136, "", muted);
    lv_obj_set_height(voice_hint, 58);

    lv_obj_t *wave = make_panel(voice, 157, 47, 44, 48);
    for (unsigned i = 0; i < 7; i++)
    {
        wave_bars[i] = make_panel(wave, i * 6, 20, 3, 8);
        lv_obj_set_style_radius(wave_bars[i], 2, 0);
    }

    lv_obj_t *led = make_panel(screen, 14, 182, 212, 46);
    led_dot = make_panel(led, 14, 18, 10, 10);
    lv_obj_set_style_radius(led_dot, LV_RADIUS_CIRCLE, 0);
    make_label(led, 34, 12, 75, "灯光 / LED");
    led_value = make_label(led, 128, 12, 70, "未启用", muted);
    lv_obj_set_style_text_align(led_value, LV_TEXT_ALIGN_RIGHT, 0);

    make_label(screen, 14, 239, 212, "最近一句", muted);
    transcript = make_label(screen, 14, 262, 212, "还没有语音指令");
    lv_obj_set_height(transcript, 24);

    wifi_value = make_label(screen, 14, 296, 108, "Wi-Fi  未连接", muted);
    mqtt_value = make_label(screen, 126, 296, 108, "MQTT  未连接", muted);

    // 初次绘制直接使用当前副本，防止服务先启动、页面后创建时丢失状态。
    ui_snapshot current;
    {
        std::lock_guard<std::mutex> state_lock(state_mutex);
        current = state;
    }
    render_snapshot(current);
    rendered_revision = current.revision;

    if (refresh_timer == nullptr)
    {
        refresh_timer = lv_timer_create(refresh, 150, nullptr);
    }
}

void ui_show_provisioning(const char *ssid)
{
    lvgl_lock_guard lock(lvgl_mutex());
    provisioning = true;
    lv_obj_t *screen = prepare_screen();

    lv_obj_t *panel = make_panel(screen, 14, 60, 212, 188);
    make_label(panel, 14, 14, 184, "连接设备热点", accent);
    make_label(panel, 14, 42, 184, ssid);
    make_label(panel, 14, 82, 184, "浏览器打开", muted);
    make_label(panel, 14, 110, 184, "https://192.168.4.1");
    lv_obj_t *hint = make_label(panel, 14, 144, 184, "保存后重启设备", muted);
    lv_obj_set_height(hint, 24);

    make_label(screen, 14, 264, 212, "配置 Wi-Fi 和 MQTT", muted);
    make_label(screen, 14, 291, 212, "语音服务将在联网后启动", muted);
}

void ui_set_network(bool wifi_connected, bool mqtt_connected)
{
    std::lock_guard<std::mutex> lock(state_mutex);
    if (state.wifi != wifi_connected || state.mqtt != mqtt_connected)
    {
        state.wifi = wifi_connected;
        state.mqtt = mqtt_connected;
        state.revision++;
    }
}

void ui_set_led(bool available, bool on)
{
    std::lock_guard<std::mutex> lock(state_mutex);
    if (state.led_available != available || state.led_on != on)
    {
        state.led_available = available;
        state.led_on = on;
        state.revision++;
    }
}

void ui_set_voice(ui_voice_state voice, const char *text)
{
    std::lock_guard<std::mutex> lock(state_mutex);

    // 云端可能先于本地 VAD 返回最终文本，不能把“正在处理”退回“正在识别”。
    if (voice == ui_voice_state::recognizing && state.voice != ui_voice_state::listening)
    {
        return;
    }

    state.voice = voice;
    if (text != nullptr)
    {
        copy_text(state.transcript, sizeof(state.transcript), text);
    }
    state.revision++;
}

void ui_finish_listening(void)
{
    std::lock_guard<std::mutex> lock(state_mutex);
    if (state.voice == ui_voice_state::listening || state.voice == ui_voice_state::recognizing)
    {
        state.voice = ui_voice_state::error;
        state.revision++;
    }
}
