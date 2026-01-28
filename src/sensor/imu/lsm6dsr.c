/**
 * @file lsm6dsr.c
 * @brief LSM6DSR IMU Driver for CH592
 * 
 * ST LSM6DSR 6-DoF IMU (Accelerometer + Gyroscope)
 * - 陀螺仪: ±125/±250/±500/±1000/±2000 dps
 * - 加速度计: ±2/±4/±8/±16 g
 * - ODR: 最高 6.66kHz
 * - 低功耗模式支持
 * 
 * 通信接口: SPI (最高 10MHz)
 */

#include "lsm6dsr.h"
#include "hal.h"
#include <string.h>

/*============================================================================
 * LSM6DSR 寄存器定义 / Register Definitions
 *============================================================================*/

// 设备 ID / Device ID
#define LSM6DSR_WHO_AM_I            0x0F
#define LSM6DSR_WHO_AM_I_VALUE      0x6B

// 控制寄存器 / Control registers
#define LSM6DSR_CTRL1_XL            0x10    // 加速度计控制
#define LSM6DSR_CTRL2_G             0x11    // 陀螺仪控制
#define LSM6DSR_CTRL3_C             0x12    // 控制 3
#define LSM6DSR_CTRL4_C             0x13    // 控制 4
#define LSM6DSR_CTRL5_C             0x14    // 控制 5
#define LSM6DSR_CTRL6_C             0x15    // 陀螺仪配置
#define LSM6DSR_CTRL7_G             0x16    // 陀螺仪配置 2
#define LSM6DSR_CTRL8_XL            0x17    // 加速度计配置
#define LSM6DSR_CTRL9_XL            0x18    // 加速度计配置 2
#define LSM6DSR_CTRL10_C            0x19    // 控制 10

// 状态寄存器 / Status register
#define LSM6DSR_STATUS_REG          0x1E

// 输出寄存器 / Output registers
#define LSM6DSR_OUT_TEMP_L          0x20
#define LSM6DSR_OUT_TEMP_H          0x21
#define LSM6DSR_OUTX_L_G            0x22
#define LSM6DSR_OUTX_H_G            0x23
#define LSM6DSR_OUTY_L_G            0x24
#define LSM6DSR_OUTY_H_G            0x25
#define LSM6DSR_OUTZ_L_G            0x26
#define LSM6DSR_OUTZ_H_G            0x27
#define LSM6DSR_OUTX_L_A            0x28
#define LSM6DSR_OUTX_H_A            0x29
#define LSM6DSR_OUTY_L_A            0x2A
#define LSM6DSR_OUTY_H_A            0x2B
#define LSM6DSR_OUTZ_L_A            0x2C
#define LSM6DSR_OUTZ_H_A            0x2D

// FIFO 寄存器 / FIFO registers
#define LSM6DSR_FIFO_CTRL1          0x07
#define LSM6DSR_FIFO_CTRL2          0x08
#define LSM6DSR_FIFO_CTRL3          0x09
#define LSM6DSR_FIFO_CTRL4          0x0A
#define LSM6DSR_FIFO_STATUS1        0x3A
#define LSM6DSR_FIFO_STATUS2        0x3B

/*============================================================================
 * 配置常量 / Configuration Constants
 *============================================================================*/

// ODR 设置
typedef enum {
    LSM6DSR_ODR_OFF      = 0x00,
    LSM6DSR_ODR_12_5Hz   = 0x01,
    LSM6DSR_ODR_26Hz     = 0x02,
    LSM6DSR_ODR_52Hz     = 0x03,
    LSM6DSR_ODR_104Hz    = 0x04,
    LSM6DSR_ODR_208Hz    = 0x05,
    LSM6DSR_ODR_416Hz    = 0x06,
    LSM6DSR_ODR_833Hz    = 0x07,
    LSM6DSR_ODR_1666Hz   = 0x08,
    LSM6DSR_ODR_3332Hz   = 0x09,
    LSM6DSR_ODR_6664Hz   = 0x0A,
} lsm6dsr_odr_t;

// 陀螺仪量程
typedef enum {
    LSM6DSR_GYR_FS_125dps   = 0x01,
    LSM6DSR_GYR_FS_250dps   = 0x00,
    LSM6DSR_GYR_FS_500dps   = 0x02,
    LSM6DSR_GYR_FS_1000dps  = 0x04,
    LSM6DSR_GYR_FS_2000dps  = 0x06,
} lsm6dsr_gyr_fs_t;

// 加速度计量程
typedef enum {
    LSM6DSR_ACC_FS_2g   = 0x00,
    LSM6DSR_ACC_FS_4g   = 0x02,
    LSM6DSR_ACC_FS_8g   = 0x03,
    LSM6DSR_ACC_FS_16g  = 0x01,
} lsm6dsr_acc_fs_t;

/*============================================================================
 * 驱动状态 / Driver State
 *============================================================================*/

static struct {
    float gyro_scale;
    float acc_scale;
    uint8_t initialized;
} lsm6dsr_ctx;

/*============================================================================
 * SPI 通信 / SPI Communication
 *============================================================================*/

static uint8_t lsm6dsr_read_reg(uint8_t reg)
{
    uint8_t val;
    hal_spi_cs(0);
    hal_spi_xfer(reg | 0x80);
    val = hal_spi_xfer(0x00);
    hal_spi_cs(1);
    return val;
}

static void lsm6dsr_write_reg(uint8_t reg, uint8_t val)
{
    hal_spi_cs(0);
    hal_spi_xfer(reg & 0x7F);
    hal_spi_xfer(val);
    hal_spi_cs(1);
}

static void lsm6dsr_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    hal_spi_cs(0);
    hal_spi_xfer(reg | 0x80);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = hal_spi_xfer(0x00);
    }
    hal_spi_cs(1);
}

/*============================================================================
 * API 实现 / API Implementation
 *============================================================================*/

int lsm6dsr_init(void)
{
    // 检查 WHO_AM_I
    uint8_t id = lsm6dsr_read_reg(LSM6DSR_WHO_AM_I);
    if (id != LSM6DSR_WHO_AM_I_VALUE) {
        return -1;
    }
    
    // 软复位
    lsm6dsr_write_reg(LSM6DSR_CTRL3_C, 0x01);
    hal_delay_us(10000);
    
    // CTRL3_C: BDU=1, IF_INC=1
    lsm6dsr_write_reg(LSM6DSR_CTRL3_C, 0x44);
    
    // CTRL1_XL: ODR=208Hz, FS=4g
    lsm6dsr_write_reg(LSM6DSR_CTRL1_XL, (LSM6DSR_ODR_208Hz << 4) | LSM6DSR_ACC_FS_4g);
    lsm6dsr_ctx.acc_scale = 4.0f / 32768.0f;
    
    // CTRL2_G: ODR=208Hz, FS=2000dps
    lsm6dsr_write_reg(LSM6DSR_CTRL2_G, (LSM6DSR_ODR_208Hz << 4) | LSM6DSR_GYR_FS_2000dps);
    lsm6dsr_ctx.gyro_scale = 2000.0f / 32768.0f;
    
    // CTRL6_C: 陀螺仪高性能模式
    lsm6dsr_write_reg(LSM6DSR_CTRL6_C, 0x00);
    
    // CTRL7_G: 陀螺仪 HPF 禁用
    lsm6dsr_write_reg(LSM6DSR_CTRL7_G, 0x00);
    
    lsm6dsr_ctx.initialized = 1;
    return 0;
}

int lsm6dsr_read_accel(float *ax, float *ay, float *az)
{
    if (!lsm6dsr_ctx.initialized) return -1;
    
    uint8_t buf[6];
    lsm6dsr_read_regs(LSM6DSR_OUTX_L_A, buf, 6);
    
    int16_t raw_x = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t raw_y = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t raw_z = (int16_t)(buf[4] | (buf[5] << 8));
    
    *ax = raw_x * lsm6dsr_ctx.acc_scale;
    *ay = raw_y * lsm6dsr_ctx.acc_scale;
    *az = raw_z * lsm6dsr_ctx.acc_scale;
    
    return 0;
}

int lsm6dsr_read_gyro(float *gx, float *gy, float *gz)
{
    if (!lsm6dsr_ctx.initialized) return -1;
    
    uint8_t buf[6];
    lsm6dsr_read_regs(LSM6DSR_OUTX_L_G, buf, 6);
    
    int16_t raw_x = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t raw_y = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t raw_z = (int16_t)(buf[4] | (buf[5] << 8));
    
    float deg2rad = 0.01745329252f;
    *gx = raw_x * lsm6dsr_ctx.gyro_scale * deg2rad;
    *gy = raw_y * lsm6dsr_ctx.gyro_scale * deg2rad;
    *gz = raw_z * lsm6dsr_ctx.gyro_scale * deg2rad;
    
    return 0;
}

int lsm6dsr_read_all(float gyro[3], float accel[3])
{
    if (!lsm6dsr_ctx.initialized) return -1;
    
    uint8_t buf[12];
    lsm6dsr_read_regs(LSM6DSR_OUTX_L_G, buf, 12);
    
    int16_t gx = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t gy = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t gz = (int16_t)(buf[4] | (buf[5] << 8));
    
    float deg2rad = 0.01745329252f;
    gyro[0] = gx * lsm6dsr_ctx.gyro_scale * deg2rad;
    gyro[1] = gy * lsm6dsr_ctx.gyro_scale * deg2rad;
    gyro[2] = gz * lsm6dsr_ctx.gyro_scale * deg2rad;
    
    int16_t ax = (int16_t)(buf[6] | (buf[7] << 8));
    int16_t ay = (int16_t)(buf[8] | (buf[9] << 8));
    int16_t az = (int16_t)(buf[10] | (buf[11] << 8));
    
    accel[0] = ax * lsm6dsr_ctx.acc_scale;
    accel[1] = ay * lsm6dsr_ctx.acc_scale;
    accel[2] = az * lsm6dsr_ctx.acc_scale;
    
    return 0;
}

int lsm6dsr_read_raw(int16_t gyro[3], int16_t accel[3])
{
    if (!lsm6dsr_ctx.initialized) return -1;
    
    uint8_t buf[12];
    lsm6dsr_read_regs(LSM6DSR_OUTX_L_G, buf, 12);
    
    gyro[0] = (int16_t)(buf[0] | (buf[1] << 8));
    gyro[1] = (int16_t)(buf[2] | (buf[3] << 8));
    gyro[2] = (int16_t)(buf[4] | (buf[5] << 8));
    
    accel[0] = (int16_t)(buf[6] | (buf[7] << 8));
    accel[1] = (int16_t)(buf[8] | (buf[9] << 8));
    accel[2] = (int16_t)(buf[10] | (buf[11] << 8));
    
    return 0;
}

int lsm6dsr_read_temp(float *temp)
{
    if (!lsm6dsr_ctx.initialized) return -1;
    
    uint8_t buf[2];
    lsm6dsr_read_regs(LSM6DSR_OUT_TEMP_L, buf, 2);
    
    int16_t raw = (int16_t)(buf[0] | (buf[1] << 8));
    *temp = 25.0f + raw / 256.0f;
    
    return 0;
}

bool lsm6dsr_data_ready(void)
{
    uint8_t status = lsm6dsr_read_reg(LSM6DSR_STATUS_REG);
    return (status & 0x03) == 0x03;
}

int lsm6dsr_set_odr(uint16_t odr_hz)
{
    lsm6dsr_odr_t odr;
    
    if (odr_hz >= 6664) odr = LSM6DSR_ODR_6664Hz;
    else if (odr_hz >= 3332) odr = LSM6DSR_ODR_3332Hz;
    else if (odr_hz >= 1666) odr = LSM6DSR_ODR_1666Hz;
    else if (odr_hz >= 833) odr = LSM6DSR_ODR_833Hz;
    else if (odr_hz >= 416) odr = LSM6DSR_ODR_416Hz;
    else if (odr_hz >= 208) odr = LSM6DSR_ODR_208Hz;
    else if (odr_hz >= 104) odr = LSM6DSR_ODR_104Hz;
    else odr = LSM6DSR_ODR_52Hz;
    
    uint8_t ctrl1 = lsm6dsr_read_reg(LSM6DSR_CTRL1_XL);
    uint8_t ctrl2 = lsm6dsr_read_reg(LSM6DSR_CTRL2_G);
    
    lsm6dsr_write_reg(LSM6DSR_CTRL1_XL, (odr << 4) | (ctrl1 & 0x0F));
    lsm6dsr_write_reg(LSM6DSR_CTRL2_G, (odr << 4) | (ctrl2 & 0x0F));
    
    return 0;
}

void lsm6dsr_suspend(void)
{
    if (!lsm6dsr_ctx.initialized) return;
    lsm6dsr_write_reg(LSM6DSR_CTRL1_XL, 0x00);
    lsm6dsr_write_reg(LSM6DSR_CTRL2_G, 0x00);
}

void lsm6dsr_resume(void)
{
    if (!lsm6dsr_ctx.initialized) return;
    lsm6dsr_write_reg(LSM6DSR_CTRL1_XL, (LSM6DSR_ODR_208Hz << 4) | LSM6DSR_ACC_FS_4g);
    lsm6dsr_write_reg(LSM6DSR_CTRL2_G, (LSM6DSR_ODR_208Hz << 4) | LSM6DSR_GYR_FS_2000dps);
}
