/**
 * @file icm42688.c
 * @brief ICM42688 IMU Driver for SlimeVR CH59X Port
 * 
 * This driver is adapted from the original SlimeVR-Tracker-nRF project
 * to work with the CH59X hardware abstraction layer.
 * 
 * The ICM42688 is a high-performance 6-axis MEMS motion tracking device
 * from TDK InvenSense with excellent gyroscope performance.
 */

#include "icm42688.h"
#include "hal.h"
#include <string.h>

/*============================================================================
 * ICM42688 Register Definitions
 *============================================================================*/

// User Bank 0
#define ICM42688_DEVICE_CONFIG         0x11
#define ICM42688_DRIVE_CONFIG          0x13
#define ICM42688_INT_CONFIG            0x14
#define ICM42688_FIFO_CONFIG           0x16
#define ICM42688_TEMP_DATA1            0x1D
#define ICM42688_TEMP_DATA0            0x1E
#define ICM42688_ACCEL_DATA_X1         0x1F
#define ICM42688_ACCEL_DATA_X0         0x20
#define ICM42688_ACCEL_DATA_Y1         0x21
#define ICM42688_ACCEL_DATA_Y0         0x22
#define ICM42688_ACCEL_DATA_Z1         0x23
#define ICM42688_ACCEL_DATA_Z0         0x24
#define ICM42688_GYRO_DATA_X1          0x25
#define ICM42688_GYRO_DATA_X0          0x26
#define ICM42688_GYRO_DATA_Y1          0x27
#define ICM42688_GYRO_DATA_Y0          0x28
#define ICM42688_GYRO_DATA_Z1          0x29
#define ICM42688_GYRO_DATA_Z0          0x2A
#define ICM42688_TMST_FSYNCH           0x2B
#define ICM42688_TMST_FSYNCL           0x2C
#define ICM42688_INT_STATUS            0x2D
#define ICM42688_FIFO_COUNTH           0x2E
#define ICM42688_FIFO_COUNTL           0x2F
#define ICM42688_FIFO_DATA             0x30
#define ICM42688_APEX_DATA0            0x31
#define ICM42688_APEX_DATA1            0x32
#define ICM42688_APEX_DATA2            0x33
#define ICM42688_APEX_DATA3            0x34
#define ICM42688_APEX_DATA4            0x35
#define ICM42688_APEX_DATA5            0x36
#define ICM42688_INT_STATUS2           0x37
#define ICM42688_INT_STATUS3           0x38
#define ICM42688_SIGNAL_PATH_RESET     0x4B
#define ICM42688_INTF_CONFIG0          0x4C
#define ICM42688_INTF_CONFIG1          0x4D
#define ICM42688_PWR_MGMT0             0x4E
#define ICM42688_GYRO_CONFIG0          0x4F
#define ICM42688_ACCEL_CONFIG0         0x50
#define ICM42688_GYRO_CONFIG1          0x51
#define ICM42688_GYRO_ACCEL_CONFIG0    0x52
#define ICM42688_ACCEL_CONFIG1         0x53
#define ICM42688_TMST_CONFIG           0x54
#define ICM42688_APEX_CONFIG0          0x56
#define ICM42688_SMD_CONFIG            0x57
#define ICM42688_FIFO_CONFIG1          0x5F
#define ICM42688_FIFO_CONFIG2          0x60
#define ICM42688_FIFO_CONFIG3          0x61
#define ICM42688_FSYNC_CONFIG          0x62
#define ICM42688_INT_CONFIG0           0x63
#define ICM42688_INT_CONFIG1           0x64
#define ICM42688_INT_SOURCE0           0x65
#define ICM42688_INT_SOURCE1           0x66
#define ICM42688_INT_SOURCE3           0x68
#define ICM42688_INT_SOURCE4           0x69
#define ICM42688_FIFO_LOST_PKT0        0x6C
#define ICM42688_FIFO_LOST_PKT1        0x6D
#define ICM42688_SELF_TEST_CONFIG      0x70
#define ICM42688_WHO_AM_I              0x75
#define ICM42688_REG_BANK_SEL          0x76

// Expected WHO_AM_I value
#define ICM42688_WHOAMI_VALUE          0x47

// Power management modes
#define ICM42688_PWR_ACCEL_MODE_OFF    0x00
#define ICM42688_PWR_ACCEL_MODE_LP     0x02
#define ICM42688_PWR_ACCEL_MODE_LN     0x03
#define ICM42688_PWR_GYRO_MODE_OFF     0x00
#define ICM42688_PWR_GYRO_MODE_STBY    0x01
#define ICM42688_PWR_GYRO_MODE_LN      0x03

/*============================================================================
 * Driver State
 *============================================================================*/

typedef enum {
    ICM42688_IF_I2C,
    ICM42688_IF_SPI
} icm42688_interface_t;

typedef struct {
    icm42688_interface_t interface;
    uint8_t i2c_addr;
    bool initialized;
    float accel_scale;
    float gyro_scale;
} icm42688_state_t;

static icm42688_state_t icm42688_state = {
    .interface = ICM42688_IF_I2C,
    .i2c_addr = 0x68,
    .initialized = false,
    .accel_scale = 1.0f / 2048.0f,   // ±16g default
    .gyro_scale = 1.0f / 16.4f,      // ±2000dps default
};

/*============================================================================
 * Private Functions
 *============================================================================*/

static int icm42688_read_reg(uint8_t reg, uint8_t *data, uint16_t len)
{
    if (icm42688_state.interface == ICM42688_IF_SPI) {
        return hal_spi_read_reg(reg, data, len);
    } else {
        return hal_i2c_read_reg(icm42688_state.i2c_addr, reg, data, len);
    }
}

static int icm42688_write_reg(uint8_t reg, uint8_t data)
{
    if (icm42688_state.interface == ICM42688_IF_SPI) {
        return hal_spi_write_reg(reg, &data, 1);
    } else {
        return hal_i2c_write_reg(icm42688_state.i2c_addr, reg, &data, 1);
    }
}

static int icm42688_select_bank(uint8_t bank)
{
    return icm42688_write_reg(ICM42688_REG_BANK_SEL, bank);
}

/*============================================================================
 * Public API
 *============================================================================*/

int icm42688_detect_i2c(uint8_t addr)
{
    uint8_t whoami = 0;
    icm42688_state.interface = ICM42688_IF_I2C;
    icm42688_state.i2c_addr = addr;
    
    if (icm42688_read_reg(ICM42688_WHO_AM_I, &whoami, 1) != 0) {
        return -1;
    }
    
    if (whoami != ICM42688_WHOAMI_VALUE) {
        return -1;
    }
    
    return 0;
}

int icm42688_detect_spi(void)
{
    uint8_t whoami = 0;
    icm42688_state.interface = ICM42688_IF_SPI;
    
    if (icm42688_read_reg(ICM42688_WHO_AM_I, &whoami, 1) != 0) {
        return -1;
    }
    
    if (whoami != ICM42688_WHOAMI_VALUE) {
        return -1;
    }
    
    return 0;
}

int icm42688_init(void)
{
    int err;
    
    // Soft reset
    err = icm42688_write_reg(ICM42688_DEVICE_CONFIG, 0x01);
    if (err) return err;
    hal_delay_ms(1);
    
    // Select Bank 0
    err = icm42688_select_bank(0);
    if (err) return err;
    
    // Configure interface
    err = icm42688_write_reg(ICM42688_INTF_CONFIG0, 0x00);  // 4-wire SPI or I2C
    if (err) return err;
    
    // Configure gyro: ±2000dps, 200Hz ODR
    // FS_SEL = 0 (±2000dps), ODR = 9 (200Hz)
    err = icm42688_write_reg(ICM42688_GYRO_CONFIG0, (0 << 5) | 9);
    if (err) return err;
    icm42688_state.gyro_scale = 2000.0f / 32768.0f;
    
    // Configure accel: ±16g, 200Hz ODR
    // FS_SEL = 0 (±16g), ODR = 9 (200Hz)
    err = icm42688_write_reg(ICM42688_ACCEL_CONFIG0, (0 << 5) | 9);
    if (err) return err;
    icm42688_state.accel_scale = 16.0f / 32768.0f;
    
    // Configure gyro filter: 2nd order low-pass
    err = icm42688_write_reg(ICM42688_GYRO_CONFIG1, 0x00);
    if (err) return err;
    
    // Configure accel filter
    err = icm42688_write_reg(ICM42688_ACCEL_CONFIG1, 0x00);
    if (err) return err;
    
    // Configure power management: Accel + Gyro in Low Noise mode
    err = icm42688_write_reg(ICM42688_PWR_MGMT0, 
                             (ICM42688_PWR_GYRO_MODE_LN << 2) | 
                             ICM42688_PWR_ACCEL_MODE_LN);
    if (err) return err;
    
    hal_delay_ms(45);  // Wait for gyro startup (per datasheet)
    
    // Configure interrupt
    err = icm42688_write_reg(ICM42688_INT_CONFIG, 0x02);  // INT1 push-pull, active high
    if (err) return err;
    
    err = icm42688_write_reg(ICM42688_INT_SOURCE0, 0x08);  // Data ready interrupt
    if (err) return err;
    
    icm42688_state.initialized = true;
    return 0;
}

int icm42688_read_data(icm42688_data_t *data)
{
    if (!icm42688_state.initialized) {
        return -1;
    }
    
    uint8_t buf[14];  // Temp(2) + Accel(6) + Gyro(6)
    
    int err = icm42688_read_reg(ICM42688_TEMP_DATA1, buf, 14);
    if (err) return err;
    
    // Parse temperature (high byte first)
    int16_t temp_raw = (int16_t)((buf[0] << 8) | buf[1]);
    data->temperature = (temp_raw / 132.48f) + 25.0f;
    
    // Parse accelerometer (high byte first)
    int16_t ax = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t ay = (int16_t)((buf[4] << 8) | buf[5]);
    int16_t az = (int16_t)((buf[6] << 8) | buf[7]);
    
    // Parse gyroscope (high byte first)
    int16_t gx = (int16_t)((buf[8] << 8) | buf[9]);
    int16_t gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t gz = (int16_t)((buf[12] << 8) | buf[13]);
    
    // Convert to physical units
    data->accel[0] = ax * icm42688_state.accel_scale;
    data->accel[1] = ay * icm42688_state.accel_scale;
    data->accel[2] = az * icm42688_state.accel_scale;
    
    data->gyro[0] = gx * icm42688_state.gyro_scale;
    data->gyro[1] = gy * icm42688_state.gyro_scale;
    data->gyro[2] = gz * icm42688_state.gyro_scale;
    
    return 0;
}

int icm42688_set_odr(uint16_t accel_hz, uint16_t gyro_hz)
{
    int err;
    uint8_t accel_odr, gyro_odr;
    
    // Map Hz to ODR register value
    // ODR values: 1=32kHz, 2=16kHz, 3=8kHz, 4=4kHz, 5=2kHz, 6=1kHz,
    //             7=200Hz, 8=100Hz, 9=50Hz, 10=25Hz, 11=12.5Hz
    if (gyro_hz >= 1000) gyro_odr = 6;
    else if (gyro_hz >= 500) gyro_odr = 15;  // 500Hz (only for gyro)
    else if (gyro_hz >= 200) gyro_odr = 7;
    else if (gyro_hz >= 100) gyro_odr = 8;
    else if (gyro_hz >= 50) gyro_odr = 9;
    else if (gyro_hz >= 25) gyro_odr = 10;
    else gyro_odr = 11;
    
    if (accel_hz >= 1000) accel_odr = 6;
    else if (accel_hz >= 500) accel_odr = 15;
    else if (accel_hz >= 200) accel_odr = 7;
    else if (accel_hz >= 100) accel_odr = 8;
    else if (accel_hz >= 50) accel_odr = 9;
    else if (accel_hz >= 25) accel_odr = 10;
    else accel_odr = 11;
    
    // Read current config to preserve FS_SEL
    uint8_t gyro_cfg, accel_cfg;
    err = icm42688_read_reg(ICM42688_GYRO_CONFIG0, &gyro_cfg, 1);
    if (err) return err;
    err = icm42688_read_reg(ICM42688_ACCEL_CONFIG0, &accel_cfg, 1);
    if (err) return err;
    
    // Update ODR while preserving FS_SEL
    gyro_cfg = (gyro_cfg & 0xF0) | gyro_odr;
    accel_cfg = (accel_cfg & 0xF0) | accel_odr;
    
    err = icm42688_write_reg(ICM42688_GYRO_CONFIG0, gyro_cfg);
    if (err) return err;
    err = icm42688_write_reg(ICM42688_ACCEL_CONFIG0, accel_cfg);
    
    return err;
}

int icm42688_set_range(uint8_t accel_range, uint16_t gyro_range)
{
    int err;
    uint8_t accel_fs, gyro_fs;
    
    // Map range to FS_SEL
    // Accel: 0=±16g, 1=±8g, 2=±4g, 3=±2g
    switch (accel_range) {
        case 2:  accel_fs = 3; icm42688_state.accel_scale = 2.0f / 32768.0f; break;
        case 4:  accel_fs = 2; icm42688_state.accel_scale = 4.0f / 32768.0f; break;
        case 8:  accel_fs = 1; icm42688_state.accel_scale = 8.0f / 32768.0f; break;
        case 16:
        default: accel_fs = 0; icm42688_state.accel_scale = 16.0f / 32768.0f; break;
    }
    
    // Gyro: 0=±2000dps, 1=±1000dps, 2=±500dps, 3=±250dps, 4=±125dps
    switch (gyro_range) {
        case 125:  gyro_fs = 4; icm42688_state.gyro_scale = 125.0f / 32768.0f; break;
        case 250:  gyro_fs = 3; icm42688_state.gyro_scale = 250.0f / 32768.0f; break;
        case 500:  gyro_fs = 2; icm42688_state.gyro_scale = 500.0f / 32768.0f; break;
        case 1000: gyro_fs = 1; icm42688_state.gyro_scale = 1000.0f / 32768.0f; break;
        case 2000:
        default:   gyro_fs = 0; icm42688_state.gyro_scale = 2000.0f / 32768.0f; break;
    }
    
    // Read current config to preserve ODR
    uint8_t gyro_cfg, accel_cfg;
    err = icm42688_read_reg(ICM42688_GYRO_CONFIG0, &gyro_cfg, 1);
    if (err) return err;
    err = icm42688_read_reg(ICM42688_ACCEL_CONFIG0, &accel_cfg, 1);
    if (err) return err;
    
    // Update FS_SEL while preserving ODR
    gyro_cfg = (gyro_cfg & 0x0F) | (gyro_fs << 5);
    accel_cfg = (accel_cfg & 0x0F) | (accel_fs << 5);
    
    err = icm42688_write_reg(ICM42688_GYRO_CONFIG0, gyro_cfg);
    if (err) return err;
    err = icm42688_write_reg(ICM42688_ACCEL_CONFIG0, accel_cfg);
    
    return err;
}

int icm42688_suspend(void)
{
    // Put accel and gyro in off mode
    return icm42688_write_reg(ICM42688_PWR_MGMT0, 0x00);
}

int icm42688_resume(void)
{
    // Restore low noise mode
    int err = icm42688_write_reg(ICM42688_PWR_MGMT0,
                                  (ICM42688_PWR_GYRO_MODE_LN << 2) |
                                  ICM42688_PWR_ACCEL_MODE_LN);
    if (err) return err;
    hal_delay_ms(45);
    return 0;
}

int icm42688_self_test(void)
{
    // Self-test procedure (simplified)
    // Full implementation would compare with factory trim values
    
    uint8_t st_config;
    int err;
    
    // Enable self-test
    err = icm42688_write_reg(ICM42688_SELF_TEST_CONFIG, 0x7F);
    if (err) return err;
    
    hal_delay_ms(50);
    
    // Read self-test response
    err = icm42688_read_reg(ICM42688_SELF_TEST_CONFIG, &st_config, 1);
    if (err) return err;
    
    // Disable self-test
    err = icm42688_write_reg(ICM42688_SELF_TEST_CONFIG, 0x00);
    if (err) return err;
    
    // Check if self-test passed (simplified check)
    return (st_config != 0) ? 0 : -1;
}

bool icm42688_data_ready(void)
{
    uint8_t status;
    if (icm42688_read_reg(ICM42688_INT_STATUS, &status, 1) != 0) {
        return false;
    }
    return (status & 0x08) != 0;  // DATA_RDY_INT
}
