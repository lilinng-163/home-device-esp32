#include "key.h"

void key::init(gpio_num_t num, gpio_config_t &cfg)
{
    cfg_ = cfg;
    num_ = num;
    gpio_config(&cfg_);          // ★ 真正把引脚配置成输入/上拉
}

bool key::get_status(void)
{
    return gpio_get_level(num_) == 0;   // 低电平（按键按下/接地）= true
}
