#include <string>
#include <cstring>

#include <esp_log.h>
#include <lvgl.h>

#include "lv_disp.h"
#include "nvs_config.h"

void start_build(void)
{
    lvgl_lock_guard lk(lvgl_mutex());
    lv_obj_t *screen = lv_screen_active();

    lv_obj_set_style_bg_color(screen, lv_color_hex(0xecbbbd), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "uploading...");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(label);
}

void show_sta_paramters(app_config &cfg)
{
    lvgl_lock_guard lk(lvgl_mutex());
    lv_obj_t *screen = lv_screen_active();

    lv_obj_set_style_bg_color(screen, lv_color_hex(0xecbbbd), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *cont = lv_obj_create(screen);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_center(cont);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_pad_row(cont, 8, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    auto add_line = [cont](const std::string &text)
    {
        lv_obj_t *label = lv_label_create(cont);
        lv_label_set_text(label, text.c_str());
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    };

    add_line("ssid: " + cfg.ssid);
    add_line("pass: " + cfg.pass);
    add_line("host: " + cfg.host);
    add_line("port: " + std::to_string(cfg.port));
    add_line("client_id: " + cfg.client_id);
    add_line("username: " + cfg.username);
    add_line("password: " + cfg.password);

    for(auto &val : cfg.publish)   add_line("publish: " + val);
    for(auto &val : cfg.subscribe) add_line("subscribe: " + val);
}