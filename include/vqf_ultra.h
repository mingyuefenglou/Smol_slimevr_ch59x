/**
 * @file vqf_ultra.h
 * @brief Ultra-Optimized VQF Sensor Fusion for CH592
 * 
 * Features:
 * - Fixed-point Q15 arithmetic (no floating point)
 * - Lookup tables for trig functions
 * - ~50% faster than float version
 * - 32 bytes RAM (vs 180 bytes for float)
 * 
 * Usage:
 *   vqf_ultra_state_t state;
 *   vqf_ultra_init(&state, 200);  // 200Hz sample rate
 *   
 *   // In sensor loop:
 *   int16_t gyro[3] = {...};   // 0.01 deg/s
 *   int16_t accel[3] = {...};  // 0.001g (mg)
 *   vqf_ultra_update(&state, gyro, accel);
 *   
 *   // Get orientation:
 *   float quat[4];
 *   vqf_ultra_get_quat(&state, quat);
 */

#ifndef __VQF_ULTRA_H__
#define __VQF_ULTRA_H__

#include "optimize.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Fixed-Point Types
 *============================================================================*/

typedef int16_t q15_t;   // Q1.15 format: -1.0 to +0.99997
typedef int32_t q31_t;   // Q1.31 format for intermediate calculations

/*============================================================================
 * VQF Ultra State Structure (32 bytes)
 *============================================================================*/

typedef struct PACKED {
    // Orientation quaternion [w, x, y, z] in Q15
    q15_t quat[4];          // 8 bytes
    
    // Gyroscope bias estimate in Q15 rad/s
    q15_t gyro_bias[3];     // 6 bytes
    
    // Low-pass filtered accelerometer in Q15
    q15_t acc_lp[3];        // 6 bytes
    
    // Filter gains in Q15
    q15_t k_acc;            // 2 bytes - accelerometer correction gain
    q15_t k_bias;           // 2 bytes - bias update gain
    
    // Counters
    uint16_t rest_count;    // 2 bytes - samples at rest
    uint16_t sample_count;  // 2 bytes - total samples (wraps)
    
    // Configuration
    uint8_t dt_shift;       // 1 byte - dt = 1/(2^dt_shift) seconds
    uint8_t flags;          // 1 byte - status flags
} vqf_ultra_state_t;        // Total: 32 bytes

// Status flags
#define VQF_ULTRA_INITIALIZED   0x80
#define VQF_ULTRA_AT_REST       0x01
#define VQF_ULTRA_MAG_OK        0x02

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize VQF ultra filter
 * @param state Filter state
 * @param sample_rate_hz Sample rate in Hz (100, 200, 400, etc.)
 */
void vqf_ultra_init(vqf_ultra_state_t *state, uint16_t sample_rate_hz);

/**
 * @brief Update filter with new sensor data
 * @param state Filter state
 * @param gyro_raw Gyroscope reading in 0.01 deg/s (from sensor)
 * @param accel_raw Accelerometer reading in mg (0.001g)
 * 
 * Note: This function is optimized for speed and should be called
 * at a consistent rate matching sample_rate_hz.
 */
void vqf_ultra_update(vqf_ultra_state_t *state,
                      const int16_t gyro_raw[3],
                      const int16_t accel_raw[3]);

/**
 * @brief Get orientation as quaternion (float)
 * @param state Filter state
 * @param quat Output quaternion [w, x, y, z], normalized
 */
void vqf_ultra_get_quat(const vqf_ultra_state_t *state, float quat[4]);

/**
 * @brief Get orientation as quaternion (Q15)
 * @param state Filter state
 * @param quat Output quaternion [w, x, y, z] in Q15 format
 */
void vqf_ultra_get_quat_q15(const vqf_ultra_state_t *state, q15_t quat[4]);

/**
 * @brief Set orientation quaternion (for wake restore)
 * v0.5.0: 用于从睡眠唤醒后恢复姿态
 * @param state Filter state
 * @param quat Input quaternion [w, x, y, z], normalized
 */
void vqf_ultra_set_quat(vqf_ultra_state_t *state, const float quat[4]);

/**
 * @brief Get orientation as Euler angles
 * @param state Filter state
 * @param euler Output [roll, pitch, yaw] in 0.01 degrees
 */
void vqf_ultra_get_euler(const vqf_ultra_state_t *state, int16_t euler[3]);

/**
 * @brief Reset filter to initial state
 * @param state Filter state
 */
void vqf_ultra_reset(vqf_ultra_state_t *state);

/**
 * @brief Set gyroscope bias manually
 * @param state Filter state
 * @param bias Bias in 0.01 deg/s
 */
void vqf_ultra_set_bias(vqf_ultra_state_t *state, const int16_t bias[3]);

/**
 * @brief Check if device is at rest
 * @param state Filter state
 * @return true if at rest
 */
static FORCE_INLINE bool vqf_ultra_is_rest(const vqf_ultra_state_t *state)
{
    return (state->flags & VQF_ULTRA_AT_REST) != 0;
}

/**
 * @brief Get current gyroscope bias in 0.01 deg/s
 * @param state Filter state
 * @param bias Output bias
 */
static FORCE_INLINE void vqf_ultra_get_bias(const vqf_ultra_state_t *state, int16_t bias[3])
{
    bias[0] = state->gyro_bias[0] / 6;
    bias[1] = state->gyro_bias[1] / 6;
    bias[2] = state->gyro_bias[2] / 6;
}

#ifdef __cplusplus
}
#endif

#endif /* __VQF_ULTRA_H__ */
