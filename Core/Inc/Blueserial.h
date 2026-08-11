#ifndef __BLUE_SERIAL_H
#define __BLUE_SERIAL_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

extern char BlueSerial_RxPacket[];
extern volatile uint8_t BlueSerial_RxFlag;

void BlueSerial_SendByte(uint8_t Byte);
HAL_StatusTypeDef BlueSerial_Init(void);
void BlueSerial_SendArray(const uint8_t *Array, uint16_t Length);
void BlueSerial_SendString(const char *String);
void BlueSerial_SendNumber(uint32_t Number, uint8_t Length);
void BlueSerial_Printf(const char *format, ...);
uint8_t BlueSerial_GetRxFlag(void);
void BlueSerial_RxIRQHandler(void);

#endif
