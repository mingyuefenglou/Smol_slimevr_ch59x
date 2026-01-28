/**
 * @file bmi270.c
 * @brief BMI270 IMU Driver for SlimeVR CH59X Port
 * 
 * This driver is adapted from the original SlimeVR-Tracker-nRF project
 * to work with the CH59X hardware abstraction layer. The core register
 * operations and initialization sequence remain the same, but the
 * interface layer has been abstracted to use hal.h functions.
 * 
 * Original: https://github.com/SlimeVR/SlimeVR-Tracker-nRF
 * License: MIT
 */

#include "bmi270.h"
#include "hal.h"
#include <string.h>

/*============================================================================
 * BMI270 Register Definitions
 *============================================================================*/

#define BMI270_CHIP_ID          0x00
#define BMI270_ERR_REG          0x02
#define BMI270_STATUS           0x03
#define BMI270_DATA_0           0x04  // Aux data
#define BMI270_DATA_8           0x0C  // Accel X LSB
#define BMI270_DATA_14          0x12  // Gyro X LSB
#define BMI270_SENSORTIME_0     0x18
#define BMI270_EVENT            0x1B
#define BMI270_INT_STATUS_0     0x1C
#define BMI270_INT_STATUS_1     0x1D
#define BMI270_INTERNAL_STATUS  0x21
#define BMI270_TEMPERATURE_0    0x22
#define BMI270_FIFO_LENGTH_0    0x24
#define BMI270_FIFO_DATA        0x26
#define BMI270_ACC_CONF         0x40
#define BMI270_ACC_RANGE        0x41
#define BMI270_GYR_CONF         0x42
#define BMI270_GYR_RANGE        0x43
#define BMI270_AUX_CONF         0x44
#define BMI270_FIFO_DOWNS       0x45
#define BMI270_FIFO_WTM_0       0x46
#define BMI270_FIFO_CONFIG_0    0x48
#define BMI270_FIFO_CONFIG_1    0x49
#define BMI270_INT1_IO_CTRL     0x53
#define BMI270_INT2_IO_CTRL     0x54
#define BMI270_INT_LATCH        0x55
#define BMI270_INT1_MAP_FEAT    0x56
#define BMI270_INT2_MAP_FEAT    0x57
#define BMI270_INT_MAP_DATA     0x58
#define BMI270_INIT_CTRL        0x59
#define BMI270_INIT_DATA        0x5E
#define BMI270_PWR_CONF         0x7C
#define BMI270_PWR_CTRL         0x7D
#define BMI270_CMD              0x7E

#define BMI270_CHIP_ID_VAL      0x24

/*============================================================================
 * BMI270 Configuration Firmware (abbreviated for example)
 *============================================================================*/

// Note: Full firmware is ~8KB, included from separate header
// This is just a placeholder - use the actual firmware from the nRF project
static const uint8_t bmi270_config_file[] = {
    // ... BMI270 firmware data goes here ...
    // Copy from: SlimeVR-Tracker-nRF/src/sensor/imu/BMI270_firmware.h
    0x00  // Placeholder
};

/*============================================================================
 * Driver State
 *============================================================================*/

typedef struct {
    uint8_t i2c_addr;
    bool initialized;
    float accel_scale;
    float gyro_scale;
} bmi270_state_t;

static bmi270_state_t bmi270_state = {
    .i2c_addr = 0x68,  // Default address (SDO low)
    .initialized = false,
    .accel_scale = 1.0f / 16384.0f,  // ±2g default
    .gyro_scale = 1.0f / 32768.0f * 2000.0f,  // ±2000°/s default
};

/*============================================================================
 * Private Functions
 *============================================================================*/

static int bmi270_read_reg(uint8_t reg, uint8_t *data, uint16_t len)
{
    return hal_i2c_read_reg(bmi270_state.i2c_addr, reg, data, len);
}

static int bmi270_write_reg(uint8_t reg, uint8_t data)
{
    return hal_i2c_write_reg(bmi270_state.i2c_addr, reg, &data, 1);
}

static int bmi270_write_burst(uint8_t reg, const uint8_t *data, uint16_t len)
{
    return hal_i2c_write_reg(bmi270_state.i2c_addr, reg, data, len);
}

/*============================================================================
 * Public API
 *============================================================================*/

int bmi270_detect(uint8_t addr)
{
    uint8_t chip_id = 0;
    bmi270_state.i2c_addr = addr;
    
    if (bmi270_read_reg(BMI270_CHIP_ID, &chip_id, 1) != 0) {
        return -1;
    }
    
    if (chip_id != BMI270_CHIP_ID_VAL) {
        return -1;
    }
    
    return 0;
}

int bmi270_init(void)
{
    int err;
    
    // Soft reset
    err = bmi270_write_reg(BMI270_CMD, 0xB6);
    if (err) return err;
    hal_delay_ms(2);
    
    // Disable power saving
    err = bmi270_write_reg(BMI270_PWR_CONF, 0x00);
    if (err) return err;
    hal_delay_us(450);
    
    // Prepare for config load
    err = bmi270_write_reg(BMI270_INIT_CTRL, 0x00);
    if (err) return err;
    
    // Load configuration file
    // Note: This needs to be done in chunks due to I2C limitations
    const uint16_t chunk_size = 32;
    for (uint16_t i = 0; i < sizeof(bmi270_config_file); i += chunk_size) {
        uint16_t len = chunk_size;
        if (i + len > sizeof(bmi270_config_file)) {
            len = sizeof(bmi270_config_file) - i;
        }
        err = bmi270_write_burst(BMI270_INIT_DATA, &bmi270_config_file[i], len);
        if (err) return err;
    }
    
    // Complete config load
    err = bmi270_write_reg(BMI270_INIT_CTRL, 0x01);
    if (err) return err;
    hal_delay_ms(20);
    
    // Verify initialization
    uint8_t status;
    err = bmi270_read_reg(BMI270_INTERNAL_STATUS, &status, 1);
    if (err) return err;
    if ((status & 0x0F) != 0x01) {
        return -2;  // Init failed
    }
    
    // Configure accelerometer: 100Hz, continuous mode, normal filter
    err = bmi270_write_reg(BMI270_ACC_CONF, 0xA8);  // odr_100, bwp_norm, perf_mode
    if (err) return err;
    
    // Configure gyroscope: 200Hz, continuous mode, normal filter
    err = bmi270_write_reg(BMI270_GYR_CONF, 0xA9);  // odr_200, bwp_norm, perf_mode
    if (err) return err;
    
    // Set ranges: ±8g accel, ±2000°/s gyro
    err = bmi270_write_reg(BMI270_ACC_RANGE, 0x02);  // ±8g
    if (err) return err;
    bmi270_state.accel_scale = 8.0f / 32768.0f;
    
    err = bmi270_write_reg(BMI270_GYR_RANGE, 0x00);  // ±2000°/s
    if (err) return err;
    bmi270_state.gyro_scale = 2000.0f / 32768.0f;
    
    // Enable accelerometer and gyroscope
    err = bmi270_write_reg(BMI270_PWR_CTRL, 0x0E);
    if (err) return err;
    
    hal_delay_ms(50);
    
    bmi270_state.initialized = true;
    return 0;
}

int bmi270_read_data(bmi270_data_t *data)
{
    if (!bmi270_state.initialized) {
        return -1;
    }
    
    uint8_t buf[12];
    int err = bmi270_read_reg(BMI270_DATA_8, buf, 12);
    if (err) return err;
    
    // Parse accelerometer data (little endian)
    int16_t ax = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t ay = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t az = (int16_t)(buf[4] | (buf[5] << 8));
    
    // Parse gyroscope data
    int16_t gx = (int16_t)(buf[6] | (buf[7] << 8));
    int16_t gy = (int16_t)(buf[8] | (buf[9] << 8));
    int16_t gz = (int16_t)(buf[10] | (buf[11] << 8));
    
    // Convert to physical units
    data->accel[0] = ax * bmi270_state.accel_scale;
    data->accel[1] = ay * bmi270_state.accel_scale;
    data->accel[2] = az * bmi270_state.accel_scale;
    
    data->gyro[0] = gx * bmi270_state.gyro_scale;
    data->gyro[1] = gy * bmi270_state.gyro_scale;
    data->gyro[2] = gz * bmi270_state.gyro_scale;
    
    return 0;
}

int bmi270_read_temperature(float *temp)
{
    uint8_t buf[2];
    int err = bmi270_read_reg(BMI270_TEMPERATURE_0, buf, 2);
    if (err) return err;
    
    int16_t raw = (int16_t)(buf[0] | (buf[1] << 8));
    *temp = (raw / 512.0f) + 23.0f;
    
    return 0;
}

int bmi270_set_odr(uint16_t accel_hz, uint16_t gyro_hz)
{
    // Map Hz to register values
    uint8_t acc_odr, gyr_odr;
    
    if (accel_hz <= 25) acc_odr = 0x06;
    else if (accel_hz <= 50) acc_odr = 0x07;
    else if (accel_hz <= 100) acc_odr = 0x08;
    else if (accel_hz <= 200) acc_odr = 0x09;
    else if (accel_hz <= 400) acc_odr = 0x0A;
    else acc_odr = 0x0B;  // 800Hz
    
    if (gyro_hz <= 25) gyr_odr = 0x06;
    else if (gyro_hz <= 50) gyr_odr = 0x07;
    else if (gyro_hz <= 100) gyr_odr = 0x08;
    else if (gyro_hz <= 200) gyr_odr = 0x09;
    else if (gyro_hz <= 400) gyr_odr = 0x0A;
    else gyr_odr = 0x0B;
    
    int err = bmi270_write_reg(BMI270_ACC_CONF, (acc_odr & 0x0F) | 0xA0);
    if (err) return err;
    
    err = bmi270_write_reg(BMI270_GYR_CONF, (gyr_odr & 0x0F) | 0xA0);
    return err;
}

int bmi270_suspend(void)
{
    return bmi270_write_reg(BMI270_PWR_CONF, 0x03);  // adv_power_save
}

int bmi270_resume(void)
{
    int err = bmi270_write_reg(BMI270_PWR_CONF, 0x00);
    hal_delay_us(450);
    return err;
}
