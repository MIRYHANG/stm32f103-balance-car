//
// Created by YZH on 2026/8/10.
//
#include "i2c.h"
#ifndef MPU_MPU6050_H
#define MPU_MPU6050_H

//从设备地址
/* STM32 HAL expects the 7-bit I2C address shifted left by one bit. */
#define MPU6050_ADDRESS (0x68U << 1)

//初始化mpu6050芯片
void MPU6050_Init(void);

#endif //MPU_MPU6050_H
