/**
 * @file icm45686.h
 * @brief TDK InvenSense ICM-45686 IMU Driver
 * 
 * Driver for the ICM-45686 6-axis IMU featuring ultra-low noise
 * gyroscope and accelerometer with advanced features.
 * 
 * Key specifications:
 * - Gyroscope: ±15.6/31.2/62.5/125/250/500/1000/2000 dps
 * - Accelerometer: ±2/4/8/16/32 g
 * - Ultra-low gyro noise: 0.0028 dps/√Hz
 * - Low power consumption
 * - SPI (up to 24MHz) and I2C (up to 1MHz) interfaces
 */

#ifndef __ICM45686_H__
#define __ICM45686_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * I2C Address
 *============================================================================*/

#define ICM45686_I2C_ADDR_LOW       0x68    // AD0 = GND
#define ICM45686_I2C_ADDR_HIGH      0x69    // AD0 = VDD

/*============================================================================
 * Register Bank 0
 *============================================================================*/

#define ICM45686_REG_DEVICE_CONFIG      0x01
#define ICM45686_REG_SIGNAL_PATH_RESET  0x02
#define ICM45686_REG_DRIVE_CONFIG1      0x03
#define ICM45686_REG_DRIVE_CONFIG2      0x04
#define ICM45686_REG_DRIVE_CONFIG3      0x05
#define ICM45686_REG_INT_CONFIG         0x06

#define ICM45686_REG_TEMP_DATA1         0x09
#define ICM45686_REG_TEMP_DATA0         0x0A
#define ICM45686_REG_ACCEL_DATA_X1      0x0B
#define ICM45686_REG_ACCEL_DATA_X0      0x0C
#define ICM45686_REG_ACCEL_DATA_Y1      0x0D
#define ICM45686_REG_ACCEL_DATA_Y0      0x0E
#define ICM45686_REG_ACCEL_DATA_Z1      0x0F
#define ICM45686_REG_ACCEL_DATA_Z0      0x10
#define ICM45686_REG_GYRO_DATA_X1       0x11
#define ICM45686_REG_GYRO_DATA_X0       0x12
#define ICM45686_REG_GYRO_DATA_Y1       0x13
#define ICM45686_REG_GYRO_DATA_Y0       0x14
#define ICM45686_REG_GYRO_DATA_Z1       0x15
#define ICM45686_REG_GYRO_DATA_Z0       0x16

#define ICM45686_REG_TMST_FSYNCH        0x17
#define ICM45686_REG_TMST_FSYNCL        0x18
#define ICM45686_REG_APEX_DATA4         0x1D
#define ICM45686_REG_APEX_DATA5         0x1E
#define ICM45686_REG_PWR_MGMT0          0x1F
#define ICM45686_REG_GYRO_CONFIG0       0x20
#define ICM45686_REG_ACCEL_CONFIG0      0x21
#define ICM45686_REG_GYRO_CONFIG1       0x23
#define ICM45686_REG_ACCEL_CONFIG1      0x24
#define ICM45686_REG_APEX_CONFIG0       0x25
#define ICM45686_REG_APEX_CONFIG1       0x26

#define ICM45686_REG_WOM_CONFIG         0x27
#define ICM45686_REG_FIFO_CONFIG1       0x28
#define ICM45686_REG_FIFO_CONFIG2       0x29
#define ICM45686_REG_FIFO_CONFIG3       0x2A
#define ICM45686_REG_INT_SOURCE0        0x2B
#define ICM45686_REG_INT_SOURCE1        0x2C
#define ICM45686_REG_INT_SOURCE3        0x2D
#define ICM45686_REG_INT_SOURCE4        0x2E

#define ICM45686_REG_FIFO_LOST_PKT0     0x2F
#define ICM45686_REG_FIFO_LOST_PKT1     0x30
#define ICM45686_REG_APEX_DATA0         0x31
#define ICM45686_REG_APEX_DATA1         0x32
#define ICM45686_REG_APEX_DATA2         0x33
#define ICM45686_REG_APEX_DATA3         0x34
#define ICM45686_REG_INTF_CONFIG0       0x35
#define ICM45686_REG_INTF_CONFIG1       0x36
#define ICM45686_REG_INT_STATUS_DRDY    0x39
#define ICM45686_REG_INT_STATUS         0x3A
#define ICM45686_REG_INT_STATUS2        0x3B
#define ICM45686_REG_INT_STATUS3        0x3C

#define ICM45686_REG_FIFO_COUNTH        0x3D
#define ICM45686_REG_FIFO_COUNTL        0x3E
#define ICM45686_REG_FIFO_DATA          0x3F
#define ICM45686_REG_WHO_AM_I           0x72
#define ICM45686_REG_BLK_SEL_W          0x79
#define ICM45686_REG_MADDR_W            0x7A
#define ICM45686_REG_M_W                0x7B
#define ICM45686_REG_BLK_SEL_R          0x7C
#define ICM45686_REG_MADDR_R            0x7D
#define ICM45686_REG_M_R                0x7E

/*============================================================================
 * Register Values
 *============================================================================*/

// WHO_AM_I value
#define ICM45686_WHO_AM_I_VALUE         0xE9

// PWR_MGMT0
#define ICM45686_PWR_ACCEL_MODE_OFF     (0 << 0)
#define ICM45686_PWR_ACCEL_MODE_LP      (2 << 0)
#define ICM45686_PWR_ACCEL_MODE_LN      (3 << 0)
#define ICM45686_PWR_GYRO_MODE_OFF      (0 << 2)
#define ICM45686_PWR_GYRO_MODE_STANDBY  (1 << 2)
#define ICM45686_PWR_GYRO_MODE_LP       (2 << 2)
#define ICM45686_PWR_GYRO_MODE_LN       (3 << 2)
#define ICM45686_PWR_IDLE               (1 << 4)
#define ICM45686_PWR_TEMP_DIS           (1 << 5)

// GYRO_CONFIG0 - Full Scale Select
#define ICM45686_GYRO_FS_2000DPS        (0 << 5)
#define ICM45686_GYRO_FS_1000DPS        (1 << 5)
#define ICM45686_GYRO_FS_500DPS         (2 << 5)
#define ICM45686_GYRO_FS_250DPS         (3 << 5)
#define ICM45686_GYRO_FS_125DPS         (4 << 5)
#define ICM45686_GYRO_FS_62_5DPS        (5 << 5)
#define ICM45686_GYRO_FS_31_25DPS       (6 << 5)
#define ICM45686_GYRO_FS_15_625DPS      (7 << 5)

// GYRO_CONFIG0 - ODR Select
#define ICM45686_GYRO_ODR_6400HZ        (3 << 0)
#define ICM45686_GYRO_ODR_3200HZ        (4 << 0)
#define ICM45686_GYRO_ODR_1600HZ        (5 << 0)
#define ICM45686_GYRO_ODR_800HZ         (6 << 0)
#define ICM45686_GYRO_ODR_400HZ         (7 << 0)
#define ICM45686_GYRO_ODR_200HZ         (8 << 0)
#define ICM45686_GYRO_ODR_100HZ         (9 << 0)
#define ICM45686_GYRO_ODR_50HZ          (10 << 0)
#define ICM45686_GYRO_ODR_25HZ          (11 << 0)
#define ICM45686_GYRO_ODR_12_5HZ        (12 << 0)

// ACCEL_CONFIG0 - Full Scale Select
#define ICM45686_ACCEL_FS_32G           (0 << 5)
#define ICM45686_ACCEL_FS_16G           (1 << 5)
#define ICM45686_ACCEL_FS_8G            (2 << 5)
#define ICM45686_ACCEL_FS_4G            (3 << 5)
#define ICM45686_ACCEL_FS_2G            (4 << 5)

// ACCEL_CONFIG0 - ODR Select
#define ICM45686_ACCEL_ODR_6400HZ       (3 << 0)
#define ICM45686_ACCEL_ODR_3200HZ       (4 << 0)
#define ICM45686_ACCEL_ODR_1600HZ       (5 << 0)
#define ICM45686_ACCEL_ODR_800HZ        (6 << 0)
#define ICM45686_ACCEL_ODR_400HZ        (7 << 0)
#define ICM45686_ACCEL_ODR_200HZ        (8 << 0)
#define ICM45686_ACCEL_ODR_100HZ        (9 << 0)
#define ICM45686_ACCEL_ODR_50HZ         (10 << 0)
#define ICM45686_ACCEL_ODR_25HZ         (11 << 0)
#define ICM45686_ACCEL_ODR_12_5HZ       (12 << 0)
#define ICM45686_ACCEL_ODR_6_25HZ       (13 << 0)
#define ICM45686_ACCEL_ODR_3_125HZ      (14 << 0)
#define ICM45686_ACCEL_ODR_1_5625HZ     (15 << 0)

// SIGNAL_PATH_RESET
#define ICM45686_SOFT_RESET_DEVICE      (1 << 4)
#define ICM45686_FIFO_FLUSH             (1 << 2)

// INT_CONFIG
#define ICM45686_INT1_POLARITY_HIGH     (0 << 0)
#define ICM45686_INT1_POLARITY_LOW      (1 << 0)
#define ICM45686_INT1_DRIVE_OD          (0 << 1)
#define ICM45686_INT1_DRIVE_PP          (1 << 1)
#define ICM45686_INT2_POLARITY_HIGH     (0 << 2)
#define ICM45686_INT2_POLARITY_LOW      (1 << 2)
#define ICM45686_INT2_DRIVE_OD          (0 << 3)
#define ICM45686_INT2_DRIVE_PP          (1 << 3)

// INT_SOURCE0
#define ICM45686_DRDY_INT1_EN           (1 << 3)
#define ICM45686_FIFO_THS_INT1_EN       (1 << 2)
#define ICM45686_FIFO_FULL_INT1_EN      (1 << 1)
#define ICM45686_AGC_RDY_INT1_EN        (1 << 0)

/*============================================================================
 * Data Structures
 *============================================================================*/

typedef struct {
    float accel[3];     // Acceleration in g
    float gyro[3];      // Angular rate in dps
    float temp;         // Temperature in °C
} icm45686_data_t;

typedef struct {
    int16_t accel_raw[3];
    int16_t gyro_raw[3];
    int16_t temp_raw;
} icm45686_raw_data_t;

typedef enum {
    ICM45686_GYRO_RANGE_2000DPS = 0,
    ICM45686_GYRO_RANGE_1000DPS,
    ICM45686_GYRO_RANGE_500DPS,
    ICM45686_GYRO_RANGE_250DPS,
    ICM45686_GYRO_RANGE_125DPS,
    ICM45686_GYRO_RANGE_62_5DPS,
    ICM45686_GYRO_RANGE_31_25DPS,
    ICM45686_GYRO_RANGE_15_625DPS,
} icm45686_gyro_range_t;

typedef enum {
    ICM45686_ACCEL_RANGE_32G = 0,
    ICM45686_ACCEL_RANGE_16G,
    ICM45686_ACCEL_RANGE_8G,
    ICM45686_ACCEL_RANGE_4G,
    ICM45686_ACCEL_RANGE_2G,
} icm45686_accel_range_t;

typedef enum {
    ICM45686_ODR_6400HZ = 3,
    ICM45686_ODR_3200HZ = 4,
    ICM45686_ODR_1600HZ = 5,
    ICM45686_ODR_800HZ = 6,
    ICM45686_ODR_400HZ = 7,
    ICM45686_ODR_200HZ = 8,
    ICM45686_ODR_100HZ = 9,
    ICM45686_ODR_50HZ = 10,
    ICM45686_ODR_25HZ = 11,
    ICM45686_ODR_12_5HZ = 12,
} icm45686_odr_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Detect ICM-45686 on I2C bus
 * @param i2c_addr I2C address (0x68 or 0x69)
 * @return 0 if detected, negative on error
 */
int icm45686_detect_i2c(uint8_t i2c_addr);

/**
 * @brief Detect ICM-45686 on SPI bus
 * @return 0 if detected, negative on error
 */
int icm45686_detect_spi(void);

/**
 * @brief Initialize ICM-45686 with default settings
 * @return 0 on success, negative on error
 */
int icm45686_init(void);

/**
 * @brief Perform soft reset
 * @return 0 on success
 */
int icm45686_soft_reset(void);

/**
 * @brief Set gyroscope full-scale range
 * @param range Full-scale range enum
 * @return 0 on success
 */
int icm45686_set_gyro_range(icm45686_gyro_range_t range);

/**
 * @brief Set accelerometer full-scale range
 * @param range Full-scale range enum
 * @return 0 on success
 */
int icm45686_set_accel_range(icm45686_accel_range_t range);

/**
 * @brief Set output data rate for both sensors
 * @param gyro_odr Gyroscope ODR
 * @param accel_odr Accelerometer ODR
 * @return 0 on success
 */
int icm45686_set_odr(icm45686_odr_t gyro_odr, icm45686_odr_t accel_odr);

/**
 * @brief Set ODR by frequency value
 * @param gyro_hz Gyroscope rate in Hz
 * @param accel_hz Accelerometer rate in Hz
 * @return 0 on success
 */
int icm45686_set_odr_hz(uint16_t gyro_hz, uint16_t accel_hz);

/**
 * @brief Enable/disable sensors
 * @param accel_en Enable accelerometer
 * @param gyro_en Enable gyroscope
 * @return 0 on success
 */
int icm45686_enable_sensors(bool accel_en, bool gyro_en);

/**
 * @brief Read sensor data (converted to physical units)
 * @param data Pointer to data structure
 * @return 0 on success
 */
int icm45686_read_data(icm45686_data_t *data);

/**
 * @brief Read raw sensor data
 * @param data Pointer to raw data structure
 * @return 0 on success
 */
int icm45686_read_raw(icm45686_raw_data_t *data);

/**
 * @brief Check if data is ready
 * @return true if data ready
 */
bool icm45686_data_ready(void);

/**
 * @brief Enter low-power mode
 * @return 0 on success
 */
int icm45686_suspend(void);

/**
 * @brief Exit low-power mode
 * @return 0 on success
 */
int icm45686_resume(void);

/**
 * @brief Configure interrupt pin
 * @param int_num Interrupt number (1 or 2)
 * @param drdy_en Enable data ready interrupt
 * @return 0 on success
 */
int icm45686_config_interrupt(uint8_t int_num, bool drdy_en);

/**
 * @brief Read temperature
 * @return Temperature in °C
 */
float icm45686_read_temperature(void);

/**
 * @brief Get gyroscope sensitivity (dps per LSB)
 * @return Sensitivity value
 */
float icm45686_get_gyro_sensitivity(void);

/**
 * @brief Get accelerometer sensitivity (g per LSB)
 * @return Sensitivity value
 */
float icm45686_get_accel_sensitivity(void);

#ifdef __cplusplus
}
#endif

#endif /* __ICM45686_H__ */
