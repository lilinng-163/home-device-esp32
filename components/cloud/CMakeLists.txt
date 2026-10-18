set(src src/xfei.cpp
        src/llm.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS "inc"
                    REQUIRES esp_websocket_client esp_event esp_http_client espressif__cjson
                    PRIV_REQUIRES net mbedtls esp-tls tcp_transport lwip esp_timer esp_hw_support freertos cxx)
