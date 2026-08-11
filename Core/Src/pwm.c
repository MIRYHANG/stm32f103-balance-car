//
// Created by YZH on 2026/8/11.
//
#include "pwm.h"
#include "tim.h"
#include "stm32f1xx_hal_tim.h"

void PWM_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
}

void PWM_SetCompare1(uint16_t compare)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, compare);
}

void PWM_SetCompare2(uint16_t compare)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, compare);
}