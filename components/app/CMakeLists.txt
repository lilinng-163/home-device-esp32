set(src src/mqtt_custom.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    PRIV_REQUIRES net device freertos espressif__cjson cxx)
