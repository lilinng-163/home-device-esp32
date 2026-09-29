set(src src/wifi.cpp src/mqtt.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    PRIV_REQUIRES espressif__mqtt esp_netif esp_rom esp_stdio esp_system esp_wifi nvs_flash cxx)