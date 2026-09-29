set(src src/led.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    PRIV_REQUIRES net esp_driver_gpio cxx)