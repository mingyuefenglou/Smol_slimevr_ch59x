/**
 * @file iim42652.h
 * @brief TDK IIM-42652 工业级 IMU 驱动
 * 
 * 特性:
 * - 扩展温度范围: -40°C ~ +105°C
 * - 与 ICM-42688 寄存器兼容
 * - 支持 SPI (最高 24MHz) 和 I2C
 * - 工业级可靠性
 */

#ifndef __IIM42652_H__
#define __IIM42652_H__

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * 寄存器定义
 *============================================================================*/

#define IIM42652_I2C_ADDR       0x68
#define IIM42652_WHO_AM_I_VAL   0x6F

// Bank 0 寄存器
#define IIM42652_REG_DEVICE_CONFIG      0x11
#define IIM42652_REG_INT_CONFIG         0x14
#define IIM42652_REG_FIFO_CONFIG        0x16
#define IIM42652_REG_TEMP_DATA1         0x1D
#define IIM42652_REG_ACCEL_DATA_X1      0x1F
#define IIM42652_REG_GYRO_DATA_X1       0x25
#define IIM42652_REG_PWR_MGMT0          0x4E
#define IIM42652_REG_GYRO_CONFIG0       0x4F
#define IIM42652_REG_ACCEL_CONFIG0      0x50
#define IIM42652_REG_GYRO_CONFIG1       0x51
#define IIM42652_REG_ACCEL_CONFIG1      0x53
#define IIM42652_REG_WHO_AM_I           0x75
#define IIM42652_REG_BANK_SEL           0x76

/*============================================================================
 * 配置选项
 *============================================================================*/

// 陀螺仪量程
typedef enum {
    IIM42652_GYRO_2000DPS = 0,
    IIM42652_GYRO_1000DPS = 1,
    IIM42652_GYRO_500DPS  = 2,
    IIM42652_GYRO_250DPS  = 3,
    IIM42652_GYRO_125DPS  = 4,
    IIM42652_GYRO_62_5DPS = 5,
    IIM42652_GYRO_31_25DPS = 6,
    IIM42652_GYRO_15_625DPS = 7
} iim42652_gyro_range_t;

// 加速度量程
typedef enum {
    IIM42652_ACCEL_16G = 0,
    IIM42652_ACCEL_8G  = 1,
    IIM42652_ACCEL_4G  = 2,
    IIM42652_ACCEL_2G  = 3
} iim42652_accel_range_t;

// 输出数据率 (ODR)
typedef enum {
    IIM42652_ODR_8000HZ = 3,
    IIM42652_ODR_4000HZ = 4,
    IIM42652_ODR_2000HZ = 5,
    IIM42652_ODR_1000HZ = 6,
    IIM42652_ODR_200HZ  = 7,
    IIM42652_ODR_100HZ  = 8,
    IIM42652_ODR_50HZ   = 9,
    IIM42652_ODR_25HZ   = 10,
    IIM42652_ODR_12_5HZ = 11
} iim42652_odr_t;

/*============================================================================
 * 配置结构
 *============================================================================*/

typedef struct {
    iim42652_gyro_range_t gyro_range;
    iim42652_accel_range_t accel_range;
    iim42652_odr_t odr;
    bool use_fifo;
    bool enable_int1;
} iim42652_config_t;

/*============================================================================
 * API
 *============================================================================*/

// 检测和初始化
bool iim42652_detect(void);
int iim42652_init(void);
int iim42652_init_advanced(const iim42652_config_t *config);

// 数据读取
int iim42652_read(float gyro[3], float accel[3]);
int iim42652_read_raw(int16_t gyro[3], int16_t accel[3]);
float iim42652_read_temp(void);

// 电源管理
int iim42652_suspend(void);
int iim42652_resume(void);

// 配置
int iim42652_set_odr(iim42652_odr_t odr);
int iim42652_set_gyro_range(iim42652_gyro_range_t range);
int iim42652_set_accel_range(iim42652_accel_range_t range);

// 中断配置
int iim42652_enable_data_ready_int(bool enable);
int iim42652_clear_int(void);

// 自检
int iim42652_self_test(void);

#endif /* __IIM42652_H__ */
