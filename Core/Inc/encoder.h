//
// Created by YZH on 2026/8/11.
//

#ifndef MPU_ENCODER_H
#define MPU_ENCODER_H
#include <stdint.h>

void Encoder_Init(void);
int16_t Encoder_Get(uint8_t n);

#endif //MPU_ENCODER_H
