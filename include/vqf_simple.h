/**
 * @file vqf_simple.h
 * @brief Simplified VQF Sensor Fusion Header
 */

#ifndef __VQF_SIMPLE_H__
#define __VQF_SIMPLE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize VQF filter
 * @param sample_time Sample period in seconds
 * @param tau_acc Accelerometer time constant (default 3.0)
 * @param tau_mag Magnetometer time constant (default 9.0)
 */
void vqf_init(float sample_time, float tau_acc, float tau_mag);

/**
 * @brief Update filter with new sensor data
 * @param gyro Gyroscope reading [x, y, z] in deg/s
 * @param accel Accelerometer reading [x, y, z] in g
 * @param mag Magnetometer reading [x, y, z] in uT, or NULL to disable
 */
void vqf_update(const float gyro[3], const float accel[3], const float *mag);

/**
 * @brief Get current orientation as quaternion
 * @param out Output quaternion [w, x, y, z]
 */
void vqf_get_quaternion(float out[4]);

/**
 * @brief Get current orientation as Euler angles
 * @param roll Roll angle in degrees
 * @param pitch Pitch angle in degrees
 * @param yaw Yaw angle in degrees
 */
void vqf_get_euler(float *roll, float *pitch, float *yaw);

/**
 * @brief Reset filter state
 */
void vqf_reset(void);

/**
 * @brief Update sample time
 * @param sample_time New sample period in seconds
 */
void vqf_set_sample_time(float sample_time);

/**
 * @brief Get estimated gyroscope bias
 * @param bias Output bias [x, y, z] in deg/s
 */
void vqf_get_bias(float bias[3]);

#ifdef __cplusplus
}
#endif

#endif /* __VQF_SIMPLE_H__ */
