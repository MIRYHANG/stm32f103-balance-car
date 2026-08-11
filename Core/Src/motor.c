//
// Created by YZH on 2026/8/11.
//
#include "motor.h"
#include "pwm.h"
#include "main.h"
#include "tim.h"

#define MOTOR_PWM_PERCENT_MAX 100

static uint16_t Motor_PercentToCompare(int16_t pwm)
{
    uint16_t magnitude;
    uint32_t period_counts = __HAL_TIM_GET_AUTORELOAD(&htim2) + 1U;

    if (period_counts > UINT16_MAX)
    {
        period_counts = UINT16_MAX;
    }

    if (pwm > MOTOR_PWM_PERCENT_MAX)
    {
        pwm = MOTOR_PWM_PERCENT_MAX;
    }
    else if (pwm < -MOTOR_PWM_PERCENT_MAX)
    {
        pwm = -MOTOR_PWM_PERCENT_MAX;
    }

    magnitude = (uint16_t)((pwm < 0) ? -pwm : pwm);
    return (uint16_t)(((uint32_t)magnitude * period_counts) /
                      MOTOR_PWM_PERCENT_MAX);
}

/**
 * @brief 电机初始化
 */

void Motor_Init(void)
{
    PWM_Init();

    PWM_SetCompare1(0);
    PWM_SetCompare2(0);

    HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
}

/**
 * @brief 设置电机PWM
 * @param channel   1=左电机，2=右电机
 * @param PWM -100~100
 */
void Motor_SetPWM(uint8_t channel, int16_t PWM)
{
    uint16_t compare = Motor_PercentToCompare(PWM);

    if (channel == 1)
    {
        PWM_SetCompare1(0);

        if (PWM == 0)
        {
            HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
        }
        else if (PWM > 0)
        {
            HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_SET);
        }

        PWM_SetCompare1(compare);
    }

    else if (channel == 2)
    {
        PWM_SetCompare2(0);

        if (PWM == 0)
        {
            HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
        }
        else if (PWM > 0)
        {
            HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_SET);
        }
        else
        {
            HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
        }

        PWM_SetCompare2(compare);
    }
}
