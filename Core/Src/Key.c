//
// Created by YZH on 2026/8/11.
//
#include "Key.h"
#include "main.h"
#include "tim.h"

static volatile uint8_t Key_Number = 0;

HAL_StatusTypeDef Key_Init(void)
{
    Key_Number = 0;
    return HAL_OK;
}

uint8_t Key_GetNum(void)
{
    uint8_t temp;
    if (Key_Number)
    {
        temp = Key_Number;
        Key_Number = 0;
        return temp;
    }
    return 0;
}

/**
 * @brief 返回键码
 *
 */

static uint8_t Key_GetState(void)
{
    if (HAL_GPIO_ReadPin(Key1_GPIO_Port, Key1_Pin) == GPIO_PIN_RESET)
    {
        return 1;
    }
    if (HAL_GPIO_ReadPin(Key2_GPIO_Port, Key2_Pin) == GPIO_PIN_RESET)
    {
        return 2;
    }
    if (HAL_GPIO_ReadPin(Key3_GPIO_Port, Key3_Pin) == GPIO_PIN_RESET)
    {
        return 3;
    }
    if (HAL_GPIO_ReadPin(Key4_GPIO_Port, Key4_Pin) == GPIO_PIN_RESET)
    {
        return 4;
    }
    return 0;
}

/**
 * @brief 用于驱动按键模块运行的自定义按键定时中断函数
 * @attention 1ms运行一次
 */

void Key_Tick(void)
{
    static uint8_t count;
    static uint8_t CurrState, PrevState;

    count ++;
    if (count >= 20)
    {
        count = 0;
        PrevState = CurrState;
        CurrState = Key_GetState();

        if (CurrState == 0 && PrevState != 0)
        {
            Key_Number = PrevState;
        }
    }
}
