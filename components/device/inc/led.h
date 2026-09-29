#pragma once
#include <string>
#include <cstring>

#include <driver/gpio.h>

class led
{
public:
    void init(gpio_num_t num, gpio_config_t &gpio_cfg);
    void on(void);
    void off(void);
    bool get_status(void);
private:
    bool status = false;
    gpio_config_t gpio_cfg_;
    gpio_num_t num_;
};