#include "font.h"
#include "ssd1306_fonts.h"

const SSD1306_Font_t *OLED_Font_Get(uint8_t font_size)
{
    if (font_size <= 8U)
    {
        return &Font_6x8;
    }
    if (font_size <= 10U)
    {
        return &Font_7x10;
    }
    if (font_size <= 18U)
    {
        return &Font_11x18;
    }
    return &Font_16x26;
}
