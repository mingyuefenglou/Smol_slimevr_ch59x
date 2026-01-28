/**
 * @file vqf_advanced.c
 * @brief Advanced VQF Sensor Fusion Implementation
 * 
 * Key Features:
 * 1. Gyroscope Integration: Dead-reckoning with bias correction
 * 2. Accelerometer Correction: Tilt estimation with adaptive gain
 * 3. Rest Detection: Enhanced bias estimation during stationary periods
 * 4. Motion Bias: Online bias estimation during motion
 * 5. Magnetometer (optional): Heading correction with disturbance rejection
 * 
 * Algorithm based on:
 * VQF: Highly Accurate IMU Orientation Estimation with Bias Estimation
 * and Magnetic Disturbance Rejection, Information Fusion 2023
 */

#include "vqf_advanced.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * Constants
 *============================================================================*/

#define DEG2RAD         (3.14159265f / 180.0f)
#define RAD2DEG         (180.0f / 3.14159265f)
#define GRAVITY         9.81f
#define MIN_ACC_NORM    0.5f    // Minimum accel norm for correction
#define MAX_ACC_NORM    1.5f    // Maximum accel norm for correction

/*============================================================================
 * Internal Functions
 *============================================================================*/

// Compute filter gain from time constant
static float compute_gain(float tau, float dt)
{
    if (tau <= 0.0f) return 0.0f;
    return 1.0f - vqf_expf(-dt / tau);
}

// Apply accelerometer correction using gradient descent
static void NO_INLINE apply_accel_correction(vqf_state_t *state, const float acc[3])
{
    float q0 = state->quat[0], q1 = state->quat[1];
    float q2 = state->quat[2], q3 = state->quat[3];
    
    // Normalize accelerometer
    float ax = acc[0], ay = acc[1], az = acc[2];
    float acc_norm = vqf_sqrt(ax*ax + ay*ay + az*az);
    
    // Skip correction if accel norm is too far from 1g
    if (acc_norm < MIN_ACC_NORM || acc_norm > MAX_ACC_NORM) {
        return;
    }
    
    float inv_norm = 1.0f / acc_norm;
    ax *= inv_norm;
    ay *= inv_norm;
    az *= inv_norm;
    
    // Low-pass filter accelerometer
    float alpha = state->k_acc;
    state->acc_lp[0] += alpha * (ax - state->acc_lp[0]);
    state->acc_lp[1] += alpha * (ay - state->acc_lp[1]);
    state->acc_lp[2] += alpha * (az - state->acc_lp[2]);
    
    ax = state->acc_lp[0];
    ay = state->acc_lp[1];
    az = state->acc_lp[2];
    
    // Estimated gravity direction from quaternion
    float vx = 2.0f * (q1*q3 - q0*q2);
    float vy = 2.0f * (q0*q1 + q2*q3);
    float vz = q0*q0 - q1*q1 - q2*q2 + q3*q3;
    
    // Error: cross product of estimated and measured gravity
    float ex = (ay * vz - az * vy);
    float ey = (az * vx - ax * vz);
    float ez = (ax * vy - ay * vx);
    
    // Error magnitude for adaptive gain
    float e_norm = vqf_sqrt(ex*ex + ey*ey + ez*ez);
    
    // Adaptive gain: reduce correction when error is large (motion detected)
    float adaptive_gain = state->k_acc;
    if (e_norm > 0.1f) {
        adaptive_gain *= 0.1f / e_norm;
        if (adaptive_gain < 0.001f) adaptive_gain = 0.001f;
    }
    
    // Apply correction as quaternion derivative
    float qDot0 = -0.5f * (q1*ex + q2*ey + q3*ez) * adaptive_gain;
    float qDot1 =  0.5f * (q0*ex + q2*ez - q3*ey) * adaptive_gain;
    float qDot2 =  0.5f * (q0*ey - q1*ez + q3*ex) * adaptive_gain;
    float qDot3 =  0.5f * (q0*ez + q1*ey - q2*ex) * adaptive_gain;
    
    state->quat[0] += qDot0;
    state->quat[1] += qDot1;
    state->quat[2] += qDot2;
    state->quat[3] += qDot3;
    
    // Normalize quaternion
    vqf_quat_normalize(state->quat);
}

// Gyroscope bias estimation during rest
static void update_bias_at_rest(vqf_state_t *state, const float gyro[3])
{
    // Simple exponential moving average of gyro readings
    float alpha = 0.01f;  // Slow adaptation
    
    state->gyro_bias[0] += alpha * (gyro[0] - state->gyro_bias[0]);
    state->gyro_bias[1] += alpha * (gyro[1] - state->gyro_bias[1]);
    state->gyro_bias[2] += alpha * (gyro[2] - state->gyro_bias[2]);
    
    // Reduce covariance
    state->bias_p[0] *= 0.99f;
    state->bias_p[1] *= 0.99f;
    state->bias_p[2] *= 0.99f;
}

// Gyroscope bias estimation during motion
// v0.6.2: 修复"信噪比陷阱" - Z轴偏差在6轴模式下不可观测
// 问题: 原来对所有轴使用相同的遗忘因子(0.0001)，导致站立时Z轴漂移
// 修复: X/Y轴可从加速度计观测，Z轴(偏航)需要磁力计，6轴模式下大幅降低遗忘速率
static void update_bias_in_motion(vqf_state_t *state, const float gyro[3],
                                   const float error[3])
{
#if VQF_USE_MOTION_BIAS
    // v0.6.2: X/Y轴(可从加速度计观测)使用正常速率
    float alpha_xy = VQF_MOTION_BIAS_ALPHA_XY;
    // v0.6.2: Z轴(偏航)在6轴模式下不可观测，大幅降低遗忘速率
    float alpha_z = VQF_MOTION_BIAS_ALPHA_Z;
    
    // Update bias based on correction error
    // X/Y轴: 加速度计可以提供倾斜信息
    state->bias_motion[0] += alpha_xy * error[0];
    state->bias_motion[1] += alpha_xy * error[1];
    // Z轴: 在6轴模式下几乎不更新，避免遗忘TCal的偏差
    state->bias_motion[2] += alpha_z * error[2];
    
    // Limit motion bias accumulation (v0.6.2: 降低限制)
    float limit = VQF_MOTION_BIAS_LIMIT;
    for (int i = 0; i < 3; i++) {
        if (state->bias_motion[i] > limit) state->bias_motion[i] = limit;
        if (state->bias_motion[i] < -limit) state->bias_motion[i] = -limit;
    }
    
    // v0.6.2: 对不同轴使用不同的应用速率
    // X/Y轴可以较快应用(可观测)，Z轴极慢应用(不可观测)
    state->gyro_bias[0] += 0.001f * state->bias_motion[0];
    state->gyro_bias[1] += 0.001f * state->bias_motion[1];
    state->gyro_bias[2] += 0.0001f * state->bias_motion[2];  // Z轴应用速率降低10倍
    
    // Decay motion bias (防止累积过大)
    state->bias_motion[0] *= 0.99f;
    state->bias_motion[1] *= 0.99f;
    state->bias_motion[2] *= 0.999f;  // Z轴衰减更慢，保持记忆
#else
    (void)state; (void)gyro; (void)error;
#endif
}

// Rest detection
// v0.6.2: 添加滞后机制，防止呼吸/微动导致频繁切换Rest/Motion
static void update_rest_detection(vqf_state_t *state, const float gyro[3],
                                   const float accel[3])
{
#if VQF_USE_REST_DETECTION
    // Compute gyro variance
    float gyro_norm = vqf_vec3_norm(gyro) * RAD2DEG;
    
    // Compute accel deviation from 1g
    float accel_norm = vqf_vec3_norm(accel);
    float accel_dev = fabsf(accel_norm - 1.0f) * GRAVITY;
    
    // v0.6.2: 使用滞后阈值防止频繁切换
    // 进入Rest: 使用严格阈值
    // 退出Rest: 使用宽松阈值(1.5倍)
    float gyro_th = (state->flags & VQF_FLAG_REST) ? 
                    (VQF_REST_GYRO_TH * 1.5f) : VQF_REST_GYRO_TH;
    float accel_th = (state->flags & VQF_FLAG_REST) ? 
                     (VQF_REST_ACCEL_TH * 1.5f) : VQF_REST_ACCEL_TH;
    
    // Check rest conditions
    if (gyro_norm < gyro_th && accel_dev < accel_th) {
        state->rest_time += state->dt;
        
        if (state->rest_time >= VQF_REST_TIME_TH) {
            state->flags |= VQF_FLAG_REST;
        }
    } else {
        // v0.6.2: 不立即退出Rest，需要持续运动一段时间
        // 这避免了呼吸等微动触发Motion模式
        if (state->rest_time > 0.0f) {
            state->rest_time -= state->dt * 2.0f;  // 退出速度是进入的2倍
            if (state->rest_time < 0.0f) {
                state->rest_time = 0.0f;
                state->flags &= ~VQF_FLAG_REST;
            }
        }
    }
    
    // Store last gyro for variance computation
    state->rest_last_gyro[0] = gyro[0];
    state->rest_last_gyro[1] = gyro[1];
    state->rest_last_gyro[2] = gyro[2];
#else
    (void)state; (void)gyro; (void)accel;
#endif
}

#if VQF_USE_MAGNETOMETER
// Magnetometer correction
static void apply_mag_correction(vqf_state_t *state, const float mag[3])
{
    if (state->tau_mag <= 0.0f) return;
    
    float mx = mag[0], my = mag[1], mz = mag[2];
    
    // Normalize magnetometer
    float mag_norm = vqf_sqrt(mx*mx + my*my + mz*mz);
    if (mag_norm < 0.1f) return;
    
    float inv_norm = 1.0f / mag_norm;
    mx *= inv_norm;
    my *= inv_norm;
    mz *= inv_norm;
    
    // Low-pass filter
    float alpha = compute_gain(state->tau_mag * 0.1f, state->dt);
    state->mag_lp[0] += alpha * (mx - state->mag_lp[0]);
    state->mag_lp[1] += alpha * (my - state->mag_lp[1]);
    state->mag_lp[2] += alpha * (mz - state->mag_lp[2]);
    
    mx = state->mag_lp[0];
    my = state->mag_lp[1];
    mz = state->mag_lp[2];
    
    // Transform mag to earth frame
    float mag_earth[3] = {mx, my, mz};
    vqf_quat_rotate(state->quat, mag_earth, mag_earth);
    
    // Compute horizontal component (for heading)
    float hx = mag_earth[0];
    float hy = mag_earth[1];
    float h_norm = vqf_sqrt(hx*hx + hy*hy);
    
    if (h_norm < 0.1f) return;  // Vertical field, skip
    
    // Check for magnetic disturbance
    float dip_angle = fabsf(mag_earth[2]) / mag_norm;
    if (state->mag_ref[2] != 0.0f) {
        float expected_dip = fabsf(state->mag_ref[2]);
        if (fabsf(dip_angle - expected_dip) > 0.3f) {
            state->flags |= VQF_FLAG_MAG_DISTURBED;
            state->mag_dist_time = 0.0f;
            return;  // Skip correction during disturbance
        }
    }
    
    // Clear disturbance flag after settling time
    state->mag_dist_time += state->dt;
    if (state->mag_dist_time > 2.0f) {
        state->flags &= ~VQF_FLAG_MAG_DISTURBED;
    }
    
    // Update reference
    if (!(state->flags & VQF_FLAG_MAG_DISTURBED)) {
        float ref_alpha = 0.001f;
        state->mag_ref[0] += ref_alpha * (hx/h_norm - state->mag_ref[0]);
        state->mag_ref[1] += ref_alpha * (hy/h_norm - state->mag_ref[1]);
        state->mag_ref[2] += ref_alpha * (dip_angle - state->mag_ref[2]);
    }
    
    // Heading error (yaw correction only)
    float heading_error = atan2f(hy, hx);
    
    // Apply correction
    float k_mag = compute_gain(state->tau_mag, state->dt);
    float half_angle = -0.5f * heading_error * k_mag;
    
    // Yaw correction quaternion
    float cz = cosf(half_angle);
    float sz = sinf(half_angle);
    float q_corr[4] = {cz, 0, 0, sz};
    
    // Apply to current quaternion
    float q_new[4];
    vqf_quat_multiply(q_corr, state->quat, q_new);
    
    state->quat[0] = q_new[0];
    state->quat[1] = q_new[1];
    state->quat[2] = q_new[2];
    state->quat[3] = q_new[3];
}
#endif

/*============================================================================
 * Public API
 *============================================================================*/

void vqf_advanced_init(vqf_state_t *state, float dt, float tau_acc, float tau_mag)
{
    memset(state, 0, sizeof(vqf_state_t));
    
    // Identity quaternion
    state->quat[0] = 1.0f;
    state->quat[1] = 0.0f;
    state->quat[2] = 0.0f;
    state->quat[3] = 0.0f;
    
    // Filter parameters
    state->dt = dt;
    state->tau_acc = (tau_acc > 0.0f) ? tau_acc : VQF_TAU_ACC_DEFAULT;
    state->tau_mag = tau_mag;
    state->k_acc = compute_gain(state->tau_acc, dt);
    
    // Initialize accelerometer LP to down
    state->acc_lp[0] = 0.0f;
    state->acc_lp[1] = 0.0f;
    state->acc_lp[2] = 1.0f;
    
    // Initialize bias covariance
    state->bias_p[0] = VQF_BIAS_SIGMA_INIT * VQF_BIAS_SIGMA_INIT;
    state->bias_p[1] = VQF_BIAS_SIGMA_INIT * VQF_BIAS_SIGMA_INIT;
    state->bias_p[2] = VQF_BIAS_SIGMA_INIT * VQF_BIAS_SIGMA_INIT;
    
#if VQF_USE_MAGNETOMETER
    state->mag_ref[0] = 1.0f;  // Initial reference pointing north
    state->mag_ref[1] = 0.0f;
    state->mag_ref[2] = 0.5f;  // Assume ~60° inclination
    state->mag_dist_time = 10.0f;  // Start with settled state
#endif
    
    state->flags = VQF_FLAG_INITIALIZED;
}

void NO_INLINE vqf_advanced_update(vqf_state_t *state, const float gyro[3], const float accel[3])
{
    float dt = state->dt;
    float q0 = state->quat[0], q1 = state->quat[1];
    float q2 = state->quat[2], q3 = state->quat[3];
    
    // Subtract bias from gyroscope
    float gx = gyro[0] - state->gyro_bias[0];
    float gy = gyro[1] - state->gyro_bias[1];
    float gz = gyro[2] - state->gyro_bias[2];
    
    // Update rest detection
    update_rest_detection(state, gyro, accel);
    
    // Gyroscope integration (quaternion derivative)
    float qDot0 = 0.5f * (-q1*gx - q2*gy - q3*gz);
    float qDot1 = 0.5f * ( q0*gx + q2*gz - q3*gy);
    float qDot2 = 0.5f * ( q0*gy - q1*gz + q3*gx);
    float qDot3 = 0.5f * ( q0*gz + q1*gy - q2*gx);
    
    // Integrate quaternion
    state->quat[0] = q0 + qDot0 * dt;
    state->quat[1] = q1 + qDot1 * dt;
    state->quat[2] = q2 + qDot2 * dt;
    state->quat[3] = q3 + qDot3 * dt;
    
    // Normalize
    vqf_quat_normalize(state->quat);
    
    // Apply accelerometer correction
    apply_accel_correction(state, accel);
    
    // Update gyro bias
    if (state->flags & VQF_FLAG_REST) {
        update_bias_at_rest(state, gyro);
    } else {
        // Compute error for motion bias
        float error[3] = {
            accel[0] - state->acc_lp[0],
            accel[1] - state->acc_lp[1],
            accel[2] - state->acc_lp[2]
        };
        update_bias_in_motion(state, gyro, error);
    }
    
    state->sample_count++;
}

void vqf_advanced_update_mag(vqf_state_t *state, const float gyro[3],
                              const float accel[3], const float mag[3])
{
    // First do 6-axis update
    vqf_advanced_update(state, gyro, accel);
    
#if VQF_USE_MAGNETOMETER
    // Then apply magnetometer correction
    if (mag != NULL && state->tau_mag > 0.0f) {
        apply_mag_correction(state, mag);
    }
#else
    (void)mag;
#endif
}

void vqf_advanced_reset(vqf_state_t *state)
{
    float dt = state->dt;
    float tau_acc = state->tau_acc;
    float tau_mag = state->tau_mag;
    
    vqf_advanced_init(state, dt, tau_acc, tau_mag);
}

void vqf_advanced_set_bias(vqf_state_t *state, const float bias[3])
{
    state->gyro_bias[0] = bias[0];
    state->gyro_bias[1] = bias[1];
    state->gyro_bias[2] = bias[2];
    
    // Reset covariance to reflect known bias
    state->bias_p[0] = 0.01f;
    state->bias_p[1] = 0.01f;
    state->bias_p[2] = 0.01f;
}

void vqf_advanced_set_params(vqf_state_t *state, float tau_acc, float tau_mag)
{
    state->tau_acc = tau_acc;
    state->tau_mag = tau_mag;
    state->k_acc = compute_gain(tau_acc, state->dt);
}
