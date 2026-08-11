//
// Created by YZH on 2026/8/11.
//

#ifndef MPU_KEY_H
#define MPU_KEY_H
#include <stdint.h>
#include "stm32f1xx_hal.h"

HAL_StatusTypeDef Key_Init(void);
uint8_t Key_GetNum(void);
void Key_Tick(void);

#endif //MPU_KEY_H
