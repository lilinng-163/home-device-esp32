idf_component_register(SRCS "src/board.cpp"
                    INCLUDE_DIRS "inc"
                    PRIV_REQUIRES device cxx esp_driver_i2c esp_driver_gpio freertos)
