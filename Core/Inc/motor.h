//
// Created by YZH on 2026/8/11.
//

#ifndef MPU_MOTOR_H
#define MPU_MOTOR_H
#include <stdint.h>

void Motor_Init(void);
void Motor_SetPWM(uint8_t channel, int16_t pwm);

#endif //MPU_MOTOR_H
