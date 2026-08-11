//
// Created by YZH on 2026/8/11.
//
#include "Key.h"
#include <sys/types.h>
#include "stm32f1xx_hal_gpio.h"

uint8_t Key_Number = 0;

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

uint8_t Key_GetState(void)
{
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET)
    {
        return 1;
    }
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_RESET)
    {
        return 2;
    }
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET)
    {
        return 3;
    }
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET)
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