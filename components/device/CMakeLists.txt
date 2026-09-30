set(src src/led.cpp src/key.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    REQUIRES esp_driver_gpio
                    PRIV_REQUIRES net cxx)