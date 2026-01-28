/**
 * @file temp_compensation.h
 * @brief 陀螺仪温度漂移补偿模块
 * 
 * 原理: 陀螺仪偏移随温度变化 (约 0.03 dps/°C)
 * 模型: offset = a + b*(T-Tref) + c*(T-Tref)^2
 */

#ifndef __TEMP_COMPENSATION_H__
#define __TEMP_COMPENSATION_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化温度补偿模块
 */
void temp_comp_init(void);

/**
 * @brief 更新当前温度
 * @param temp_c 温度 (°C)
 */
void temp_comp_update_temp(float temp_c);

/**
 * @brief 应用温度补偿到陀螺仪数据
 * @param gyro 陀螺仪数据 [rad/s] (会被修改)
 */
void temp_comp_apply(float gyro[3]);

/**
 * @brief 学习温度补偿系数
 * @param observed_offset 观测到的陀螺仪偏移
 */
void temp_comp_learn(const float observed_offset[3]);

/**
 * @brief 保存补偿系数到Flash
 */
void temp_comp_save(void);

/**
 * @brief 获取当前温度
 * @return 温度 (°C)
 */
float temp_comp_get_temp(void);

/**
 * @brief 获取温度变化率
 * @return 温度变化率 (°C/s)
 */
float temp_comp_get_rate(void);

/**
 * @brief 获取当前补偿值
 * @param comp 输出补偿值 [rad/s]
 */
void temp_comp_get_compensation(float comp[3]);

#ifdef __cplusplus
}
#endif

#endif /* __TEMP_COMPENSATION_H__ */
