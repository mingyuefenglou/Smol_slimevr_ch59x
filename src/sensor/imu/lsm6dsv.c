/**
 * @file lsm6dsv.c
 * @brief LSM6DSV IMU Driver for CH592
 * 
 * ST LSM6DSV 6-DoF IMU (Accelerometer + Gyroscope)
 * - 陀螺仪: ±125/±250/±500/±1000/±2000/±4000 dps
 * - 加速度计: ±2/±4/±8/±16 g
 * - ODR: 最高 7.68kHz (加速度计), 7.68kHz (陀螺仪)
 * - 内置 SFLP 传感器融合
 * 
 * 通信接口: SPI (最高 10MHz)
 */

#include "lsm6dsv.h"
#include "hal.h"
#include <string.h>

/*============================================================================
 * LSM6DSV 寄存器定义 / Register Definitions
 *============================================================================*/

// 设备 ID / Device ID
#define LSM6DSV_WHO_AM_I            0x0F
#define LSM6DSV_WHO_AM_I_VALUE      0x70

// 控制寄存器 / Control registers
#define LSM6DSV_CTRL1               0x10    // 加速度计 ODR & FS
#define LSM6DSV_CTRL2               0x11    // 陀螺仪 ODR & FS
#define LSM6DSV_CTRL3               0x12    // 控制 (IF_INC, BDU, etc.)
#define LSM6DSV_CTRL4               0x13    // INT1 配置
#define LSM6DSV_CTRL5               0x14    // INT2 配置
#define LSM6DSV_CTRL6               0x15    // 陀螺仪电源模式
#define LSM6DSV_CTRL7               0x16    // 加速度计电源模式
#define LSM6DSV_CTRL8               0x17    // 滤波器配置
#define LSM6DSV_CTRL9               0x18    // 设备配置
#define LSM6DSV_CTRL10              0x19    // 时间戳

// 状态寄存器 / Status registers
#define LSM6DSV_STATUS_REG          0x1E

// 输出寄存器 / Output registers
#define LSM6DSV_OUT_TEMP_L          0x20
#define LSM6DSV_OUT_TEMP_H          0x21
#define LSM6DSV_OUTX_L_G            0x22    // 陀螺仪 X 低字节
#define LSM6DSV_OUTX_H_G            0x23
#define LSM6DSV_OUTY_L_G            0x24
#define LSM6DSV_OUTY_H_G            0x25
#define LSM6DSV_OUTZ_L_G            0x26
#define LSM6DSV_OUTZ_H_G            0x27
#define LSM6DSV_OUTX_L_A            0x28    // 加速度计 X 低字节
#define LSM6DSV_OUTX_H_A            0x29
#define LSM6DSV_OUTY_L_A            0x2A
#define LSM6DSV_OUTY_H_A            0x2B
#define LSM6DSV_OUTZ_L_A            0x2C
#define LSM6DSV_OUTZ_H_A            0x2D

// SFLP (传感器融合) / Sensor Fusion Low Power
#define LSM6DSV_EMB_FUNC_EN_A       0x04    // (Bank A)
#define LSM6DSV_EMB_FUNC_EN_B       0x05    // (Bank A)
#define LSM6DSV_SFLP_ODR            0x5E    // (Bank A)

// FIFO 配置 / FIFO configuration
#define LSM6DSV_FIFO_CTRL1          0x07
#define LSM6DSV_FIFO_CTRL2          0x08
#define LSM6DSV_FIFO_CTRL3          0x09
#define LSM6DSV_FIFO_CTRL4          0x0A
#define LSM6DSV_FIFO_STATUS1        0x1B
#define LSM6DSV_FIFO_STATUS2        0x1C
#define LSM6DSV_FIFO_DATA_OUT_TAG   0x78
#define LSM6DSV_FIFO_DATA_OUT_X_L   0x79

// 功能配置 / Function configuration
#define LSM6DSV_FUNC_CFG_ACCESS     0x01

/*============================================================================
 * 配置常量 / Configuration Constants
 *============================================================================*/

// ODR (Output Data Rate) 设置 / ODR settings
typedef enum {
    LSM6DSV_ODR_OFF     = 0x00,
    LSM6DSV_ODR_1_875Hz = 0x01,
    LSM6DSV_ODR_7_5Hz   = 0x02,
    LSM6DSV_ODR_15Hz    = 0x03,
    LSM6DSV_ODR_30Hz    = 0x04,
    LSM6DSV_ODR_60Hz    = 0x05,
    LSM6DSV_ODR_120Hz   = 0x06,
    LSM6DSV_ODR_240Hz   = 0x07,
    LSM6DSV_ODR_480Hz   = 0x08,
    LSM6DSV_ODR_960Hz   = 0x09,
    LSM6DSV_ODR_1920Hz  = 0x0A,
    LSM6DSV_ODR_3840Hz  = 0x0B,
    LSM6DSV_ODR_7680Hz  = 0x0C,
} lsm6dsv_odr_t;

// 陀螺仪量程 / Gyroscope full scale
typedef enum {
    LSM6DSV_GYR_FS_125dps   = 0x00,
    LSM6DSV_GYR_FS_250dps   = 0x01,
    LSM6DSV_GYR_FS_500dps   = 0x02,
    LSM6DSV_GYR_FS_1000dps  = 0x03,
    LSM6DSV_GYR_FS_2000dps  = 0x04,
    LSM6DSV_GYR_FS_4000dps  = 0x0C,
} lsm6dsv_gyr_fs_t;

// 加速度计量程 / Accelerometer full scale
typedef enum {
    LSM6DSV_ACC_FS_2g   = 0x00,
    LSM6DSV_ACC_FS_4g   = 0x01,
    LSM6DSV_ACC_FS_8g   = 0x02,
    LSM6DSV_ACC_FS_16g  = 0x03,
} lsm6dsv_acc_fs_t;

/*============================================================================
 * 驱动状态 / Driver State
 *============================================================================*/

static struct {
    float gyro_scale;       // dps/LSB
    float acc_scale;        // g/LSB
    uint8_t initialized;
} lsm6dsv_ctx;

/*============================================================================
 * SPI 通信 / SPI Communication
 *============================================================================*/

static uint8_t lsm6dsv_read_reg(uint8_t reg)
{
    uint8_t val;
    hal_spi_cs(0);
    hal_spi_xfer(reg | 0x80);  // 读操作: bit7 = 1
    val = hal_spi_xfer(0x00);
    hal_spi_cs(1);
    return val;
}

static void lsm6dsv_write_reg(uint8_t reg, uint8_t val)
{
    hal_spi_cs(0);
    hal_spi_xfer(reg & 0x7F);  // 写操作: bit7 = 0
    hal_spi_xfer(val);
    hal_spi_cs(1);
}

static void lsm6dsv_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
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

int lsm6dsv_init(void)
{
    // 检查 WHO_AM_I / Check WHO_AM_I
    uint8_t id = lsm6dsv_read_reg(LSM6DSV_WHO_AM_I);
    if (id != LSM6DSV_WHO_AM_I_VALUE) {
        return -1;  // 设备未找到
    }
    
    // 软复位 / Software reset
    lsm6dsv_write_reg(LSM6DSV_CTRL3, 0x01);
    hal_delay_us(10000);  // 等待复位完成
    
    // 配置控制寄存器 / Configure control registers
    // CTRL3: BDU=1, IF_INC=1
    lsm6dsv_write_reg(LSM6DSV_CTRL3, 0x44);
    
    // CTRL1: 加速度计 ODR=240Hz, FS=4g
    lsm6dsv_write_reg(LSM6DSV_CTRL1, (LSM6DSV_ODR_240Hz << 4) | (LSM6DSV_ACC_FS_4g << 0));
    lsm6dsv_ctx.acc_scale = 4.0f / 32768.0f;  // g/LSB
    
    // CTRL2: 陀螺仪 ODR=240Hz, FS=2000dps
    lsm6dsv_write_reg(LSM6DSV_CTRL2, (LSM6DSV_ODR_240Hz << 4) | (LSM6DSV_GYR_FS_2000dps << 0));
    lsm6dsv_ctx.gyro_scale = 2000.0f / 32768.0f;  // dps/LSB
    
    // CTRL6: 陀螺仪高性能模式
    lsm6dsv_write_reg(LSM6DSV_CTRL6, 0x00);
    
    // CTRL7: 加速度计高性能模式
    lsm6dsv_write_reg(LSM6DSV_CTRL7, 0x00);
    
    lsm6dsv_ctx.initialized = 1;
    return 0;
}

int lsm6dsv_read_accel(float *ax, float *ay, float *az)
{
    if (!lsm6dsv_ctx.initialized) return -1;
    
    uint8_t buf[6];
    lsm6dsv_read_regs(LSM6DSV_OUTX_L_A, buf, 6);
    
    int16_t raw_x = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t raw_y = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t raw_z = (int16_t)(buf[4] | (buf[5] << 8));
    
    *ax = raw_x * lsm6dsv_ctx.acc_scale;
    *ay = raw_y * lsm6dsv_ctx.acc_scale;
    *az = raw_z * lsm6dsv_ctx.acc_scale;
    
    return 0;
}

int lsm6dsv_read_gyro(float *gx, float *gy, float *gz)
{
    if (!lsm6dsv_ctx.initialized) return -1;
    
    uint8_t buf[6];
    lsm6dsv_read_regs(LSM6DSV_OUTX_L_G, buf, 6);
    
    int16_t raw_x = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t raw_y = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t raw_z = (int16_t)(buf[4] | (buf[5] << 8));
    
    // 转换为 rad/s / Convert to rad/s
    float deg2rad = 0.01745329252f;
    *gx = raw_x * lsm6dsv_ctx.gyro_scale * deg2rad;
    *gy = raw_y * lsm6dsv_ctx.gyro_scale * deg2rad;
    *gz = raw_z * lsm6dsv_ctx.gyro_scale * deg2rad;
    
    return 0;
}

int lsm6dsv_read_all(float gyro[3], float accel[3])
{
    if (!lsm6dsv_ctx.initialized) return -1;
    
    uint8_t buf[12];
    lsm6dsv_read_regs(LSM6DSV_OUTX_L_G, buf, 12);
    
    // 陀螺仪 / Gyroscope
    int16_t gx = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t gy = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t gz = (int16_t)(buf[4] | (buf[5] << 8));
    
    float deg2rad = 0.01745329252f;
    gyro[0] = gx * lsm6dsv_ctx.gyro_scale * deg2rad;
    gyro[1] = gy * lsm6dsv_ctx.gyro_scale * deg2rad;
    gyro[2] = gz * lsm6dsv_ctx.gyro_scale * deg2rad;
    
    // 加速度计 / Accelerometer
    int16_t ax = (int16_t)(buf[6] | (buf[7] << 8));
    int16_t ay = (int16_t)(buf[8] | (buf[9] << 8));
    int16_t az = (int16_t)(buf[10] | (buf[11] << 8));
    
    accel[0] = ax * lsm6dsv_ctx.acc_scale;
    accel[1] = ay * lsm6dsv_ctx.acc_scale;
    accel[2] = az * lsm6dsv_ctx.acc_scale;
    
    return 0;
}

int lsm6dsv_read_raw(int16_t gyro[3], int16_t accel[3])
{
    if (!lsm6dsv_ctx.initialized) return -1;
    
    uint8_t buf[12];
    lsm6dsv_read_regs(LSM6DSV_OUTX_L_G, buf, 12);
    
    gyro[0] = (int16_t)(buf[0] | (buf[1] << 8));
    gyro[1] = (int16_t)(buf[2] | (buf[3] << 8));
    gyro[2] = (int16_t)(buf[4] | (buf[5] << 8));
    
    accel[0] = (int16_t)(buf[6] | (buf[7] << 8));
    accel[1] = (int16_t)(buf[8] | (buf[9] << 8));
    accel[2] = (int16_t)(buf[10] | (buf[11] << 8));
    
    return 0;
}

int lsm6dsv_read_temp(float *temp)
{
    if (!lsm6dsv_ctx.initialized) return -1;
    
    uint8_t buf[2];
    lsm6dsv_read_regs(LSM6DSV_OUT_TEMP_L, buf, 2);
    
    int16_t raw = (int16_t)(buf[0] | (buf[1] << 8));
    *temp = 25.0f + raw / 256.0f;
    
    return 0;
}

bool lsm6dsv_data_ready(void)
{
    uint8_t status = lsm6dsv_read_reg(LSM6DSV_STATUS_REG);
    return (status & 0x03) == 0x03;  // GDA + XLDA
}

int lsm6dsv_set_odr(uint16_t odr_hz)
{
    lsm6dsv_odr_t odr;
    
    if (odr_hz >= 7680) odr = LSM6DSV_ODR_7680Hz;
    else if (odr_hz >= 3840) odr = LSM6DSV_ODR_3840Hz;
    else if (odr_hz >= 1920) odr = LSM6DSV_ODR_1920Hz;
    else if (odr_hz >= 960) odr = LSM6DSV_ODR_960Hz;
    else if (odr_hz >= 480) odr = LSM6DSV_ODR_480Hz;
    else if (odr_hz >= 240) odr = LSM6DSV_ODR_240Hz;
    else if (odr_hz >= 120) odr = LSM6DSV_ODR_120Hz;
    else if (odr_hz >= 60) odr = LSM6DSV_ODR_60Hz;
    else odr = LSM6DSV_ODR_30Hz;
    
    // 更新 CTRL1 和 CTRL2
    uint8_t ctrl1 = lsm6dsv_read_reg(LSM6DSV_CTRL1);
    uint8_t ctrl2 = lsm6dsv_read_reg(LSM6DSV_CTRL2);
    
    lsm6dsv_write_reg(LSM6DSV_CTRL1, (odr << 4) | (ctrl1 & 0x0F));
    lsm6dsv_write_reg(LSM6DSV_CTRL2, (odr << 4) | (ctrl2 & 0x0F));
    
    return 0;
}

void lsm6dsv_suspend(void)
{
    if (!lsm6dsv_ctx.initialized) return;
    
    // 设置 ODR = 0 (Power-down)
    lsm6dsv_write_reg(LSM6DSV_CTRL1, 0x00);
    lsm6dsv_write_reg(LSM6DSV_CTRL2, 0x00);
}

void lsm6dsv_resume(void)
{
    if (!lsm6dsv_ctx.initialized) return;
    
    // 恢复 ODR=240Hz
    lsm6dsv_write_reg(LSM6DSV_CTRL1, (LSM6DSV_ODR_240Hz << 4) | LSM6DSV_ACC_FS_4g);
    lsm6dsv_write_reg(LSM6DSV_CTRL2, (LSM6DSV_ODR_240Hz << 4) | LSM6DSV_GYR_FS_2000dps);
}
