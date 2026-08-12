#include "Blueserial.h"
#include "usart.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>


char BlueSerial_RxPacket[100];
volatile uint8_t BlueSerial_RxFlag = 0;

static volatile uint8_t BlueSerial_RxData;
static uint8_t BlueSerial_RxState;
static uint8_t BlueSerial_RxIndex;

#define BLUE_SERIAL_TX_BUFFER_SIZE 512U

static uint8_t BlueSerial_TxBuffer[BLUE_SERIAL_TX_BUFFER_SIZE];
static volatile uint16_t BlueSerial_TxHead;
static volatile uint16_t BlueSerial_TxTail;
static volatile uint16_t BlueSerial_TxDMALength;
static volatile uint8_t BlueSerial_TxDMABusy;

/**
 * @brief 启动一次蓝牙串口DMA发送
 */

static void BlueSerial_TxStartDMA(void)
{
    uint16_t Length;

    if ((BlueSerial_TxDMABusy != 0U) ||
        (BlueSerial_TxHead == BlueSerial_TxTail))
    {
        return;
    }

    if (BlueSerial_TxHead > BlueSerial_TxTail)
    {
        Length = BlueSerial_TxHead - BlueSerial_TxTail;
    }
    else
    {
        Length = BLUE_SERIAL_TX_BUFFER_SIZE - BlueSerial_TxTail;
    }

    BlueSerial_TxDMALength = Length;
    BlueSerial_TxDMABusy = 1U;

    if (HAL_UART_Transmit_DMA(&huart2,
        &BlueSerial_TxBuffer[BlueSerial_TxTail], Length) != HAL_OK)
    {
        BlueSerial_TxDMABusy = 0U;
        BlueSerial_TxDMALength = 0U;
    }
}

/**
 * @brief 将待发送数据写入蓝牙串口发送缓冲区
 */

static void BlueSerial_TxQueue(const uint8_t *Data, uint16_t Length)
{
    uint32_t FreeLength;
    uint32_t Primask;

    if ((Data == NULL) || (Length == 0U))
    {
        return;
    }

    Primask = __get_PRIMASK();
    __disable_irq();

    if (BlueSerial_TxHead >= BlueSerial_TxTail)
    {
        FreeLength = BLUE_SERIAL_TX_BUFFER_SIZE -
                     (BlueSerial_TxHead - BlueSerial_TxTail) - 1U;
    }
    else
    {
        FreeLength = BlueSerial_TxTail - BlueSerial_TxHead - 1U;
    }

    /* 缓冲区空间不足时丢弃本次完整数据，避免发送半包 */
    if (Length <= FreeLength)
    {
        for (uint16_t i = 0; i < Length; i++)
        {
            BlueSerial_TxBuffer[BlueSerial_TxHead] = Data[i];
            BlueSerial_TxHead++;

            if (BlueSerial_TxHead >= BLUE_SERIAL_TX_BUFFER_SIZE)
            {
                BlueSerial_TxHead = 0U;
            }
        }

        BlueSerial_TxStartDMA();
    }

    if (Primask == 0U)
    {
        __enable_irq();
    }
}

/**
 * @brief 蓝牙串口初始化
 */

HAL_StatusTypeDef BlueSerial_Init(void)
{
    BlueSerial_RxFlag = 0;
    BlueSerial_RxState = 0;
    BlueSerial_RxIndex = 0;
    BlueSerial_RxPacket[0] = '\0';
    BlueSerial_TxHead = 0U;
    BlueSerial_TxTail = 0U;
    BlueSerial_TxDMALength = 0U;
    BlueSerial_TxDMABusy = 0U;
    return HAL_UART_Receive_IT(&huart2, (uint8_t *)&BlueSerial_RxData, 1);
}

/**
 * @brief 蓝牙串口发送一个字节
 */

void BlueSerial_SendByte(uint8_t Byte)
{
    BlueSerial_TxQueue(&Byte, 1U);
}

/**
 * @brief 蓝牙串口发送数组
 */

void BlueSerial_SendArray(const uint8_t *Array, uint16_t Length)
{
    BlueSerial_TxQueue(Array, Length);
}

/**
 * @brief 蓝牙串口发送字符串
 */

void BlueSerial_SendString(const char *String)
{
    size_t Length;

    if (String == NULL)
    {
        return;
    }

    Length = strlen(String);

    if (Length <= UINT16_MAX)
    {
        BlueSerial_TxQueue((const uint8_t *)String, (uint16_t)Length);
    }
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
        uint8_t Digit = (uint8_t)(Number /
            BlueSerial_Pow(10U, (uint32_t)(Length - i - 1U)) % 10U);

        BlueSerial_SendByte((uint8_t)(Digit + (uint8_t)'0'));
    }
}

/**
 * @brief 蓝牙串口格式化输出
 */

void BlueSerial_Printf(const char *format, ...)
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

void BlueSerial_RxIRQHandler(void)
{
    uint8_t RxData = BlueSerial_RxData;

    if (BlueSerial_RxState == 0)
    {
        if ((RxData == '[') && (BlueSerial_RxFlag == 0))
        {
            BlueSerial_RxState = 1;
            BlueSerial_RxIndex = 0;
        }
    }
    else if (RxData == ']')
    {
        BlueSerial_RxState = 0;
        BlueSerial_RxPacket[BlueSerial_RxIndex] = '\0';
        BlueSerial_RxFlag = 1;
    }
    else if (BlueSerial_RxIndex < sizeof(BlueSerial_RxPacket) - 1U)
    {
        BlueSerial_RxPacket[BlueSerial_RxIndex++] = (char)RxData;
    }
    else
    {
        BlueSerial_RxState = 0;
        BlueSerial_RxIndex = 0;
        BlueSerial_RxPacket[0] = '\0';
    }

    (void)HAL_UART_Receive_IT(&huart2, (uint8_t *)&BlueSerial_RxData, 1);
}

/**
 * @brief 蓝牙串口DMA发送完成处理
 */

void BlueSerial_TxIRQHandler(void)
{
    BlueSerial_TxTail += BlueSerial_TxDMALength;

    if (BlueSerial_TxTail >= BLUE_SERIAL_TX_BUFFER_SIZE)
    {
        BlueSerial_TxTail = (uint16_t)(BlueSerial_TxTail -
                                      BLUE_SERIAL_TX_BUFFER_SIZE);
    }

    BlueSerial_TxDMALength = 0U;
    BlueSerial_TxDMABusy = 0U;
    BlueSerial_TxStartDMA();
}

/**
 * @brief 蓝牙串口DMA发送错误处理
 */

void BlueSerial_TxErrorIRQHandler(void)
{
    BlueSerial_TxTail += BlueSerial_TxDMALength;

    if (BlueSerial_TxTail >= BLUE_SERIAL_TX_BUFFER_SIZE)
    {
        BlueSerial_TxTail = (uint16_t)(BlueSerial_TxTail -
                                      BLUE_SERIAL_TX_BUFFER_SIZE);
    }

    BlueSerial_TxDMALength = 0U;
    BlueSerial_TxDMABusy = 0U;
    BlueSerial_TxStartDMA();
}

/**
 * @brief USART2 接收错误恢复
 */

void BlueSerial_RxErrorIRQHandler(void)
{
    BlueSerial_RxState = 0U;
    BlueSerial_RxIndex = 0U;

    if (BlueSerial_RxFlag == 0U)
    {
        BlueSerial_RxPacket[0] = '\0';
    }

    if (huart2.RxState == HAL_UART_STATE_READY)
    {
        (void)HAL_UART_Receive_IT(
            &huart2,
            (uint8_t *)&BlueSerial_RxData,
            1U
        );
    }
}
