/**
 * @file vqf_simple.c
 * @brief Simplified VQF Sensor Fusion for SlimeVR CH59X
 * 
 * This is a simplified implementation of the VQF (Versatile Quaternion-based
 * Filter) algorithm optimized for resource-constrained MCUs like CH59X.
 * 
 * The full VQF algorithm is described in:
 * D. Laidig and T. Seel, "VQF: Highly Accurate IMU Orientation Estimation
 * with Bias Estimation and Magnetic Disturbance Rejection"
 * 
 * For full accuracy, consider using the complete implementation from:
 * https://github.com/dlaidig/vqf
 */

#include "vqf_simple.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>

/*============================================================================
 * Constants
 *============================================================================*/

#define DEG_TO_RAD (3.14159265358979323846f / 180.0f)
#define RAD_TO_DEG (180.0f / 3.14159265358979323846f)

/*============================================================================
 * Helper Functions
 *============================================================================*/

static inline float fast_sqrt(float x)
{
    // Newton-Raphson approximation for sqrt
    if (x <= 0.0f) return 0.0f;
    float guess = x * 0.5f;
    for (int i = 0; i < 3; i++) {
        guess = 0.5f * (guess + x / guess);
    }
    return guess;
}

static void quat_normalize(float q[4])
{
    float norm = fast_sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (norm > 1e-10f) {
        float inv = 1.0f / norm;
        q[0] *= inv;
        q[1] *= inv;
        q[2] *= inv;
        q[3] *= inv;
    }
}

static void quat_multiply(const float q1[4], const float q2[4], float out[4])
{
    out[0] = q1[0]*q2[0] - q1[1]*q2[1] - q1[2]*q2[2] - q1[3]*q2[3];
    out[1] = q1[0]*q2[1] + q1[1]*q2[0] + q1[2]*q2[3] - q1[3]*q2[2];
    out[2] = q1[0]*q2[2] - q1[1]*q2[3] + q1[2]*q2[0] + q1[3]*q2[1];
    out[3] = q1[0]*q2[3] + q1[1]*q2[2] - q1[2]*q2[1] + q1[3]*q2[0];
}

static void rotate_vector(const float q[4], const float v[3], float out[3])
{
    // Rotate vector v by quaternion q: q * v * q^-1
    float qv[4] = {0, v[0], v[1], v[2]};
    float qc[4] = {q[0], -q[1], -q[2], -q[3]};  // conjugate
    float temp[4], result[4];
    
    quat_multiply(q, qv, temp);
    quat_multiply(temp, qc, result);
    
    out[0] = result[1];
    out[1] = result[2];
    out[2] = result[3];
}

/*============================================================================
 * VQF State
 *============================================================================*/

typedef struct {
    float quat[4];           // Orientation quaternion (w, x, y, z)
    float gyro_bias[3];      // Estimated gyroscope bias
    float delta_bias[3];     // Bias correction
    float tau_acc;           // Accelerometer time constant
    float tau_mag;           // Magnetometer time constant
    float beta;              // Gyro integration gain
    float sample_time;       // Sample period in seconds
    bool initialized;
} vqf_state_t;

static vqf_state_t vqf = {
    .quat = {1.0f, 0.0f, 0.0f, 0.0f},
    .gyro_bias = {0.0f, 0.0f, 0.0f},
    .delta_bias = {0.0f, 0.0f, 0.0f},
    .tau_acc = 3.0f,
    .tau_mag = 9.0f,
    .beta = 1.0f,
    .sample_time = 0.005f,  // 200Hz default
    .initialized = false
};

/*============================================================================
 * Public API
 *============================================================================*/

void vqf_init(float sample_time, float tau_acc, float tau_mag)
{
    vqf.quat[0] = 1.0f;
    vqf.quat[1] = 0.0f;
    vqf.quat[2] = 0.0f;
    vqf.quat[3] = 0.0f;
    
    memset(vqf.gyro_bias, 0, sizeof(vqf.gyro_bias));
    memset(vqf.delta_bias, 0, sizeof(vqf.delta_bias));
    
    vqf.sample_time = sample_time;
    vqf.tau_acc = tau_acc > 0 ? tau_acc : 3.0f;
    vqf.tau_mag = tau_mag > 0 ? tau_mag : 9.0f;
    
    vqf.initialized = true;
}

void vqf_update(const float gyro[3], const float accel[3], const float *mag)
{
    if (!vqf.initialized) {
        vqf_init(0.005f, 3.0f, 9.0f);
    }
    
    float dt = vqf.sample_time;
    
    // Bias-corrected gyroscope
    float gyr[3] = {
        (gyro[0] - vqf.gyro_bias[0]) * DEG_TO_RAD,
        (gyro[1] - vqf.gyro_bias[1]) * DEG_TO_RAD,
        (gyro[2] - vqf.gyro_bias[2]) * DEG_TO_RAD
    };
    
    // Gyroscope integration using quaternion derivative
    float omega_mag = fast_sqrt(gyr[0]*gyr[0] + gyr[1]*gyr[1] + gyr[2]*gyr[2]);
    
    if (omega_mag > 1e-10f) {
        float half_angle = 0.5f * omega_mag * dt;
        float sin_half = sinf(half_angle);
        float cos_half = cosf(half_angle);
        
        float dq[4] = {
            cos_half,
            sin_half * gyr[0] / omega_mag,
            sin_half * gyr[1] / omega_mag,
            sin_half * gyr[2] / omega_mag
        };
        
        float new_quat[4];
        quat_multiply(vqf.quat, dq, new_quat);
        memcpy(vqf.quat, new_quat, sizeof(vqf.quat));
        quat_normalize(vqf.quat);
    }
    
    // Accelerometer correction (gravity vector alignment)
    float acc_norm = fast_sqrt(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
    
    if (acc_norm > 0.5f && acc_norm < 1.5f) {
        // Normalize acceleration
        float acc_n[3] = {accel[0]/acc_norm, accel[1]/acc_norm, accel[2]/acc_norm};
        
        // Expected gravity in sensor frame
        float g_expected[3] = {0.0f, 0.0f, 1.0f};
        float g_sensor[3];
        
        // Rotate expected gravity to sensor frame
        float qc[4] = {vqf.quat[0], -vqf.quat[1], -vqf.quat[2], -vqf.quat[3]};
        rotate_vector(qc, g_expected, g_sensor);
        
        // Error is cross product of measured and expected
        float error[3] = {
            acc_n[1] * g_sensor[2] - acc_n[2] * g_sensor[1],
            acc_n[2] * g_sensor[0] - acc_n[0] * g_sensor[2],
            acc_n[0] * g_sensor[1] - acc_n[1] * g_sensor[0]
        };
        
        // Apply correction
        float k_acc = dt / vqf.tau_acc;
        float correction[3] = {
            error[0] * k_acc,
            error[1] * k_acc,
            error[2] * k_acc
        };
        
        // Apply as small rotation
        float corr_mag = fast_sqrt(correction[0]*correction[0] + 
                                   correction[1]*correction[1] + 
                                   correction[2]*correction[2]);
        
        if (corr_mag > 1e-10f) {
            float half_corr = 0.5f * corr_mag;
            float sin_half = sinf(half_corr);
            float cos_half = cosf(half_corr);
            
            float dq_corr[4] = {
                cos_half,
                sin_half * correction[0] / corr_mag,
                sin_half * correction[1] / corr_mag,
                sin_half * correction[2] / corr_mag
            };
            
            float new_quat[4];
            quat_multiply(dq_corr, vqf.quat, new_quat);
            memcpy(vqf.quat, new_quat, sizeof(vqf.quat));
            quat_normalize(vqf.quat);
        }
        
        // Bias estimation (slow adaptation)
        float k_bias = dt * 0.01f;  // Very slow bias update
        vqf.gyro_bias[0] += error[0] * k_bias;
        vqf.gyro_bias[1] += error[1] * k_bias;
        vqf.gyro_bias[2] += error[2] * k_bias;
    }
    
    // Magnetometer correction (heading only)
    if (mag != NULL) {
        float mag_norm = fast_sqrt(mag[0]*mag[0] + mag[1]*mag[1] + mag[2]*mag[2]);
        
        if (mag_norm > 10.0f && mag_norm < 100.0f) {
            // Normalize magnetometer
            float mag_n[3] = {mag[0]/mag_norm, mag[1]/mag_norm, mag[2]/mag_norm};
            
            // Rotate to world frame
            float mag_world[3];
            rotate_vector(vqf.quat, mag_n, mag_world);
            
            // Project to horizontal plane
            float heading_error = atan2f(mag_world[1], mag_world[0]);
            
            // Apply yaw correction only
            float k_mag = dt / vqf.tau_mag;
            float yaw_corr = heading_error * k_mag * 0.5f;
            
            // Rotation around world Z axis
            float dq_yaw[4] = {
                cosf(yaw_corr),
                0.0f,
                0.0f,
                sinf(yaw_corr)
            };
            
            float new_quat[4];
            quat_multiply(dq_yaw, vqf.quat, new_quat);
            memcpy(vqf.quat, new_quat, sizeof(vqf.quat));
            quat_normalize(vqf.quat);
        }
    }
}

void vqf_get_quaternion(float out[4])
{
    memcpy(out, vqf.quat, sizeof(vqf.quat));
}

void vqf_get_euler(float *roll, float *pitch, float *yaw)
{
    float q0 = vqf.quat[0];
    float q1 = vqf.quat[1];
    float q2 = vqf.quat[2];
    float q3 = vqf.quat[3];
    
    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (q0 * q1 + q2 * q3);
    float cosr_cosp = 1.0f - 2.0f * (q1 * q1 + q2 * q2);
    *roll = atan2f(sinr_cosp, cosr_cosp) * RAD_TO_DEG;
    
    // Pitch (y-axis rotation)
    float sinp = 2.0f * (q0 * q2 - q3 * q1);
    if (fabsf(sinp) >= 1.0f)
        *pitch = copysignf(90.0f, sinp);
    else
        *pitch = asinf(sinp) * RAD_TO_DEG;
    
    // Yaw (z-axis rotation)
    float siny_cosp = 2.0f * (q0 * q3 + q1 * q2);
    float cosy_cosp = 1.0f - 2.0f * (q2 * q2 + q3 * q3);
    *yaw = atan2f(siny_cosp, cosy_cosp) * RAD_TO_DEG;
}

void vqf_reset(void)
{
    vqf.quat[0] = 1.0f;
    vqf.quat[1] = 0.0f;
    vqf.quat[2] = 0.0f;
    vqf.quat[3] = 0.0f;
    
    memset(vqf.gyro_bias, 0, sizeof(vqf.gyro_bias));
}

void vqf_set_sample_time(float sample_time)
{
    vqf.sample_time = sample_time;
}

void vqf_get_bias(float bias[3])
{
    memcpy(bias, vqf.gyro_bias, sizeof(vqf.gyro_bias));
}
