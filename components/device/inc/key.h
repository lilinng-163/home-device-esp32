#pragma once
#include <string>
#include <cstring>
#include <driver/gpio.h>

class key
{
public:
    void init(gpio_num_t num, gpio_config_t &cfg);
    bool get_status(void);
private:
    gpio_num_t num_;
    gpio_config_t cfg_;
    bool status = false;
};