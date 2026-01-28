/**
 * @file sc7i22.h
 * @brief 士兰微 SC7I22 国产 6轴 IMU 驱动
 * 
 * 特性:
 * - 低功耗设计
 * - 宽温度范围: -40°C ~ +85°C
 * - 支持 SPI 和 I2C 接口
 * - 国产替代方案
 */

#ifndef __SC7I22_H__
#define __SC7I22_H__

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * 寄存器定义
 *============================================================================*/

#define SC7I22_I2C_ADDR         0x68
#define SC7I22_WHO_AM_I_VAL     0x22

// 寄存器地址
#define SC7I22_REG_WHO_AM_I     0x00
#define SC7I22_REG_CONFIG       0x01
#define SC7I22_REG_GYRO_CONFIG  0x02
#define SC7I22_REG_ACCEL_CONFIG 0x03
#define SC7I22_REG_ACCEL_XOUT_H 0x08
#define SC7I22_REG_TEMP_OUT_H   0x0E
#define SC7I22_REG_GYRO_XOUT_H  0x10
#define SC7I22_REG_PWR_MGMT     0x16
#define SC7I22_REG_SOFT_RESET   0x17

/*============================================================================
 * 配置选项
 *============================================================================*/

// 陀螺仪量程
typedef enum {
    SC7I22_GYRO_2000DPS = 0,
    SC7I22_GYRO_1000DPS = 1,
    SC7I22_GYRO_500DPS  = 2,
    SC7I22_GYRO_250DPS  = 3
} sc7i22_gyro_range_t;

// 加速度量程
typedef enum {
    SC7I22_ACCEL_16G = 0,
    SC7I22_ACCEL_8G  = 1,
    SC7I22_ACCEL_4G  = 2,
    SC7I22_ACCEL_2G  = 3
} sc7i22_accel_range_t;

// 输出数据率
typedef enum {
    SC7I22_ODR_500HZ  = 0,
    SC7I22_ODR_200HZ  = 1,
    SC7I22_ODR_100HZ  = 2,
    SC7I22_ODR_50HZ   = 3,
    SC7I22_ODR_25HZ   = 4
} sc7i22_odr_t;

/*============================================================================
 * 配置结构
 *============================================================================*/

typedef struct {
    sc7i22_gyro_range_t gyro_range;
    sc7i22_accel_range_t accel_range;
    sc7i22_odr_t odr;
    bool enable_int;
} sc7i22_config_t;

/*============================================================================
 * API
 *============================================================================*/

// 检测和初始化
bool sc7i22_detect(void);
int sc7i22_init(void);
int sc7i22_init_advanced(const sc7i22_config_t *config);

// 数据读取
int sc7i22_read(float gyro[3], float accel[3]);
int sc7i22_read_raw(int16_t gyro[3], int16_t accel[3]);
float sc7i22_read_temp(void);

// 电源管理
int sc7i22_suspend(void);
int sc7i22_resume(void);

// 配置
int sc7i22_set_odr(sc7i22_odr_t odr);
int sc7i22_set_gyro_range(sc7i22_gyro_range_t range);
int sc7i22_set_accel_range(sc7i22_accel_range_t range);

// 自检
int sc7i22_self_test(void);

#endif /* __SC7I22_H__ */
