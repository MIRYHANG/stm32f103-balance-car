//
// Created by YZH on 2026/8/11.
//

#ifndef MPU_PWM_H
#define MPU_PWM_H
#include <stdint.h>

void PWM_Init(void);
void PWM_SetCompare1(uint16_t compare);
void PWM_SetCompare2(uint16_t compare);

#endif //MPU_PWM_H
