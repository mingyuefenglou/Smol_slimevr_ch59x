/**
 * @file icm42688.h
 * @brief ICM42688 IMU Driver Header for SlimeVR CH59X Port
 */

#ifndef __ICM42688_H__
#define __ICM42688_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ICM42688 sensor data structure
 */
typedef struct {
    float accel[3];      // Acceleration in g
    float gyro[3];       // Angular velocity in deg/s
    float temperature;   // Temperature in Celsius
} icm42688_data_t;

/**
 * @brief Detect ICM42688 via I2C at given address
 * @param addr I2C address (0x68 or 0x69)
 * @return 0 if found, negative on error
 */
int icm42688_detect_i2c(uint8_t addr);

/**
 * @brief Detect ICM42688 via SPI
 * @return 0 if found, negative on error
 */
int icm42688_detect_spi(void);

/**
 * @brief Initialize ICM42688 sensor
 * @return 0 on success, negative on error
 */
int icm42688_init(void);

/**
 * @brief Read accelerometer and gyroscope data
 * @param data Pointer to data structure
 * @return 0 on success, negative on error
 */
int icm42688_read_data(icm42688_data_t *data);

/**
 * @brief Set output data rate
 * @param accel_hz Accelerometer ODR in Hz
 * @param gyro_hz Gyroscope ODR in Hz
 * @return 0 on success, negative on error
 */
int icm42688_set_odr(uint16_t accel_hz, uint16_t gyro_hz);

/**
 * @brief Set full scale range
 * @param accel_range Accelerometer range (2, 4, 8, or 16 g)
 * @param gyro_range Gyroscope range (125, 250, 500, 1000, or 2000 dps)
 * @return 0 on success, negative on error
 */
int icm42688_set_range(uint8_t accel_range, uint16_t gyro_range);

/**
 * @brief Suspend sensor (low power mode)
 */
int icm42688_suspend(void);

/**
 * @brief Resume sensor from suspend
 */
int icm42688_resume(void);

/**
 * @brief Run self-test
 * @return 0 on pass, negative on fail
 */
int icm42688_self_test(void);

/**
 * @brief Check if data is ready
 * @return true if new data available
 */
bool icm42688_data_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __ICM42688_H__ */
