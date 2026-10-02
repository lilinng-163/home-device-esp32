set(src src/mqtt_custom.cpp src/lv_disp.cpp src/nvs_config.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    PRIV_REQUIRES net device freertos espressif__cjson lvgl__lvgl esp_timer nvs_flash cxx)
