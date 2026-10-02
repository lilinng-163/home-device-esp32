set(src src/led.cpp src/key.cpp src/lcd.cpp src/io_ex.cpp)
idf_component_register(SRCS ${src}
                    INCLUDE_DIRS inc
                    REQUIRES esp_driver_gpio esp_driver_spi esp_lcd esp_driver_i2c espressif__esp_io_expander
                    PRIV_REQUIRES net cxx jbrilha__esp_lcd_st7789 esp_timer sheldonix__esp_io_expander_xl9555)