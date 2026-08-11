//
// Created by YZH on 2026/8/11.
//
#include "motor.h"
#include "pwm.h"
#include "gpio.h"

/**
 * @brief 电机初始化
 */

void Motor_Init(void)
{
    PWM_Init();

    PWM_SetCompare1(0);
    PWM_SetCompare2(0);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
}

/**
 * @brief 设置电机PWM
 * @param channel   1=左电机，2=右电机
 * @param PWM -100~100
 */
void Motor_SetPWM(uint8_t channel, int16_t PWM)
{
    if (channel == 1)
    {
        if (PWM >= 0)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);

            PWM_SetCompare1(PWM);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);

            PWM_SetCompare1(-PWM);
        }
    }

    else if (channel == 2)
    {
        if (PWM >= 0)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);

            PWM_SetCompare2(PWM);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);

            PWM_SetCompare2(-PWM);
        }
    }
}