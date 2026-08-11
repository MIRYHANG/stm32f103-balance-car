//
// Created by YZH on 2026/8/10.
//
#include "i2c.h"
#ifndef MPU_MPU6050_H
#define MPU_MPU6050_H

//从设备地址
/* STM32 HAL expects the 7-bit I2C address shifted left by one bit. */
#define MPU6050_ADDRESS (0x68U << 1)

extern int16_t AccX;
extern int16_t AccY;
extern int16_t AccZ;

extern int16_t GyroX;
extern int16_t GyroY;
extern int16_t GyroZ;
//初始化mpu6050芯片
HAL_StatusTypeDef MPU6050_Init(void);

HAL_StatusTypeDef MPU6050_GetID(uint8_t *id);

/* 获取六轴原始数据 */
HAL_StatusTypeDef MPU6050_GetData(void);

#endif //MPU_MPU6050_H
