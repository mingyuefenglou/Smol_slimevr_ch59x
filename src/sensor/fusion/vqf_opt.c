/**
 * @file vqf_opt.c
 * @brief Optimized VQF Sensor Fusion Implementation
 * 
 * Simplified Madgwick-style AHRS with:
 * - Gyroscope integration
 * - Accelerometer tilt correction
 * - Optional magnetometer heading
 * - Automatic gyro bias estimation
 */

#include "vqf_opt.h"
#include <string.h>

/*============================================================================
 * Implementation
 *============================================================================*/

void vqf_opt_init(vqf_opt_state_t *state, float dt, float tau_acc, float tau_mag)
{
    memset(state, 0, sizeof(vqf_opt_state_t));
    
    // Identity quaternion
    state->quat[0] = 1.0f;
    state->quat[1] = 0.0f;
    state->quat[2] = 0.0f;
    state->quat[3] = 0.0f;
    
    state->dt = dt;
    state->tau_acc = tau_acc;
    state->tau_mag = tau_mag;
    
    // Calculate filter gains from time constants
    // beta = sqrt(3/4) * (1/tau) for Madgwick
    state->beta = 0.866f / tau_acc;
    state->zeta = 0.0f;  // Gyro bias estimation gain
    
    // Initialize accel LP filter
    state->acc_lp[0] = 0.0f;
    state->acc_lp[1] = 0.0f;
    state->acc_lp[2] = 1.0f;
}

void vqf_opt_reset(vqf_opt_state_t *state)
{
    state->quat[0] = 1.0f;
    state->quat[1] = 0.0f;
    state->quat[2] = 0.0f;
    state->quat[3] = 0.0f;
    
    state->gyro_bias[0] = 0.0f;
    state->gyro_bias[1] = 0.0f;
    state->gyro_bias[2] = 0.0f;
    
    state->sample_count = 0;
}

void NO_INLINE vqf_opt_update(vqf_opt_state_t *state, const float gyro[3],
                               const float accel[3], const float mag[3])
{
    float q0 = state->quat[0];
    float q1 = state->quat[1];
    float q2 = state->quat[2];
    float q3 = state->quat[3];
    float dt = state->dt;
    float beta = state->beta;
    
    // Subtract gyro bias
    float gx = gyro[0] - state->gyro_bias[0];
    float gy = gyro[1] - state->gyro_bias[1];
    float gz = gyro[2] - state->gyro_bias[2];
    
    // Rate of change of quaternion from gyroscope
    float qDot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    float qDot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    float qDot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    float qDot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);
    
    // Compute accelerometer correction
    float ax = accel[0], ay = accel[1], az = accel[2];
    float acc_norm = fast_invsqrt(ax*ax + ay*ay + az*az);
    
    if (acc_norm > 0.0f && acc_norm < 1e10f) {
        ax *= acc_norm;
        ay *= acc_norm;
        az *= acc_norm;
        
        // Low-pass filter accelerometer
        float alpha = dt / (state->tau_acc + dt);
        state->acc_lp[0] += alpha * (ax - state->acc_lp[0]);
        state->acc_lp[1] += alpha * (ay - state->acc_lp[1]);
        state->acc_lp[2] += alpha * (az - state->acc_lp[2]);
        
        ax = state->acc_lp[0];
        ay = state->acc_lp[1];
        az = state->acc_lp[2];
        
        // Estimated direction of gravity
        float vx = 2.0f * (q1 * q3 - q0 * q2);
        float vy = 2.0f * (q0 * q1 + q2 * q3);
        float vz = q0*q0 - q1*q1 - q2*q2 + q3*q3;
        
        // Error is cross product between estimated and measured gravity
        float ex = (ay * vz - az * vy);
        float ey = (az * vx - ax * vz);
        float ez = (ax * vy - ay * vx);
        
        // Apply proportional feedback
        qDot0 -= beta * (q1 * ex + q2 * ey + q3 * ez) * 0.5f;
        qDot1 += beta * (q0 * ex + q2 * ez - q3 * ey) * 0.5f;
        qDot2 += beta * (q0 * ey - q1 * ez + q3 * ex) * 0.5f;
        qDot3 += beta * (q0 * ez + q1 * ey - q2 * ex) * 0.5f;
        
        // Update gyro bias estimate (slow adaptation)
        if (state->sample_count > 200) {
            float bias_alpha = 0.0001f;
            state->gyro_bias[0] += bias_alpha * ex;
            state->gyro_bias[1] += bias_alpha * ey;
            state->gyro_bias[2] += bias_alpha * ez;
        }
    }
    
    // Optional magnetometer correction
    if (mag != NULL && state->tau_mag > 0.0f) {
        float mx = mag[0], my = mag[1], mz = mag[2];
        float mag_norm = fast_invsqrt(mx*mx + my*my + mz*mz);
        
        if (mag_norm > 0.0f && mag_norm < 1e10f) {
            mx *= mag_norm;
            my *= mag_norm;
            mz *= mag_norm;
            
            // Reference direction of Earth's magnetic field
            float hx = 2.0f * (mx * (0.5f - q2*q2 - q3*q3) + my * (q1*q2 - q0*q3) + mz * (q1*q3 + q0*q2));
            float hy = 2.0f * (mx * (q1*q2 + q0*q3) + my * (0.5f - q1*q1 - q3*q3) + mz * (q2*q3 - q0*q1));
            float bx = fast_sqrt(hx*hx + hy*hy);
            float bz = 2.0f * (mx * (q1*q3 - q0*q2) + my * (q2*q3 + q0*q1) + mz * (0.5f - q1*q1 - q2*q2));
            
            // Estimated direction of magnetic field
            float wx = bx * (0.5f - q2*q2 - q3*q3) + bz * (q1*q3 - q0*q2);
            float wy = bx * (q1*q2 - q0*q3) + bz * (q0*q1 + q2*q3);
            float wz = bx * (q0*q2 + q1*q3) + bz * (0.5f - q1*q1 - q2*q2);
            
            // Error
            float ex = (my * wz - mz * wy);
            float ey = (mz * wx - mx * wz);
            float ez = (mx * wy - my * wx);
            
            float mag_beta = 0.866f / state->tau_mag;
            qDot0 -= mag_beta * (q1 * ex + q2 * ey + q3 * ez) * 0.5f;
            qDot1 += mag_beta * (q0 * ex + q2 * ez - q3 * ey) * 0.5f;
            qDot2 += mag_beta * (q0 * ey - q1 * ez + q3 * ex) * 0.5f;
            qDot3 += mag_beta * (q0 * ez + q1 * ey - q2 * ex) * 0.5f;
        }
    }
    
    // Integrate rate of change of quaternion
    q0 += qDot0 * dt;
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;
    
    // Normalize quaternion
    float inv_norm = fast_invsqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    state->quat[0] = q0 * inv_norm;
    state->quat[1] = q1 * inv_norm;
    state->quat[2] = q2 * inv_norm;
    state->quat[3] = q3 * inv_norm;
    
    state->sample_count++;
}
