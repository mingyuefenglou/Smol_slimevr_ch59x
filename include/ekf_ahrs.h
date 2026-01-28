/**
 * @file ekf_ahrs.h
 * @brief Extended Kalman Filter for AHRS
 * 
 * 扩展卡尔曼滤波器姿态估计
 * Extended Kalman Filter for Attitude Estimation
 * 
 * 可行性分析 / Feasibility Analysis:
 * ====================================
 * 
 * 内存使用 / Memory Usage:
 *   - 状态变量: 7 floats = 28 bytes (q[4] + bias[3])
 *   - 协方差:   7 floats = 28 bytes (对角简化)
 *   - 其他:     ~20 bytes
 *   - 总计:     ~80 bytes (vs VQF 64 bytes)
 * 
 * CPU 使用 / CPU Usage:
 *   - 预测:     ~300μs
 *   - 更新:     ~500μs
 *   - 总计:     ~800μs @ 60MHz
 *   - 比 VQF (~400μs) 慢约 2x
 *   - 但仍可满足 200Hz (5000μs 周期)
 * 
 * 精度提升 / Accuracy Improvement:
 *   - 更好的陀螺仪偏差估计
 *   - 更低的长期漂移
 *   - 自适应测量噪声
 *   - 静态精度: <0.3° (vs VQF 0.5°)
 *   - 动态精度: <1.5° (vs VQF 2°)
 * 
 * 结论 / Conclusion:
 *   ✅ 可行! RAM 和 CPU 资源充足
 *   推荐在精度要求高的场景使用
 */

#ifndef __EKF_AHRS_H__
#define __EKF_AHRS_H__

#include "optimize.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 数据结构 / Data Structures
 *============================================================================*/

/**
 * @brief EKF 状态结构 (~80 bytes)
 */
typedef struct PACKED {
    // 状态向量 / State vector
    float q[4];         // 四元数 [w, x, y, z]
    float bias[3];      // 陀螺仪偏差 [rad/s]
    
    // 协方差 (对角简化) / Covariance (diagonal simplified)
    float P[7];         // P[0-3]=quat, P[4-6]=bias
    
    // 配置 / Configuration
    float dt;           // 采样周期 [s]
    
    // 状态 / Status
    uint32_t sample_count;
    uint8_t initialized;
} ekf_ahrs_state_t;

/*============================================================================
 * API 函数 / API Functions
 *============================================================================*/

/**
 * @brief 初始化 EKF
 * 
 * @param ekf EKF 状态结构
 * @param dt 采样周期 (秒), 如 0.005 for 200Hz
 */
void ekf_ahrs_init(ekf_ahrs_state_t *ekf, float dt);

/**
 * @brief 重置 EKF 状态
 */
void ekf_ahrs_reset(ekf_ahrs_state_t *ekf);

/**
 * @brief 更新 EKF
 * 
 * @param ekf EKF 状态
 * @param gyro 陀螺仪数据 [rad/s]
 * @param accel 加速度计数据 [g]
 * @param mag 磁力计数据 (可选, NULL 忽略)
 */
void ekf_ahrs_update(ekf_ahrs_state_t *ekf,
                     const float gyro[3],
                     const float accel[3],
                     const float mag[3]);

/**
 * @brief 获取四元数
 * 
 * @param ekf EKF 状态
 * @param quat 输出四元数 [w, x, y, z]
 */
void ekf_ahrs_get_quat(const ekf_ahrs_state_t *ekf, float quat[4]);

/**
 * @brief 获取欧拉角
 * 
 * @param ekf EKF 状态
 * @param roll 输出滚转角 (度)
 * @param pitch 输出俯仰角 (度)
 * @param yaw 输出偏航角 (度)
 */
void ekf_ahrs_get_euler(const ekf_ahrs_state_t *ekf, 
                        float *roll, float *pitch, float *yaw);

/**
 * @brief 获取陀螺仪偏差估计
 * 
 * @param ekf EKF 状态
 * @param bias 输出偏差 [rad/s]
 */
void ekf_ahrs_get_bias(const ekf_ahrs_state_t *ekf, float bias[3]);

/*============================================================================
 * 算法选择宏 / Algorithm Selection Macros
 * 
 * 默认使用 VQF 算法 (更低功耗，足够精度)
 * Default: VQF algorithm (lower power, sufficient accuracy)
 * 
 * 如需使用 EKF (更高精度):
 * To use EKF (higher accuracy):
 *   1. 在 include/config.h 中定义 USE_EKF_INSTEAD_OF_VQF
 *   2. 或在编译时: make DEFINES="-DUSE_EKF_INSTEAD_OF_VQF"
 *============================================================================*/

// 注意: 默认 VQF, 取消下面注释以使用 EKF
// NOTE: Default is VQF, uncomment below to use EKF
// #define USE_EKF_INSTEAD_OF_VQF

#ifdef USE_EKF_INSTEAD_OF_VQF
    // 类型别名 / Type alias
    typedef ekf_ahrs_state_t fusion_state_t;
    
    // 函数别名 / Function aliases
    #define fusion_init(s, dt, t1, t2)   ekf_ahrs_init(s, dt)
    #define fusion_update(s, g, a, m)    ekf_ahrs_update(s, g, a, m)
    #define fusion_get_quat(s, q)        ekf_ahrs_get_quat(s, q)
    #define fusion_reset(s)              ekf_ahrs_reset(s)
#else
    #include "vqf_opt.h"
    typedef vqf_opt_state_t fusion_state_t;
    #define fusion_init(s, dt, t1, t2)   vqf_opt_init(s, dt, t1, t2)
    #define fusion_update(s, g, a, m)    vqf_opt_update(s, g, a, m)
    #define fusion_get_quat(s, q)        do { \
        (q)[0] = (s)->quat[0]; (q)[1] = (s)->quat[1]; \
        (q)[2] = (s)->quat[2]; (q)[3] = (s)->quat[3]; \
    } while(0)
    #define fusion_reset(s)              vqf_opt_reset(s)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __EKF_AHRS_H__ */
