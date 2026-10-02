set(src src/ui.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    REQUIRES lvgl__lvgl
                    PRIV_REQUIRES cxx app)