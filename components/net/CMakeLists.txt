set(src src/wifi.cpp src/mqtt.cpp src/https_server.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    REQUIRES esp_event esp_wifi espressif__mqtt esp_https_server
                    PRIV_REQUIRES esp_netif esp_rom esp_stdio esp_system nvs_flash cxx
                    EMBED_TXTFILES "servercert.pem" "prvtkey.pem" "cacert.pem")
