#include "io_ex.h"

#include <esp_io_expander_xl9555.h>   // esp_io_expander_new_i2c_xl9555

int xl9555::init(i2c_master_bus_handle_t bus, uint32_t addr)
{
    if (esp_io_expander_new_i2c_xl9555(bus, addr, &handle_) != ESP_OK)
    {
        handle_ = nullptr;
        return -1;
    }
    return 0;
}

int xl9555::set_dir(uint8_t bit, bool input)
{
    if (!handle_)
        return -1;

    esp_err_t err = esp_io_expander_set_dir(
        handle_,
        (uint32_t)1 << bit,
        input ? IO_EXPANDER_INPUT : IO_EXPANDER_OUTPUT);
    return (err == ESP_OK) ? 0 : -1;
}

int xl9555::write_pin(uint8_t bit, bool level)
{
    if (!handle_)
        return -1;

    esp_err_t err = esp_io_expander_set_level(handle_, (uint32_t)1 << bit, level ? 1 : 0);
    return (err == ESP_OK) ? 0 : -1;
}

int xl9555::read_pin(uint8_t bit)
{
    if (!handle_)
        return -1;

    uint32_t mask = 0;
    if (esp_io_expander_get_level(handle_, (uint32_t)1 << bit, &mask) != ESP_OK)
        return -1;

    return (int)((mask >> bit) & 1u);
}
