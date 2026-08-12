//
// Created by YZH on 2026/8/10.
//
#include "i2c.h"
#ifndef MPU_MPU6050_H
#define MPU_MPU6050_H

//从设备地址
/* STM32 HAL expects the 7-bit I2C address shifted left by one bit. */
#define MPU6050_ADDRESS (0x68U << 1)

extern volatile int16_t AccX;
extern volatile int16_t AccY;
extern volatile int16_t AccZ;

extern volatile int16_t GyroX;
extern volatile int16_t GyroY;
extern volatile int16_t GyroZ;

extern volatile float AngleAcc;
extern volatile float AngleGyro;
extern volatile float Angle;
//初始化mpu6050芯片
HAL_StatusTypeDef MPU6050_Init(void);

HAL_StatusTypeDef MPU6050_GetID(uint8_t *id);

/* 获取六轴原始数据 */
HAL_StatusTypeDef MPU6050_GetData(void);

/* 启动一次中断读取 */
HAL_StatusTypeDef MPU6050_StartReadIT(void);

/* I2C读取完成和错误处理 */
void MPU6050_ReadCompleteIRQHandler(void);
void MPU6050_ReadErrorIRQHandler(void);

#endif //MPU_MPU6050_H
