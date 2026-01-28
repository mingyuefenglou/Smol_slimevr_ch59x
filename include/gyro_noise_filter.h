/**
 * @file gyro_noise_filter.h
 * @brief 陀螺仪噪音优化模块头文件
 * 
 * Gyroscope Noise Optimization Module Header
 */

#ifndef __GYRO_NOISE_FILTER_H__
#define __GYRO_NOISE_FILTER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 配置结构 / Configuration Structure
 *============================================================================*/

typedef struct {
    bool enable_median;         // 启用中值滤波
    bool enable_moving_avg;     // 启用移动平均
    bool enable_kalman;         // 启用卡尔曼滤波
    bool enable_rest_detection; // 启用静止检测
    bool enable_temp_comp;      // 启用温度补偿
    float process_noise;        // 卡尔曼过程噪声
    float measurement_noise;    // 卡尔曼测量噪声
} gyro_filter_config_t;

/*============================================================================
 * 状态结构 / State Structure
 *============================================================================*/

typedef struct {
    gyro_filter_config_t config;
    float noise_level[3];       // 估计的噪声水平 (rad/s)
    bool calibrated;            // 是否已校准
    bool is_stationary;         // 是否静止
    uint32_t sample_count;      // 处理的样本数
} gyro_filter_state_t;

/*============================================================================
 * API 函数 / API Functions
 *============================================================================*/

/**
 * @brief 初始化陀螺仪滤波器
 * @param config 配置 (NULL 使用默认)
 */
void gyro_filter_init(const gyro_filter_config_t *config);

/**
 * @brief 重置滤波器状态
 */
void gyro_filter_reset(void);

/**
 * @brief 校准陀螺仪 (设备静止时调用)
 * @param gyro_samples 陀螺仪采样数据 [N][3]
 * @param count 采样数量 (建议 >= 200)
 * @param temperature 当前温度 (°C)
 * @return 0 成功
 */
int gyro_filter_calibrate(float gyro_samples[][3], int count, float temperature);

/**
 * @brief 处理陀螺仪数据
 * @param gyro_in 输入陀螺仪数据 [rad/s]
 * @param gyro_out 输出滤波后数据 [rad/s]
 * @param accel 加速度计数据 (用于静止检测, 可为 NULL)
 * @param temperature 当前温度 (°C)
 */
void gyro_filter_process(const float gyro_in[3], float gyro_out[3],
                         const float accel[3], float temperature);

/**
 * @brief 获取当前偏差估计
 */
void gyro_filter_get_bias(float bias[3]);

/**
 * @brief 设置偏差
 */
void gyro_filter_set_bias(const float bias[3]);

/**
 * @brief 检查是否静止
 */
bool gyro_filter_is_stationary(void);

/**
 * @brief 获取估计的噪声水平
 */
void gyro_filter_get_noise_level(float noise[3]);

#ifdef __cplusplus
}
#endif

#endif /* __GYRO_NOISE_FILTER_H__ */
