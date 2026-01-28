/**
 * @file lsm6dsv.h
 * @brief LSM6DSV IMU Driver Header
 */

#ifndef __LSM6DSV_H__
#define __LSM6DSV_H__

#include "optimize.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LSM6DSV
 * @return 0 成功, -1 设备未找到
 */
int lsm6dsv_init(void);

/**
 * @brief 读取加速度计数据 (g)
 */
int lsm6dsv_read_accel(float *ax, float *ay, float *az);

/**
 * @brief 读取陀螺仪数据 (rad/s)
 */
int lsm6dsv_read_gyro(float *gx, float *gy, float *gz);

/**
 * @brief 读取所有数据
 */
int lsm6dsv_read_all(float gyro[3], float accel[3]);

/**
 * @brief 读取原始数据
 */
int lsm6dsv_read_raw(int16_t gyro[3], int16_t accel[3]);

/**
 * @brief 读取温度 (°C)
 */
int lsm6dsv_read_temp(float *temp);

/**
 * @brief 检查数据是否就绪
 */
bool lsm6dsv_data_ready(void);

/**
 * @brief 设置输出数据率
 */
int lsm6dsv_set_odr(uint16_t odr_hz);

/**
 * @brief 进入低功耗模式
 */
void lsm6dsv_suspend(void);

/**
 * @brief 从低功耗模式恢复
 */
void lsm6dsv_resume(void);

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DSV_H__ */
