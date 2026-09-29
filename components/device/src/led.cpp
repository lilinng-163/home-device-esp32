#include "led.h"

void led::init(gpio_num_t num, gpio_config_t &gpio_cfg)
{
    gpio_cfg_ = gpio_cfg;
    num_ = num;
}
void led::on(void)
{
    if(gpio_cfg_.pull_up_en == GPIO_PULLUP_ENABLE)
    {
        gpio_set_level(num_, 0);
    }
    else
    {
        gpio_set_level(num_, 1);
    }
    status = true;
}
void led::off(void)
{
    if(gpio_cfg_.pull_up_en == GPIO_PULLUP_ENABLE)
    {
        gpio_set_level(num_, 1);
    }
    else
    {
        gpio_set_level(num_, 0);
    }
    status = false;

}
bool led::get_status(void)
{
    return status;
}