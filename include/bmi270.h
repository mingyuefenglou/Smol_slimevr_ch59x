/**
 * @file bmi270.h
 * @brief BMI270 IMU Driver Header for SlimeVR CH59X Port
 */

#ifndef __BMI270_H__
#define __BMI270_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BMI270 sensor data structure
 */
typedef struct {
    float accel[3];  // Acceleration in g
    float gyro[3];   // Angular velocity in deg/s
} bmi270_data_t;

/**
 * @brief Detect BMI270 at given I2C address
 * @param addr I2C address (0x68 or 0x69)
 * @return 0 if found, negative on error
 */
int bmi270_detect(uint8_t addr);

/**
 * @brief Initialize BMI270 sensor
 * @return 0 on success, negative on error
 */
int bmi270_init(void);

/**
 * @brief Read accelerometer and gyroscope data
 * @param data Pointer to data structure
 * @return 0 on success, negative on error
 */
int bmi270_read_data(bmi270_data_t *data);

/**
 * @brief Read temperature
 * @param temp Pointer to store temperature in Celsius
 * @return 0 on success, negative on error
 */
int bmi270_read_temperature(float *temp);

/**
 * @brief Set output data rate
 * @param accel_hz Accelerometer ODR in Hz
 * @param gyro_hz Gyroscope ODR in Hz
 * @return 0 on success, negative on error
 */
int bmi270_set_odr(uint16_t accel_hz, uint16_t gyro_hz);

/**
 * @brief Suspend sensor (low power mode)
 */
int bmi270_suspend(void);

/**
 * @brief Resume sensor from suspend
 */
int bmi270_resume(void);

#ifdef __cplusplus
}
#endif

#endif /* __BMI270_H__ */
