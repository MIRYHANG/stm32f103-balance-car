//
// Created by YZH on 2026/8/10.
//
#include "mpu6050.h"

int16_t AccX, AccY, AccZ;
int16_t GyroX, GyroY, GyroZ;

/**
 * @ brief 写寄存器地址
 *
 * @ param reg 寄存器地址
 * @ param data 寄存器的值
 */

void MPU6050_Write_Reg(const uint8_t reg, uint8_t data)
{
    //1.操作句柄 2.从设备地址 3.寄存器地址 4.寄存器地址的位数 5.写入的数据地址 6.写入的字节数 7.超时时间
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDRESS,
        reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

/**
 * @ brief 读寄存器地址
 *
 * @ param reg 寄存器地址
 * @ param data 寄存器的值
 */

void MPU6050_Read_Reg(const uint8_t reg, uint8_t *data)
{
    //1.操作句柄 2.从设备地址 3.寄存器地址 4.寄存器地址的位数 5.读入的数据地址 6.读入的字节数 7.超时时间
    HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDRESS,
        reg, I2C_MEMADD_SIZE_8BIT, data, 1, 100);
}

/**
 *
 * @ brief WHO_AM_I 检测是否正常通信
 */

uint8_t MPU6050_GetID(void)
{
    uint8_t id;

    MPU6050_Read_Reg(0x75, &id);

    return id;
}

/**
 * @ brief 读取多个寄存器地址
 * @ param reg 寄存器地址
 * @ param data 传输数据
 * @ param len 传输数据长度
 */

void MPU6050_Read_Regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    HAL_I2C_Mem_Read(
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

void MPU6050_Init(void)
{
    //重启复位芯片 配置电源管理寄存器
    MPU6050_Write_Reg(0x6B,0x80);
    uint8_t data = 0;
    //重置完成之后,0x6B寄存器的值重置为0x40,表示低功耗模式
    while (data != 0x40)
    {
        MPU6050_Read_Reg(0x6B,&data);
    }

    //唤醒mpu6050
    MPU6050_Write_Reg(0x6B,0x00);

    //配置角速度量程 ±2000°/s
    MPU6050_Write_Reg(0x1B,3 << 3);   //本质在Bit3写入3 选择对应的量程

    //配置加速度量程±2g
    MPU6050_Write_Reg(0x1C,0x00);    //本质在Bit3写入0 选择对应的量程

    //关闭中断使能 因为用不到中断
    MPU6050_Write_Reg(0x38,0x00);

    //用户配置寄存器 不使用FIFO队列 不使用扩展I2C
    MPU6050_Write_Reg(0x6A,0x00);

    //设置采样频率 香农定律：采样频率>=2*使用频率 采样分频为2
    MPU6050_Write_Reg(0x19,0x01);

    //设置低通滤波的值为184Hz 188Hz
    MPU6050_Write_Reg(0x1A,1);

    //配置使用的系统时钟为添加PLL
    MPU6050_Write_Reg(0x6B,0x01);

    //使能加速度传感器 角速度传感器
    MPU6050_Write_Reg(0x6C,0x00);
}

/**
 * @ brief 读取六轴传感器的值
 */

void MPU6050_GetData(void)
{
    uint8_t Data[14];

    MPU6050_Read_Regs(0x3B, Data, 14);

    AccX = (int16_t)((Data[0] << 8) | Data[1]);
    AccY = (int16_t)((Data[2] << 8) | Data[3]);
    AccZ = (int16_t)((Data[4] << 8) | Data[5]);

    GyroX = (int16_t)((Data[8] << 8) | Data[9]);
    GyroY = (int16_t)((Data[10] << 8) | Data[11]);
    GyroZ = (int16_t)((Data[12] << 8) | Data[13]);
}
