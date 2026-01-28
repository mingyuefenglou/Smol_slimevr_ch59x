/**
 * @file auto_calibration.h
 * @brief 运动中自动校准模块
 * 
 * 功能:
 * 1. 静止检测 + 陀螺仪偏移校准
 * 2. 运动中漂移补偿
 * 3. 加速度计校准
 * 4. 磁力计融合校准
 */

#ifndef __AUTO_CALIBRATION_H__
#define __AUTO_CALIBRATION_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化自动校准模块
 */
void auto_calib_init(void);

/**
 * @brief 更新校准 (每次IMU读取后调用)
 * @param gyro 陀螺仪数据 [rad/s]
 * @param accel 加速度计数据 [m/s²]
 */
void auto_calib_update(float gyro[3], float accel[3]);

/**
 * @brief 更新磁力计校准
 * @param imu_heading IMU估计的航向 [rad]
 * @param mag_heading 磁力计测量的航向 [rad]
 */
void auto_calib_mag_update(float imu_heading, float mag_heading);

/**
 * @brief 检查是否应该使用磁力计
 * @return true 如果磁力计可信
 */
bool auto_calib_should_use_mag(float imu_heading, float mag_heading);

/**
 * @brief 检查是否静止
 * @return true 如果设备静止
 */
bool auto_calib_is_still(void);

/**
 * @brief 检查校准是否有效
 * @return true 如果校准有效
 */
bool auto_calib_is_valid(void);

/**
 * @brief 获取陀螺仪偏移
 * @param offset 输出偏移值 [rad/s]
 */
void auto_calib_get_gyro_offset(float offset[3]);

/**
 * @brief 启用/禁用磁力计校准
 */
void auto_calib_enable_mag(bool enable);

/**
 * @brief 检查磁力计是否已校准
 */
bool auto_calib_mag_calibrated(void);

/**
 * @brief 强制执行校准
 */
void auto_calib_force(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUTO_CALIBRATION_H__ */
