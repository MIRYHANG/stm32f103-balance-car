#ifndef MPU_FONT_H
#define MPU_FONT_H

#include <stdint.h>
#include "ssd1306.h"

const SSD1306_Font_t *OLED_Font_Get(uint8_t font_size);

#endif /* MPU_FONT_H */
