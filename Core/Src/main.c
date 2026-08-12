/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "Blueserial.h"
#include "serial.h"
#include "Key.h"
#include "motor.h"
#include "encoder.h"
#include "pwm.h"
#include "mpu6050.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint32_t SchedulerTick = 0;

volatile uint8_t TimerErrorFlag;  //定时中断执行超时标志位
volatile uint16_t TimerCount;     //定时中断执行时间对应的计数值
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_I2C2_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();
  OLED_Clear();

  if (MPU6050_Init() != HAL_OK)
  {
    Error_Handler();
  }

  if (BlueSerial_Init() != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    static uint32_t LastOledFrameTick = 0;
    static uint32_t LastOledPageTick = 0;
    static uint8_t OledPage = 8U;
    static uint32_t LastSerialTick = 0;

    uint32_t Now = SchedulerTick;

    /* 50ms任务：OLED显示 */
    if ((OledPage >= 8U) && ((uint32_t)(Now - LastOledFrameTick) >= 50U))
    {
      int16_t DisplayAccX;
      int16_t DisplayAccY;
      int16_t DisplayAccZ;
      int16_t DisplayGyroX;
      int16_t DisplayGyroY;
      int16_t DisplayGyroZ;
      uint8_t DisplayTimerErrorFlag;
      uint16_t DisplayTimerCount;
      uint32_t Primask;

      LastOledFrameTick = Now;

      /* 先保存同一次采样的数据，避免显示过程中六轴数据前后不一致 */
      Primask = __get_PRIMASK();
      __disable_irq();

      DisplayAccX = AccX;
      DisplayAccY = AccY;
      DisplayAccZ = AccZ;

      DisplayGyroX = GyroX;
      DisplayGyroY = GyroY;
      DisplayGyroZ = GyroZ;

      DisplayTimerErrorFlag = TimerErrorFlag;
      DisplayTimerCount = TimerCount;
      if (Primask == 0U)
      {
        __enable_irq();
      }

      OLED_Printf(0, 0, OLED_8X16, "%+06d", DisplayAccX);
      OLED_Printf(0, 16, OLED_8X16, "%+06d", DisplayAccY);
      OLED_Printf(0, 32, OLED_8X16, "%+06d", DisplayAccZ);

      OLED_Printf(64, 0, OLED_8X16, "%+06d", DisplayGyroX);
      OLED_Printf(64, 16, OLED_8X16, "%+06d", DisplayGyroY);
      OLED_Printf(64, 32, OLED_8X16, "%+06d", DisplayGyroZ);

      OLED_Printf(0, 48, OLED_8X16, "Flag:%1d", DisplayTimerErrorFlag);
      OLED_Printf(64, 48, OLED_8X16, "C:%05d", DisplayTimerCount);

      OledPage = 0U;
    }



    /* 5ms任务：蓝牙串口发送角度 */
    if ((uint32_t)(Now - LastSerialTick) >= 5U)
    {
      float SendAngleAcc;
      float SendAngleGyro;
      float SendAngle;
      uint32_t Primask;

      LastSerialTick = Now;

      /* 三个角度必须来自同一次滤波计算 */
      Primask = __get_PRIMASK();     /*记录中断状态*/
      __disable_irq();
      SendAngleAcc = AngleAcc;
      SendAngleGyro = AngleGyro;
      SendAngle = Angle;
      if (Primask == 0U)  /*如果原来中断是开的,恢复成开  如果原来本来就是关闭的,不要擅自打开*/
      {
        __enable_irq();
      }

      BlueSerial_Printf("[plot,%f,%f,%f]", SendAngleAcc, SendAngleGyro, SendAngle);
    }

    /* 每次只更新OLED的一页 */
    if ((OledPage < 8U) &&
        ((uint32_t)(Now - LastOledPageTick) >= 1U))
    {
      LastOledPageTick = Now;

      OLED_UpdatePage(OledPage);
      OledPage++;
    }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    Key_Tick();
    SchedulerTick++;

    /* 每1ms启动一次MPU6050中断读取，启动后立即退出，不等待I2C传输 */
    (void)MPU6050_StartReadIT();

    /* 中断函数退出前，再次检查标志位 */
    /* 如果标志位又置1了，说明中断函数执行时间超过了定时时间（1ms）*/
    if (__HAL_TIM_GET_FLAG(htim, TIM_FLAG_UPDATE) != RESET)
    {
      /* 置TimerErrorFlag为1，表示定时中断错误 */
      TimerErrorFlag = 1;

      /* 清标志位，避免中断连续触发，导致主函数完全无法执行 */
      __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);
    }

    /* 中断函数退出前，读取计数器的值，此值可用于测量中断函数的具体执行时间 */
    TimerCount = __HAL_TIM_GET_COUNTER(htim);
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
