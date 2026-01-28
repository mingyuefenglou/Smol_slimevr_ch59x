/**
 * @file vqf_advanced.h
 * @brief Advanced VQF (Versatile Quaternion-based Filter) Implementation
 * 
 * VQF is a state-of-the-art sensor fusion algorithm that provides:
 * - Superior accuracy compared to Madgwick/Mahony
 * - Automatic gyroscope bias estimation
 * - Adaptive accelerometer trust based on motion detection
 * - Optional magnetometer integration with automatic disturbance rejection
 * - Rest detection for enhanced bias estimation
 * 
 * Reference:
 * D. Laidig and T. Seel. "VQF: Highly Accurate IMU Orientation Estimation
 * with Bias Estimation and Magnetic Disturbance Rejection."
 * Information Fusion 91 (2023): 187-204.
 * 
 * Memory Optimized for CH592:
 * - Full state: ~180 bytes RAM
 * - Basic state: ~100 bytes RAM (without magnetometer)
 */

#ifndef __VQF_ADVANCED_H__
#define __VQF_ADVANCED_H__

#include "optimize.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

// Feature flags
// v0.6.2: 根据config.h中的USE_MAGNETOMETER自动启用
#include "config.h"
#if defined(USE_MAGNETOMETER) && USE_MAGNETOMETER
#define VQF_USE_MAGNETOMETER    1   // Enable magnetometer (adds ~40 bytes)
#else
#define VQF_USE_MAGNETOMETER    0   // Magnetometer disabled
#endif

#define VQF_USE_REST_DETECTION  1   // Enable rest detection for better bias
#define VQF_USE_MOTION_BIAS     1   // Gyro bias estimation during motion

// Filter parameters (tunable)
#define VQF_TAU_ACC_DEFAULT     3.0f    // Accelerometer time constant (s)
#define VQF_TAU_MAG_DEFAULT     9.0f    // Magnetometer time constant (s)
#define VQF_BIAS_SIGMA_INIT     0.5f    // Initial bias uncertainty (rad/s)
#define VQF_BIAS_SIGMA_MOTION   0.05f   // v0.6.2: 降低运动时偏差不确定性
#define VQF_BIAS_SIGMA_REST     0.03f   // Bias uncertainty at rest

// Motion detection thresholds
// v0.6.2: 优化参数，解决"大动稳、小动/站立飘"问题
#define VQF_REST_GYRO_TH        1.5f    // Rest threshold gyro (deg/s) - 降低以更容易进入Rest
#define VQF_REST_ACCEL_TH       0.3f    // Rest threshold accel (m/s²) - 降低阈值
#define VQF_REST_TIME_TH        0.5f    // v0.6.2: 从1.5s降到0.5s，快速进入Rest模式

// v0.6.2: 新增Motion Bias控制参数 (解决信噪比陷阱)
#define VQF_MOTION_BIAS_ALPHA_XY    0.0001f   // 水平轴偏差更新速率 (可观测)
#define VQF_MOTION_BIAS_ALPHA_Z     0.00001f  // v0.6.2: Z轴(偏航)遗忘速率降低10倍！
#define VQF_MOTION_BIAS_LIMIT       0.05f     // v0.6.2: 偏差累积限制从0.1降到0.05 rad/s

/*============================================================================
 * VQF State Structure (~180 bytes with magnetometer, ~140 without)
 *============================================================================*/

typedef struct PACKED {
    // Orientation quaternion (16 bytes)
    float quat[4];              // [w, x, y, z]
    
    // Gyroscope bias estimate (12 bytes)
    float gyro_bias[3];         // rad/s
    
    // Accelerometer low-pass state (12 bytes)
    float acc_lp[3];
    
    // Bias estimation state (24 bytes)
    float bias_p[3];            // Covariance diagonal (simplified)
    float bias_motion[3];       // Motion-based bias update accumulator
    
    // Rest detection state (16 bytes)
    float rest_last_gyro[3];
    float rest_time;            // Time spent at rest
    
    // Filter coefficients (16 bytes)
    float tau_acc;
    float tau_mag;
    float dt;
    float k_acc;                // Accelerometer gain
    
#if VQF_USE_MAGNETOMETER
    // Magnetometer state (28 bytes)
    float mag_ref[3];           // Reference field in earth frame
    float mag_lp[3];            // Low-pass filtered measurement
    float mag_dist_time;        // Time since disturbance detected
#endif
    
    // Status (4 bytes)
    u32 sample_count;
    
    // Flags (1 byte)
    u8 flags;
} vqf_state_t;

// Flags
#define VQF_FLAG_REST           BIT(0)
#define VQF_FLAG_MAG_DISTURBED  BIT(1)
#define VQF_FLAG_INITIALIZED    BIT(7)

/*============================================================================
 * Optimized Math Functions
 *============================================================================*/

// Fast inverse square root (Quake III algorithm, refined)
static FORCE_INLINE float vqf_invsqrt(float x)
{
    float xhalf = 0.5f * x;
    union { float f; u32 i; } u = {x};
    u.i = 0x5f375a86 - (u.i >> 1);  // Better constant
    u.f *= (1.5f - xhalf * u.f * u.f);
    u.f *= (1.5f - xhalf * u.f * u.f);  // Second iteration for accuracy
    return u.f;
}

// Fast square root
static FORCE_INLINE float vqf_sqrt(float x)
{
    return x * vqf_invsqrt(x);
}

// Fast exp approximation (for filter coefficients)
static FORCE_INLINE float vqf_expf(float x)
{
    // Limit range
    if (x < -5.0f) return 0.0f;
    if (x > 5.0f) return 148.4f;
    
    // Padé approximation
    float x2 = x * x;
    return (1.0f + x + 0.5f * x2 + 0.1667f * x * x2) / 
           (1.0f - x + 0.5f * x2 - 0.1667f * x * x2);
}

// Vector operations
static FORCE_INLINE float vqf_vec3_norm(const float v[3])
{
    return vqf_sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static FORCE_INLINE void vqf_vec3_normalize(float v[3])
{
    float inv_n = vqf_invsqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    v[0] *= inv_n;
    v[1] *= inv_n;
    v[2] *= inv_n;
}

static FORCE_INLINE void vqf_vec3_cross(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

static FORCE_INLINE float vqf_vec3_dot(const float a[3], const float b[3])
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

/*============================================================================
 * Quaternion Operations
 *============================================================================*/

static FORCE_INLINE void vqf_quat_normalize(float q[4])
{
    float inv_n = vqf_invsqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    q[0] *= inv_n;
    q[1] *= inv_n;
    q[2] *= inv_n;
    q[3] *= inv_n;
}

static FORCE_INLINE void vqf_quat_multiply(const float a[4], const float b[4], float out[4])
{
    out[0] = a[0]*b[0] - a[1]*b[1] - a[2]*b[2] - a[3]*b[3];
    out[1] = a[0]*b[1] + a[1]*b[0] + a[2]*b[3] - a[3]*b[2];
    out[2] = a[0]*b[2] - a[1]*b[3] + a[2]*b[0] + a[3]*b[1];
    out[3] = a[0]*b[3] + a[1]*b[2] - a[2]*b[1] + a[3]*b[0];
}

// Rotate vector by quaternion
static FORCE_INLINE void vqf_quat_rotate(const float q[4], const float v[3], float out[3])
{
    float qw = q[0], qx = q[1], qy = q[2], qz = q[3];
    
    // t = 2 * cross(q.xyz, v)
    float tx = 2.0f * (qy * v[2] - qz * v[1]);
    float ty = 2.0f * (qz * v[0] - qx * v[2]);
    float tz = 2.0f * (qx * v[1] - qy * v[0]);
    
    // out = v + q.w * t + cross(q.xyz, t)
    out[0] = v[0] + qw * tx + qy * tz - qz * ty;
    out[1] = v[1] + qw * ty + qz * tx - qx * tz;
    out[2] = v[2] + qw * tz + qx * ty - qy * tx;
}

// Rotate vector by quaternion inverse
static FORCE_INLINE void vqf_quat_rotate_inv(const float q[4], const float v[3], float out[3])
{
    float qw = q[0], qx = -q[1], qy = -q[2], qz = -q[3];
    
    float tx = 2.0f * (qy * v[2] - qz * v[1]);
    float ty = 2.0f * (qz * v[0] - qx * v[2]);
    float tz = 2.0f * (qx * v[1] - qy * v[0]);
    
    out[0] = v[0] + qw * tx + qy * tz - qz * ty;
    out[1] = v[1] + qw * ty + qz * tx - qx * tz;
    out[2] = v[2] + qw * tz + qx * ty - qy * tx;
}

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize VQF filter
 * @param state State structure to initialize
 * @param dt Sample period in seconds (e.g., 0.005 for 200Hz)
 * @param tau_acc Accelerometer time constant (typical: 3.0)
 * @param tau_mag Magnetometer time constant (typical: 9.0, 0 to disable)
 */
void vqf_advanced_init(vqf_state_t *state, float dt, float tau_acc, float tau_mag);

/**
 * @brief Update filter with 6-axis IMU data
 * @param state Filter state
 * @param gyro Gyroscope reading in rad/s
 * @param accel Accelerometer reading in g (or m/s², normalized internally)
 */
void vqf_advanced_update(vqf_state_t *state, const float gyro[3], const float accel[3]);

/**
 * @brief Update filter with 9-axis IMU data (with magnetometer)
 * @param state Filter state
 * @param gyro Gyroscope reading in rad/s
 * @param accel Accelerometer reading in g
 * @param mag Magnetometer reading (arbitrary units, normalized internally)
 */
void vqf_advanced_update_mag(vqf_state_t *state, const float gyro[3], 
                              const float accel[3], const float mag[3]);

/**
 * @brief Get current orientation quaternion
 * @param state Filter state
 * @param quat Output quaternion [w, x, y, z]
 */
static FORCE_INLINE void vqf_advanced_get_quat(const vqf_state_t *state, float quat[4])
{
    quat[0] = state->quat[0];
    quat[1] = state->quat[1];
    quat[2] = state->quat[2];
    quat[3] = state->quat[3];
}

/**
 * @brief Get current gyroscope bias estimate
 * @param state Filter state
 * @param bias Output bias in rad/s
 */
static FORCE_INLINE void vqf_advanced_get_bias(const vqf_state_t *state, float bias[3])
{
    bias[0] = state->gyro_bias[0];
    bias[1] = state->gyro_bias[1];
    bias[2] = state->gyro_bias[2];
}

/**
 * @brief Check if device is at rest
 * @param state Filter state
 * @return true if at rest
 */
static FORCE_INLINE bool vqf_advanced_is_rest(const vqf_state_t *state)
{
    return (state->flags & VQF_FLAG_REST) != 0;
}

/**
 * @brief Reset filter to initial state
 * @param state Filter state
 */
void vqf_advanced_reset(vqf_state_t *state);

/**
 * @brief Set gyroscope bias manually
 * @param state Filter state
 * @param bias Bias in rad/s
 */
void vqf_advanced_set_bias(vqf_state_t *state, const float bias[3]);

/**
 * @brief Get filter coefficients for tuning
 * @param state Filter state
 * @param tau_acc Output accelerometer time constant
 * @param tau_mag Output magnetometer time constant
 */
static FORCE_INLINE void vqf_advanced_get_params(const vqf_state_t *state, 
                                                  float *tau_acc, float *tau_mag)
{
    *tau_acc = state->tau_acc;
    *tau_mag = state->tau_mag;
}

/**
 * @brief Set filter coefficients
 * @param state Filter state
 * @param tau_acc Accelerometer time constant
 * @param tau_mag Magnetometer time constant
 */
void vqf_advanced_set_params(vqf_state_t *state, float tau_acc, float tau_mag);

#ifdef __cplusplus
}
#endif

#endif /* __VQF_ADVANCED_H__ */
