set(src
    src/dashboard.cpp
    src/lv_disp.cpp
    fonts/ui_font_chinese_16.c)

idf_component_register(
    SRCS ${src}
    INCLUDE_DIRS inc
    REQUIRES lvgl__lvgl
    PRIV_REQUIRES device esp_timer esp_lcd freertos)
