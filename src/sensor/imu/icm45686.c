/**
 * @file icm45686.c
 * @brief TDK InvenSense ICM-45686 IMU Driver Implementation
 */

#include "icm45686.h"
#include "hal.h"
#include <string.h>

/*============================================================================
 * Static Variables
 *============================================================================*/

static uint8_t device_addr = 0;
static bool use_spi = false;
static icm45686_gyro_range_t current_gyro_range = ICM45686_GYRO_RANGE_2000DPS;
static icm45686_accel_range_t current_accel_range = ICM45686_ACCEL_RANGE_16G;

// Sensitivity lookup tables
static const float gyro_sensitivity[] = {
    16.4f,      // 2000 dps
    32.8f,      // 1000 dps
    65.5f,      // 500 dps
    131.0f,     // 250 dps
    262.0f,     // 125 dps
    524.3f,     // 62.5 dps
    1048.6f,    // 31.25 dps
    2097.2f,    // 15.625 dps
};

static const float accel_sensitivity[] = {
    1024.0f,    // 32g
    2048.0f,    // 16g
    4096.0f,    // 8g
    8192.0f,    // 4g
    16384.0f,   // 2g
};

/*============================================================================
 * Low-Level Register Access
 *============================================================================*/

static int write_reg(uint8_t reg, uint8_t value)
{
    if (use_spi) {
        uint8_t tx[2] = {reg & 0x7F, value};  // Clear R/W bit for write
        return hal_spi_transfer(tx, NULL, 2);
    } else {
        return hal_i2c_write_reg(device_addr, reg, &value, 1);
    }
}

static int read_reg(uint8_t reg, uint8_t *value)
{
    if (use_spi) {
        uint8_t tx[2] = {reg | 0x80, 0};  // Set R/W bit for read
        uint8_t rx[2];
        int err = hal_spi_transfer(tx, rx, 2);
        if (err == 0) {
            *value = rx[1];
        }
        return err;
    } else {
        return hal_i2c_read_reg(device_addr, reg, value, 1);
    }
}

static int read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    if (use_spi) {
        uint8_t tx[32] = {0};
        uint8_t rx[32];
        tx[0] = reg | 0x80;
        int err = hal_spi_transfer(tx, rx, len + 1);
        if (err == 0) {
            memcpy(data, rx + 1, len);
        }
        return err;
    } else {
        return hal_i2c_read_reg(device_addr, reg, data, len);
    }
}

/*============================================================================
 * Detection and Initialization
 *============================================================================*/

int icm45686_detect_i2c(uint8_t i2c_addr)
{
    device_addr = i2c_addr;
    use_spi = false;
    
    uint8_t who_am_i;
    int err = read_reg(ICM45686_REG_WHO_AM_I, &who_am_i);
    
    if (err != 0) {
        return -1;
    }
    
    if (who_am_i != ICM45686_WHO_AM_I_VALUE) {
        return -2;
    }
    
    return 0;
}

int icm45686_detect_spi(void)
{
    use_spi = true;
    device_addr = 0;
    
    uint8_t who_am_i;
    int err = read_reg(ICM45686_REG_WHO_AM_I, &who_am_i);
    
    if (err != 0) {
        use_spi = false;
        return -1;
    }
    
    if (who_am_i != ICM45686_WHO_AM_I_VALUE) {
        use_spi = false;
        return -2;
    }
    
    return 0;
}

int icm45686_init(void)
{
    int err;
    
    // Soft reset
    err = icm45686_soft_reset();
    if (err) return err;
    
    // Wait for reset
    hal_delay_ms(2);
    
    // Configure interface
    // For I2C: disable SPI, for SPI: configure appropriately
    if (!use_spi) {
        err = write_reg(ICM45686_REG_INTF_CONFIG0, 0x00);
        if (err) return err;
    }
    
    // Set gyro config: 2000dps, 200Hz
    err = write_reg(ICM45686_REG_GYRO_CONFIG0, 
                    ICM45686_GYRO_FS_2000DPS | ICM45686_GYRO_ODR_200HZ);
    if (err) return err;
    current_gyro_range = ICM45686_GYRO_RANGE_2000DPS;
    
    // Set accel config: 16g, 200Hz
    err = write_reg(ICM45686_REG_ACCEL_CONFIG0,
                    ICM45686_ACCEL_FS_16G | ICM45686_ACCEL_ODR_200HZ);
    if (err) return err;
    current_accel_range = ICM45686_ACCEL_RANGE_16G;
    
    // Configure gyro filter
    err = write_reg(ICM45686_REG_GYRO_CONFIG1, 0x00);  // Default filter
    if (err) return err;
    
    // Configure accel filter
    err = write_reg(ICM45686_REG_ACCEL_CONFIG1, 0x00);  // Default filter
    if (err) return err;
    
    // Enable both sensors in low-noise mode
    err = write_reg(ICM45686_REG_PWR_MGMT0,
                    ICM45686_PWR_ACCEL_MODE_LN | ICM45686_PWR_GYRO_MODE_LN);
    if (err) return err;
    
    // Wait for sensors to stabilize
    hal_delay_ms(50);
    
    return 0;
}

int icm45686_soft_reset(void)
{
    int err = write_reg(ICM45686_REG_SIGNAL_PATH_RESET, 
                        ICM45686_SOFT_RESET_DEVICE);
    if (err) return err;
    
    hal_delay_ms(2);
    return 0;
}

/*============================================================================
 * Configuration
 *============================================================================*/

int icm45686_set_gyro_range(icm45686_gyro_range_t range)
{
    uint8_t reg_val;
    int err = read_reg(ICM45686_REG_GYRO_CONFIG0, &reg_val);
    if (err) return err;
    
    reg_val = (reg_val & 0x0F) | (range << 5);
    err = write_reg(ICM45686_REG_GYRO_CONFIG0, reg_val);
    if (err) return err;
    
    current_gyro_range = range;
    return 0;
}

int icm45686_set_accel_range(icm45686_accel_range_t range)
{
    uint8_t reg_val;
    int err = read_reg(ICM45686_REG_ACCEL_CONFIG0, &reg_val);
    if (err) return err;
    
    reg_val = (reg_val & 0x0F) | (range << 5);
    err = write_reg(ICM45686_REG_ACCEL_CONFIG0, reg_val);
    if (err) return err;
    
    current_accel_range = range;
    return 0;
}

int icm45686_set_odr(icm45686_odr_t gyro_odr, icm45686_odr_t accel_odr)
{
    int err;
    uint8_t reg_val;
    
    // Set gyro ODR
    err = read_reg(ICM45686_REG_GYRO_CONFIG0, &reg_val);
    if (err) return err;
    reg_val = (reg_val & 0xF0) | (gyro_odr & 0x0F);
    err = write_reg(ICM45686_REG_GYRO_CONFIG0, reg_val);
    if (err) return err;
    
    // Set accel ODR
    err = read_reg(ICM45686_REG_ACCEL_CONFIG0, &reg_val);
    if (err) return err;
    reg_val = (reg_val & 0xF0) | (accel_odr & 0x0F);
    err = write_reg(ICM45686_REG_ACCEL_CONFIG0, reg_val);
    if (err) return err;
    
    return 0;
}

int icm45686_set_odr_hz(uint16_t gyro_hz, uint16_t accel_hz)
{
    icm45686_odr_t gyro_odr, accel_odr;
    
    // Convert Hz to ODR enum for gyro
    if (gyro_hz >= 6400) gyro_odr = ICM45686_ODR_6400HZ;
    else if (gyro_hz >= 3200) gyro_odr = ICM45686_ODR_3200HZ;
    else if (gyro_hz >= 1600) gyro_odr = ICM45686_ODR_1600HZ;
    else if (gyro_hz >= 800) gyro_odr = ICM45686_ODR_800HZ;
    else if (gyro_hz >= 400) gyro_odr = ICM45686_ODR_400HZ;
    else if (gyro_hz >= 200) gyro_odr = ICM45686_ODR_200HZ;
    else if (gyro_hz >= 100) gyro_odr = ICM45686_ODR_100HZ;
    else if (gyro_hz >= 50) gyro_odr = ICM45686_ODR_50HZ;
    else if (gyro_hz >= 25) gyro_odr = ICM45686_ODR_25HZ;
    else gyro_odr = ICM45686_ODR_12_5HZ;
    
    // Convert Hz to ODR enum for accel
    if (accel_hz >= 6400) accel_odr = ICM45686_ODR_6400HZ;
    else if (accel_hz >= 3200) accel_odr = ICM45686_ODR_3200HZ;
    else if (accel_hz >= 1600) accel_odr = ICM45686_ODR_1600HZ;
    else if (accel_hz >= 800) accel_odr = ICM45686_ODR_800HZ;
    else if (accel_hz >= 400) accel_odr = ICM45686_ODR_400HZ;
    else if (accel_hz >= 200) accel_odr = ICM45686_ODR_200HZ;
    else if (accel_hz >= 100) accel_odr = ICM45686_ODR_100HZ;
    else if (accel_hz >= 50) accel_odr = ICM45686_ODR_50HZ;
    else if (accel_hz >= 25) accel_odr = ICM45686_ODR_25HZ;
    else accel_odr = ICM45686_ODR_12_5HZ;
    
    return icm45686_set_odr(gyro_odr, accel_odr);
}

int icm45686_enable_sensors(bool accel_en, bool gyro_en)
{
    uint8_t pwr = 0;
    
    if (accel_en) {
        pwr |= ICM45686_PWR_ACCEL_MODE_LN;
    }
    
    if (gyro_en) {
        pwr |= ICM45686_PWR_GYRO_MODE_LN;
    }
    
    return write_reg(ICM45686_REG_PWR_MGMT0, pwr);
}

/*============================================================================
 * Data Reading
 *============================================================================*/

int icm45686_read_data(icm45686_data_t *data)
{
    if (!data) return -1;
    
    icm45686_raw_data_t raw;
    int err = icm45686_read_raw(&raw);
    if (err) return err;
    
    // Convert gyro (raw to dps)
    float gyro_sens = gyro_sensitivity[current_gyro_range];
    data->gyro[0] = raw.gyro_raw[0] / gyro_sens;
    data->gyro[1] = raw.gyro_raw[1] / gyro_sens;
    data->gyro[2] = raw.gyro_raw[2] / gyro_sens;
    
    // Convert accel (raw to g)
    float accel_sens = accel_sensitivity[current_accel_range];
    data->accel[0] = raw.accel_raw[0] / accel_sens;
    data->accel[1] = raw.accel_raw[1] / accel_sens;
    data->accel[2] = raw.accel_raw[2] / accel_sens;
    
    // Convert temperature
    // Temp = (TEMP_DATA / 128) + 25
    data->temp = (raw.temp_raw / 128.0f) + 25.0f;
    
    return 0;
}

int icm45686_read_raw(icm45686_raw_data_t *data)
{
    if (!data) return -1;
    
    uint8_t buf[14];
    
    // Read all sensor data in one burst (temp + accel + gyro)
    int err = read_regs(ICM45686_REG_TEMP_DATA1, buf, 14);
    if (err) return err;
    
    // Temperature (big-endian)
    data->temp_raw = (int16_t)((buf[0] << 8) | buf[1]);
    
    // Accelerometer (big-endian)
    data->accel_raw[0] = (int16_t)((buf[2] << 8) | buf[3]);
    data->accel_raw[1] = (int16_t)((buf[4] << 8) | buf[5]);
    data->accel_raw[2] = (int16_t)((buf[6] << 8) | buf[7]);
    
    // Gyroscope (big-endian)
    data->gyro_raw[0] = (int16_t)((buf[8] << 8) | buf[9]);
    data->gyro_raw[1] = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_raw[2] = (int16_t)((buf[12] << 8) | buf[13]);
    
    return 0;
}

bool icm45686_data_ready(void)
{
    uint8_t status;
    if (read_reg(ICM45686_REG_INT_STATUS_DRDY, &status) != 0) {
        return false;
    }
    return (status & 0x01) != 0;  // DRDY bit
}

/*============================================================================
 * Power Management
 *============================================================================*/

int icm45686_suspend(void)
{
    // Put both sensors in off mode
    return write_reg(ICM45686_REG_PWR_MGMT0, 
                     ICM45686_PWR_ACCEL_MODE_OFF | ICM45686_PWR_GYRO_MODE_OFF);
}

int icm45686_resume(void)
{
    // Enable both sensors in low-noise mode
    int err = write_reg(ICM45686_REG_PWR_MGMT0,
                        ICM45686_PWR_ACCEL_MODE_LN | ICM45686_PWR_GYRO_MODE_LN);
    if (err) return err;
    
    // Wait for sensors to stabilize
    hal_delay_ms(50);
    return 0;
}

/*============================================================================
 * Interrupt Configuration
 *============================================================================*/

int icm45686_config_interrupt(uint8_t int_num, bool drdy_en)
{
    int err;
    
    // Configure interrupt polarity and drive type
    err = write_reg(ICM45686_REG_INT_CONFIG,
                    ICM45686_INT1_POLARITY_HIGH | ICM45686_INT1_DRIVE_PP |
                    ICM45686_INT2_POLARITY_HIGH | ICM45686_INT2_DRIVE_PP);
    if (err) return err;
    
    // Route data ready to selected interrupt
    if (int_num == 1) {
        uint8_t src = drdy_en ? ICM45686_DRDY_INT1_EN : 0;
        err = write_reg(ICM45686_REG_INT_SOURCE0, src);
    } else {
        // INT2 configuration would go in INT_SOURCE3/4
        uint8_t src = drdy_en ? ICM45686_DRDY_INT1_EN : 0;
        err = write_reg(ICM45686_REG_INT_SOURCE3, src);
    }
    
    return err;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

float icm45686_read_temperature(void)
{
    uint8_t buf[2];
    if (read_regs(ICM45686_REG_TEMP_DATA1, buf, 2) != 0) {
        return 0.0f;
    }
    
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    return (raw / 128.0f) + 25.0f;
}

float icm45686_get_gyro_sensitivity(void)
{
    return gyro_sensitivity[current_gyro_range];
}

float icm45686_get_accel_sensitivity(void)
{
    return accel_sensitivity[current_accel_range];
}
