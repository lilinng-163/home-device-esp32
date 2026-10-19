set(src src/mqtt_custom.cpp
    src/nvs_config.cpp
    src/cloud_xfei_proc.cpp
    src/device_command.cpp
    src/wakeup_board.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    REQUIRES cloud espressif__esp-sr
                    PRIV_REQUIRES net device ui esp_driver_gpio freertos espressif__cjson esp_timer nvs_flash esp_netif lwip cxx)
