idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES mbedtls ui board esp_netif esp_rom esp_stdio esp_system esp_wifi nvs_flash cxx net device app cloud lvgl__lvgl)
