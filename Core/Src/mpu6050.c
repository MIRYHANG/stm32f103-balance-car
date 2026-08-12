//
// Created by YZH on 2026/8/10.
//
#include "mpu6050.h"
#include <math.h>

volatile int16_t AccX, AccY, AccZ;
volatile int16_t GyroX, GyroY, GyroZ;

volatile float AngleAcc;
volatile float AngleGyro;
volatile float Angle;

static uint8_t MPU6050_RxBuffer[14];
static volatile uint8_t MPU6050_ReadBusy;

/**
 * @ brief 解析六轴传感器的原始数据
 */

static void MPU6050_ParseData(const uint8_t *Data)
{
    AccX = (int16_t)(((uint16_t)Data[0] << 8) | Data[1]);
    AccY = (int16_t)(((uint16_t)Data[2] << 8) | Data[3]);
    AccZ = (int16_t)(((uint16_t)Data[4] << 8) | Data[5]);

    GyroX = (int16_t)(((uint16_t)Data[8] << 8) | Data[9]);
    GyroY = (int16_t)(((uint16_t)Data[10] << 8) | Data[11]);
    GyroZ = (int16_t)(((uint16_t)Data[12] << 8) | Data[13]);
}

/**
 * @ brief 计算俯仰角并执行互补滤波
 * @ attention 每隔1ms执行一次
 */

static void MPU6050_UpdateAngle(void)
{
    const float Dt = 0.001f;
    const float Alpha = 0.001f;

    AngleAcc = -atan2f((float)AccX, (float)AccZ)
               * (180.0f / 3.1415926f);

    AngleGyro = Angle
                + (float)GyroY
                * (2000.0f / 32768.0f)
                * Dt;

    Angle = Alpha * AngleAcc
            + (1.0f - Alpha) * AngleGyro;
}

/**
 * @ brief 写寄存器地址
 *
 * @ param reg 寄存器地址
 * @ param data 寄存器的值
 */

static HAL_StatusTypeDef MPU6050_Write_Reg(const uint8_t reg, uint8_t data)
{
    //1.操作句柄 2.从设备地址 3.寄存器地址 4.寄存器地址的位数 5.写入的数据地址 6.写入的字节数 7.超时时间
    return HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDRESS,
        reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

/**
 * @ brief 读寄存器地址
 *
 * @ param reg 寄存器地址
 * @ param data 寄存器的值
 */

static HAL_StatusTypeDef MPU6050_Read_Reg(const uint8_t reg, uint8_t *data)
{
    //1.操作句柄 2.从设备地址 3.寄存器地址 4.寄存器地址的位数 5.读入的数据地址 6.读入的字节数 7.超时时间
    return HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDRESS,
        reg, I2C_MEMADD_SIZE_8BIT, data, 1, 100);
}

/**
 *
 * @ brief WHO_AM_I 检测是否正常通信
 */

HAL_StatusTypeDef MPU6050_GetID(uint8_t *id)
{
    if (id == 0)
    {
        return HAL_ERROR;
    }

    *id = 0;

    return MPU6050_Read_Reg(0x75, id);
}

/**
 * @ brief 读取多个寄存器地址
 * @ param reg 寄存器地址
 * @ param data 传输数据
 * @ param len 传输数据长度
 */

static HAL_StatusTypeDef MPU6050_Read_Regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    return HAL_I2C_Mem_Read(
        &hi2c2,
        MPU6050_ADDRESS,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        data,
        len,
        100
    );
}

/**
 * @ brief 初始化mpu6050
 */

HAL_StatusTypeDef MPU6050_Init(void)
{
    //重启复位芯片 配置电源管理寄存器
    if (MPU6050_Write_Reg(0x6B,0x80) != HAL_OK)
    {
        return HAL_ERROR;
    }
    uint8_t data = 0;
    //重置完成之后,0x6B寄存器的值重置为0x40,表示低功耗模式
    uint32_t StartTime = HAL_GetTick();

    while (data != 0x40)
    {
        if (MPU6050_Read_Reg(0x6B, &data) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (HAL_GetTick() - StartTime > 200)
        {
            return HAL_TIMEOUT;
        }
    }

    uint8_t id;

    if (MPU6050_GetID(&id) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (id != 0x68)
    {
        return HAL_ERROR;
    }

    //唤醒mpu6050
    if (MPU6050_Write_Reg(0x6B,0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //配置角速度量程 ±2000°/s
    if (MPU6050_Write_Reg(0x1B,3 << 3) != HAL_OK)   //本质在Bit3写入3 选择对应的量程
    {
        return HAL_ERROR;
    }

    //配置加速度量程±16g
    if (MPU6050_Write_Reg(0x1C,3 << 3) != HAL_OK)    //本质在Bit3写入3 选择对应的量程
    {
        return HAL_ERROR;
    }

    //关闭中断使能 因为用不到中断
    if (MPU6050_Write_Reg(0x38,0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //用户配置寄存器 不使用FIFO队列 不使用扩展I2C
    if (MPU6050_Write_Reg(0x6A,0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //设置采样频率：DLPF开启时，1kHz / (1 + SMPLRT_DIV) = 1kHz
    if (MPU6050_Write_Reg(0x19,0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //设置低通滤波的值为184Hz 188Hz
    if (MPU6050_Write_Reg(0x1A,1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //配置使用的系统时钟为添加PLL
    if (MPU6050_Write_Reg(0x6B,0x01) != HAL_OK)
    {
        return HAL_ERROR;
    }

    //使能加速度传感器 角速度传感器
    if (MPU6050_Write_Reg(0x6C,0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @ brief 读取六轴传感器的值
 */

HAL_StatusTypeDef MPU6050_GetData(void)
{
    uint8_t Data[14] = {0};

    if (MPU6050_Read_Regs(0x3B, Data, 14) != HAL_OK)
    {
        return HAL_ERROR;
    }

    MPU6050_ParseData(Data);

    return HAL_OK;
}

/**
 * @ brief 启动一次六轴数据中断读取
 */

HAL_StatusTypeDef MPU6050_StartReadIT(void)
{
    HAL_StatusTypeDef Status;

    if (MPU6050_ReadBusy != 0U)
    {
        return HAL_BUSY;
    }

    MPU6050_ReadBusy = 1U;

    Status = HAL_I2C_Mem_Read_IT(
        &hi2c2,
        MPU6050_ADDRESS,
        0x3B,
        I2C_MEMADD_SIZE_8BIT,
        MPU6050_RxBuffer,
        sizeof(MPU6050_RxBuffer)
    );

    if (Status != HAL_OK)
    {
        MPU6050_ReadBusy = 0U;
    }

    return Status;
}

/**
 * @ brief I2C读取完成处理
 */

void MPU6050_ReadCompleteIRQHandler(void)
{
    MPU6050_ParseData(MPU6050_RxBuffer);
    MPU6050_UpdateAngle();
    MPU6050_ReadBusy = 0U;
}

/**
 * @ brief I2C读取错误处理
 */

void MPU6050_ReadErrorIRQHandler(void)
{
    MPU6050_ReadBusy = 0U;
}
