#pragma once

class lcd;
class io_ex;
class key;

struct board_t
{
    int i2c0_scl;
    int i2c0_sda;
    int xl9555_addr;

    int lcd_pwr_bit;
    int lcd_rst_bit;
    int lcd_bl_bit;

    int lcd_sck;
    int lcd_mosi;
    int lcd_cs;
    int lcd_dc;

    int key_gpio;
};

const board_t &board(void);

// 建总线 + 挂芯片/外设 + 接线（谁挂在谁上）
void board_init();

// 取初始化好的硬件对象
lcd    &board_lcd();
io_ex  &board_ioex();
key    &board_key();
