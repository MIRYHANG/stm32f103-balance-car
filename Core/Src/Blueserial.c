#include "Blueserial.h"
#include "usart.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>


char BlueSerial_RxPacket[100];
volatile uint8_t BlueSerial_RxFlag = 0;

static uint8_t BlueSerial_RxData;

/**
 * @brief 蓝牙串口初始化
 */

void BlueSerial_Init(void)
{
    HAL_UART_Receive_IT(&huart2,&BlueSerial_RxData,1);
}

/**
 * @brief 蓝牙串口发送一个字节
 */

void BlueSerial_SendByte(uint8_t Byte)
{
    HAL_UART_Transmit(&huart2,&Byte,1,HAL_MAX_DELAY);
}

/**
 * @brief 蓝牙串口发送数组
 */

void BlueSerial_SendArray(uint8_t *Array, uint16_t Length)
{
    HAL_UART_Transmit(&huart2,Array,Length,HAL_MAX_DELAY);
}

/**
 * @brief 蓝牙串口发送字符串
 */

void BlueSerial_SendString(char *String)
{
    HAL_UART_Transmit(&huart2,(uint8_t *)String,strlen(String),HAL_MAX_DELAY);
}

/**
 * @brief 整数次方函数
 */

static uint32_t BlueSerial_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;

    while (Y--)
    {
        Result *= X;
    }

    return Result;
}

/**
 * @brief 蓝牙串口发送数字
 */

void BlueSerial_SendNumber(uint32_t Number, uint8_t Length)
{
    uint8_t i;

    for (i = 0; i < Length; i++)
    {
        BlueSerial_SendByte(
            Number /
            BlueSerial_Pow(10, Length - i - 1)
            % 10 + '0'
        );
    }
}

/**
 * @brief 蓝牙串口格式化输出
 */

void BlueSerial_Printf(char *format, ...)
{
    char String[100];

    va_list arg;

    va_start(arg, format);

    vsnprintf(
        String,
        sizeof(String),
        format,
        arg
    );

    va_end(arg);

    BlueSerial_SendString(String);
}

/**
 * @brief 获取是否收到完整数据包
 */

uint8_t BlueSerial_GetRxFlag(void)
{
    if (BlueSerial_RxFlag == 1)
    {
        BlueSerial_RxFlag = 0;
        return 1;
    }

    return 0;
}

/**
 * @brief HAL串口接收完成回调
 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    static uint8_t RxState = 0;
    static uint8_t pRxPacket = 0;

    if (huart->Instance == USART2)
    {
        uint8_t RxData = BlueSerial_RxData;

        if (RxState == 0)
        {
            if ((RxData == '[') && (BlueSerial_RxFlag == 0))
            {
                RxState = 1;
                pRxPacket = 0;
            }
        }

        else if (RxState == 1)
        {
            if (RxData == ']')
            {
                RxState = 0;

                BlueSerial_RxPacket[pRxPacket] = '\0';

                BlueSerial_RxFlag = 1;
            }

            else
            {

                if (pRxPacket < sizeof(BlueSerial_RxPacket) - 1)
                {
                    BlueSerial_RxPacket[pRxPacket] = RxData;

                    pRxPacket++;
                }

                else
                {
                    RxState = 0;
                    pRxPacket = 0;
                }
            }
        }
        HAL_UART_Receive_IT(&huart2,&BlueSerial_RxData,1);
    }
}