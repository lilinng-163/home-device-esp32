set(src src/mqtt_custom.cpp 
    src/lv_disp.cpp 
    src/nvs_config.cpp
    src/cloud_xfei_proc.cpp
    src/device_command.cpp
    src/wakeup_board.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    REQUIRES cloud espressif__esp-sr
                    PRIV_REQUIRES net device esp_driver_gpio freertos espressif__cjson lvgl__lvgl esp_timer nvs_flash esp_netif lwip cxx)
