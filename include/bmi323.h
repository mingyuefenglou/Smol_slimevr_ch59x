/**
 * @file bmi323.h
 * @brief BMI323 IMU Driver Header
 */

#ifndef __BMI323_H__
#define __BMI323_H__

#include <stdint.h>
#include <stdbool.h>

// WHO_AM_I 值
#define BMI323_DEVICE_ID        0x0043

// I2C 地址
#define BMI323_I2C_ADDR_0       0x68    // SDO = GND
#define BMI323_I2C_ADDR_1       0x69    // SDO = VDD

// ODR 值
#define BMI323_ODR_0_78Hz       0x01
#define BMI323_ODR_1_5Hz        0x02
#define BMI323_ODR_3_1Hz        0x03
#define BMI323_ODR_6_25Hz       0x04
#define BMI323_ODR_12_5Hz       0x05
#define BMI323_ODR_25Hz         0x06
#define BMI323_ODR_50Hz         0x07
#define BMI323_ODR_100Hz        0x08
#define BMI323_ODR_200Hz        0x09
#define BMI323_ODR_400Hz        0x0A
#define BMI323_ODR_800Hz        0x0B
#define BMI323_ODR_1600Hz       0x0C
#define BMI323_ODR_3200Hz       0x0D
#define BMI323_ODR_6400Hz       0x0E

// 加速度量程
#define BMI323_ACC_RANGE_2G     0x00
#define BMI323_ACC_RANGE_4G     0x01
#define BMI323_ACC_RANGE_8G     0x02
#define BMI323_ACC_RANGE_16G    0x03

// 陀螺仪量程
#define BMI323_GYR_RANGE_125DPS     0x00
#define BMI323_GYR_RANGE_250DPS     0x01
#define BMI323_GYR_RANGE_500DPS     0x02
#define BMI323_GYR_RANGE_1000DPS    0x03
#define BMI323_GYR_RANGE_2000DPS    0x04

/**
 * @brief 初始化 BMI323
 * @param use_spi true: SPI, false: I2C
 * @param i2c_addr I2C 地址 (0x68 或 0x69)
 * @return 0: 成功, <0: 错误
 */
int bmi323_init(bool use_spi, uint8_t i2c_addr);

/**
 * @brief 配置加速度计
 * @param odr ODR 设置
 * @param range 量程设置
 * @return 0: 成功
 */
int bmi323_set_accel_config(uint8_t odr, uint8_t range);

/**
 * @brief 配置陀螺仪
 * @param odr ODR 设置
 * @param range 量程设置
 * @return 0: 成功
 */
int bmi323_set_gyro_config(uint8_t odr, uint8_t range);

/**
 * @brief 检查数据就绪
 * @return true: 数据可读
 */
bool bmi323_data_ready(void);

/**
 * @brief 读取原始数据
 * @param gyro 陀螺仪输出 [x, y, z] (LSB)
 * @param accel 加速度输出 [x, y, z] (LSB)
 * @return 0: 成功
 */
int bmi323_read_raw(int16_t gyro[3], int16_t accel[3]);

/**
 * @brief 读取物理单位数据
 * @param gyro 陀螺仪输出 [x, y, z] (rad/s)
 * @param accel 加速度输出 [x, y, z] (g)
 * @return 0: 成功
 */
int bmi323_read_all(float gyro[3], float accel[3]);

/**
 * @brief 读取温度
 * @return 温度 (°C)
 */
int16_t bmi323_read_temp(void);

/**
 * @brief 进入低功耗模式
 */
void bmi323_power_down(void);

/**
 * @brief 退出低功耗模式
 */
void bmi323_power_up(void);

#endif /* __BMI323_H__ */
