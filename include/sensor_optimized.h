/**
 * @file sensor_optimized.h
 * @brief 优化的传感器读取 - DMA + 中断驱动
 * 
 * 目标: 传感器延迟 < 3ms
 * 
 * 架构:
 * 1. IMU 中断触发数据就绪
 * 2. DMA 传输读取数据
 * 3. 后台处理融合算法
 */

#ifndef __SENSOR_OPTIMIZED_H__
#define __SENSOR_OPTIMIZED_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化优化传感器模块
 * @return 0 成功
 */
int sensor_optimized_init(void);

/**
 * @brief 获取传感器样本
 * @param gyro 陀螺仪输出 [rad/s]
 * @param accel 加速度计输出 [m/s²]
 * @param timestamp_us 时间戳输出 [us]
 * @return true 如果有新数据
 */
bool sensor_optimized_get_sample(float gyro[3], float accel[3], uint32_t *timestamp_us);

/**
 * @brief 获取统计信息
 * @param total 总样本数
 * @param dropped 丢弃样本数
 * @param max_latency 最大延迟 [us]
 * @param avg_latency 平均延迟 [us]
 */
void sensor_optimized_get_stats(uint32_t *total, uint32_t *dropped, 
                                uint32_t *max_latency, float *avg_latency);

/**
 * @brief IMU数据就绪中断处理 (在GPIO中断中调用)
 */
void sensor_optimized_isr(void);

/**
 * @brief DMA完成中断处理 (在DMA中断中调用)
 */
void sensor_optimized_dma_complete(void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_OPTIMIZED_H__ */
