/**
 * @file lsm6dso.h
 * @brief LSM6DSO IMU Driver Header
 */

#ifndef __LSM6DSO_H__
#define __LSM6DSO_H__

#include <stdint.h>
#include <stdbool.h>

// WHO_AM_I 值
#define LSM6DSO_DEVICE_ID       0x6C

// I2C 地址
#define LSM6DSO_I2C_ADDR_0      0x6A    // SDO/SA0 = GND
#define LSM6DSO_I2C_ADDR_1      0x6B    // SDO/SA0 = VDD

// ODR 值
#define LSM6DSO_ODR_OFF         0x00
#define LSM6DSO_ODR_12_5Hz      0x01
#define LSM6DSO_ODR_26Hz        0x02
#define LSM6DSO_ODR_52Hz        0x03
#define LSM6DSO_ODR_104Hz       0x04
#define LSM6DSO_ODR_208Hz       0x05
#define LSM6DSO_ODR_416Hz       0x06
#define LSM6DSO_ODR_833Hz       0x07
#define LSM6DSO_ODR_1666Hz      0x08
#define LSM6DSO_ODR_3332Hz      0x09
#define LSM6DSO_ODR_6664Hz      0x0A

// 加速度量程
#define LSM6DSO_XL_FS_2G        0x00
#define LSM6DSO_XL_FS_4G        0x02
#define LSM6DSO_XL_FS_8G        0x03
#define LSM6DSO_XL_FS_16G       0x01

// 陀螺仪量程
#define LSM6DSO_GY_FS_125DPS    0x01
#define LSM6DSO_GY_FS_250DPS    0x00
#define LSM6DSO_GY_FS_500DPS    0x02
#define LSM6DSO_GY_FS_1000DPS   0x04
#define LSM6DSO_GY_FS_2000DPS   0x06

/**
 * @brief 初始化 LSM6DSO
 * @param use_spi true: SPI, false: I2C
 * @param i2c_addr I2C 地址 (0x6A 或 0x6B)
 * @return 0: 成功, <0: 错误
 */
int lsm6dso_init(bool use_spi, uint8_t i2c_addr);

/**
 * @brief 配置加速度计
 * @param odr ODR 设置
 * @param fs 量程设置
 * @return 0: 成功
 */
int lsm6dso_set_accel_config(uint8_t odr, uint8_t fs);

/**
 * @brief 配置陀螺仪
 * @param odr ODR 设置
 * @param fs 量程设置
 * @return 0: 成功
 */
int lsm6dso_set_gyro_config(uint8_t odr, uint8_t fs);

/**
 * @brief 检查数据就绪
 * @return true: 数据可读
 */
bool lsm6dso_data_ready(void);

/**
 * @brief 读取原始数据
 * @param gyro 陀螺仪输出 [x, y, z] (LSB)
 * @param accel 加速度输出 [x, y, z] (LSB)
 * @return 0: 成功
 */
int lsm6dso_read_raw(int16_t gyro[3], int16_t accel[3]);

/**
 * @brief 读取物理单位数据
 * @param gyro 陀螺仪输出 [x, y, z] (rad/s)
 * @param accel 加速度输出 [x, y, z] (g)
 * @return 0: 成功
 */
int lsm6dso_read_all(float gyro[3], float accel[3]);

/**
 * @brief 读取温度
 * @return 温度 (°C)
 */
int16_t lsm6dso_read_temp(void);

/**
 * @brief 进入低功耗模式
 */
void lsm6dso_power_down(void);

/**
 * @brief 退出低功耗模式
 */
void lsm6dso_power_up(void);

#endif /* __LSM6DSO_H__ */
