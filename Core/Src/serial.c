//
// Created by YZH on 2026/8/11.
//
#include "serial.h"
#include <string.h>
#include <stdarg.h>
#include "stm32f1xx_hal_uart.h"
#include "usart.h"

volatile uint8_t Serial_RxData;
volatile uint8_t Serial_RxFlag;

/**
 * @ brief 串口初始化
 */
HAL_StatusTypeDef Serial_Init(void)
{
    Serial_RxFlag = 0;
    return HAL_UART_Receive_IT(&huart1, (uint8_t *)&Serial_RxData, 1);
}

/**
 * @ brief 串口发送一个字节
 */
void Serial_SendByte(uint8_t Byte)
{
    HAL_UART_Transmit(&huart1,&Byte,1,HAL_MAX_DELAY);
}

/**
 * @brief 串口发送数组
 */

void Serial_SendArray(const uint8_t *Array, uint16_t Length)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)Array, Length, HAL_MAX_DELAY);
}

/**
 * @brief 串口发送字符串
 */

void Serial_SendString(const char *String)
{
    HAL_UART_Transmit(&huart1,(uint8_t *)String,strlen(String),HAL_MAX_DELAY);
}

/**
 * @brief 次方函数
 */

uint32_t Serial_Pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1;

    while (y--)
    {
        result *= x;
    }

    return result;
}


/**
 * @brief 串口发送数字
 */

void Serial_SendNumber(uint32_t number, uint8_t length)
{
    uint8_t i;

    for (i = 0; i < length; i++)
    {
        Serial_SendByte(number / Serial_Pow(10, length - i - 1) % 10 + '0');
    }
}


/**
 * @brief printf 重定向 把所有printf都转化为串口输出
 */

int fputc(int ch, FILE *f)
{
    uint8_t temp = (uint8_t)ch;

    HAL_UART_Transmit(&huart1,&temp,1,HAL_MAX_DELAY);

    return ch;
}

/**
 * @brief 自定义 Serial_Printf
 */

void Serial_Printf(const char *format, ...)  //不定参数 可以传入不同数据类型
{
    char String[100];

    va_list arg;   //va_list专门处理不定参数

    va_start(arg, format);

    vsnprintf(String,sizeof(String),format,arg);

    va_end(arg);

    Serial_SendString(String);
}


/**
 * @brief 获取接收标志
 */

uint8_t Serial_GetRxFlag(void)
{
    if (Serial_RxFlag == 1)
    {
        Serial_RxFlag = 0;

        return 1;
    }

    return 0;
}

/**
 * @brief 获取接收到的数据
 */

uint8_t Serial_GetRxData(void)
{
    return Serial_RxData;
}

/**
 * @brief HAL 串口接收完成回调
 */

void Serial_RxIRQHandler(void)
{
    Serial_RxFlag = 1;
    (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&Serial_RxData, 1);
}
