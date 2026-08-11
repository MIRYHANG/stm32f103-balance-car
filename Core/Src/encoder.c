//
// Created by YZH on 2026/8/11.
//
#include "encoder.h"

#include "stm32f1xx_hal_tim.h"
#include "tim.h"

/**
 * @brief 编码器初始化
 */

void Encoder_Init(void)
{
    HAL_TIM_Encoder_Start(&htim3,TIM_CHANNEL_1);
    HAL_TIM_Encoder_Start(&htim3,TIM_CHANNEL_2);

    HAL_TIM_Encoder_Start(&htim4,TIM_CHANNEL_1);
    HAL_TIM_Encoder_Start(&htim4,TIM_CHANNEL_1);

    __HAL_TIM_SET_COUNTER(&htim3, 0);
    __HAL_TIM_SET_COUNTER(&htim4, 0);

}

/**
 * @ brief 获取编码器增量
 * @ param n 1=左电机，2=右电机
 * @ return 从上一次读取到现在的编码器增量
 */

int16_t Encoder_Get(uint8_t n)
{
    int16_t temp;

    if (n == 1)
    {
        temp = (int16_t)__HAL_TIM_GET_COUNTER(&htim3);

        __HAL_TIM_SET_COUNTER(&htim3, 0);

        return temp;
    }

    else if (n == 2)
    {
        temp = (int16_t)__HAL_TIM_GET_COUNTER(&htim4);

        __HAL_TIM_SET_COUNTER(&htim4, 0);

        return temp;
    }

    return 0;
}