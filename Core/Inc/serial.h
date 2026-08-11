//
// Created by YZH on 2026/8/11.
//

#ifndef MPU_SERIAL_H
#define MPU_SERIAL_H

#include <stdio.h>
#include <stdint.h>

void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(uint8_t *Array, uint16_t Length);
void Serial_SendString(char *String);
void Serial_SendNumber(uint32_t number, uint8_t length);
void Serial_Printf(char *format, ...);

uint8_t Serial_GetRxFlag(void);
uint8_t Serial_GetRxData(void);

#endif //MPU_SERIAL_H
