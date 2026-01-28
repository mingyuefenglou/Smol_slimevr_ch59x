/**
 * @file imu_interface.h
 * @brief IMU 统一接口层头文件
 * 
 * IMU Unified Interface Layer Header
 */

#ifndef __IMU_INTERFACE_H__
#define __IMU_INTERFACE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * IMU 类型定义 (与 config.h 一致)
 *============================================================================*/

#define IMU_UNKNOWN         0
#define IMU_MPU6050         1
#define IMU_BMI160          2
#define IMU_BMI270          3
#define IMU_ICM42688        4
#define IMU_ICM45686        5
#define IMU_LSM6DSV         6
#define IMU_LSM6DSR         7

/*============================================================================
 * 接口类型 - 定义在imu_interface.c中的枚举
 * 这里不再定义宏，以避免与.c文件中的枚举冲突
 *============================================================================*/

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化 IMU (自动检测 SPI/I2C)
 * @return 0 成功, -1 未检测到 IMU
 */
int imu_init(void);

/**
 * @brief 读取陀螺仪和加速度计数据
 * @param gyro 输出陀螺仪 [rad/s]
 * @param accel 输出加速度计 [g]
 */
int imu_read_all(float gyro[3], float accel[3]);

/**
 * @brief 读取原始数据
 */
int imu_read_raw(int16_t gyro[3], int16_t accel[3]);

/**
 * @brief 检查数据是否就绪
 */
bool imu_data_ready(void);

/**
 * @brief 获取检测到的 IMU 类型
 */
uint8_t imu_get_type(void);

/**
 * @brief 获取 IMU 类型名称字符串
 */
const char* imu_get_type_name(void);

/**
 * @brief 获取使用的接口类型 (SPI/I2C)
 */
uint8_t imu_get_interface(void);

/**
 * @brief 设置陀螺仪偏差校正
 */
void imu_set_gyro_bias(const float bias[3]);

/**
 * @brief 进入低功耗模式
 */
void imu_suspend(void);

/**
 * @brief 从低功耗模式恢复
 */
void imu_resume(void);

/**
 * @brief v0.4.24: 使能WOM (Wake on Motion) 中断
 * @param threshold_mg 运动检测阈值 (mg)
 * @return 0 成功, 负值失败
 */
int imu_enable_wom(uint16_t threshold_mg);

/**
 * @brief v0.4.24: 禁用WOM中断，恢复正常模式
 * @return 0 成功, 负值失败
 */
int imu_disable_wom(void);

#ifdef __cplusplus
}
#endif

#endif /* __IMU_INTERFACE_H__ */
