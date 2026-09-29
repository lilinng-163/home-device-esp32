set(src src/wifi.cpp src/mqtt.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    REQUIRES esp_event esp_wifi espressif__mqtt
                    PRIV_REQUIRES esp_netif esp_rom esp_stdio esp_system nvs_flash cxx)
