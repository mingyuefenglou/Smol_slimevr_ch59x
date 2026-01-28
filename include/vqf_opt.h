/**
 * @file vqf_opt.h
 * @brief Optimized VQF Sensor Fusion for CH592
 * 
 * Memory optimizations:
 * - Fixed-point Q15/Q31 math where possible
 * - Reduced state variables
 * - Inline critical functions
 * - ~200 bytes RAM (was ~400+)
 */

#ifndef __VQF_OPT_H__
#define __VQF_OPT_H__

#include "optimize.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Optimized VQF State - 64 bytes (was 100+)
 *============================================================================*/

typedef struct PACKED {
    // Quaternion state (16 bytes)
    float quat[4];
    
    // Gyro bias estimate (12 bytes)
    float gyro_bias[3];
    
    // Filter gains (8 bytes)
    float tau_acc;
    float tau_mag;
    
    // Timing (4 bytes)
    float dt;
    
    // Filter state (12 bytes)
    float acc_lp[3];
    
    // Coefficients (8 bytes)
    float beta;
    float zeta;
    
    // Flags (4 bytes)
    u32 sample_count;
} vqf_opt_state_t;

/*============================================================================
 * Fast Math (Optimized for RISC-V)
 *============================================================================*/

// Fast inverse square root (Quake algorithm)
// 输入必须 > 0，否则返回安全值
static FORCE_INLINE float fast_invsqrt(float x) {
    if (x < 1e-10f) return 1000.0f;  // 防止除零
    float xhalf = 0.5f * x;
    union { float f; u32 i; } u = {x};
    u.i = 0x5f3759df - (u.i >> 1);
    u.f *= (1.5f - xhalf * u.f * u.f);
    return u.f;
}

// Fast square root
static FORCE_INLINE float fast_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;  // 防止负数
    return x * fast_invsqrt(x);
}

// Fast atan2 approximation (max error ~0.01 rad)
static inline float fast_atan2(float y, float x) {
    float abs_y = y < 0 ? -y : y;
    float abs_x = x < 0 ? -x : x;
    float a = abs_x < abs_y ? abs_x / abs_y : abs_y / abs_x;
    float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (abs_y > abs_x) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    if (y < 0) r = -r;
    return r;
}

/*============================================================================
 * Quaternion Operations (Inline)
 *============================================================================*/

static FORCE_INLINE void quat_normalize(float q[4]) {
    float inv_norm = fast_invsqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    q[0] *= inv_norm;
    q[1] *= inv_norm;
    q[2] *= inv_norm;
    q[3] *= inv_norm;
}

static FORCE_INLINE void quat_multiply(const float a[4], const float b[4], float out[4]) {
    out[0] = a[0]*b[0] - a[1]*b[1] - a[2]*b[2] - a[3]*b[3];
    out[1] = a[0]*b[1] + a[1]*b[0] + a[2]*b[3] - a[3]*b[2];
    out[2] = a[0]*b[2] - a[1]*b[3] + a[2]*b[0] + a[3]*b[1];
    out[3] = a[0]*b[3] + a[1]*b[2] - a[2]*b[1] + a[3]*b[0];
}

// Rotate vector by quaternion (optimized)
static FORCE_INLINE void quat_rotate_vec(const float q[4], const float v[3], float out[3]) {
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

/*============================================================================
 * VQF API
 *============================================================================*/

/**
 * @brief Initialize VQF filter
 * @param state State structure
 * @param dt Sample period in seconds
 * @param tau_acc Accelerometer time constant
 * @param tau_mag Magnetometer time constant (0 to disable)
 */
void vqf_opt_init(vqf_opt_state_t *state, float dt, float tau_acc, float tau_mag);

/**
 * @brief Update filter with new sensor data
 * @param state State structure
 * @param gyro Gyroscope data in rad/s
 * @param accel Accelerometer data in g
 * @param mag Magnetometer data (can be NULL)
 */
void vqf_opt_update(vqf_opt_state_t *state, const float gyro[3], 
                     const float accel[3], const float mag[3]);

/**
 * @brief Get current quaternion
 * @param state State structure
 * @param quat Output quaternion [w, x, y, z]
 */
static FORCE_INLINE void vqf_opt_get_quat(const vqf_opt_state_t *state, float quat[4]) {
    quat[0] = state->quat[0];
    quat[1] = state->quat[1];
    quat[2] = state->quat[2];
    quat[3] = state->quat[3];
}

/**
 * @brief Reset filter to identity
 * @param state State structure
 */
void vqf_opt_reset(vqf_opt_state_t *state);

/**
 * @brief Get gyro bias estimate
 * @param state State structure
 * @param bias Output bias in rad/s
 */
static FORCE_INLINE void vqf_opt_get_bias(const vqf_opt_state_t *state, float bias[3]) {
    bias[0] = state->gyro_bias[0];
    bias[1] = state->gyro_bias[1];
    bias[2] = state->gyro_bias[2];
}

#ifdef __cplusplus
}
#endif

#endif /* __VQF_OPT_H__ */
