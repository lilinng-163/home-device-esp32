idf_component_register(SRCS "src/xfei.cpp"
                    INCLUDE_DIRS "inc"
                    REQUIRES esp_websocket_client esp_event
                    PRIV_REQUIRES net esp_http_client mbedtls espressif__cjson cxx)
