/**
 * @file lsm6dso.c
 * @brief LSM6DSO IMU Driver
 * 
 * STMicroelectronics LSM6DSO 6轴 IMU 驱动
 * - 加速度计: ±2/±4/±8/±16g
 * - 陀螺仪: ±125/±250/±500/±1000/±2000 dps
 * - 支持 SPI/I2C 接口
 * - 数据就绪中断
 */

#include "lsm6dso.h"
#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * LSM6DSO 寄存器定义
 *============================================================================*/

// 设备识别
#define LSM6DSO_WHO_AM_I            0x0F
#define LSM6DSO_WHO_AM_I_VALUE      0x6C

// 控制寄存器
#define LSM6DSO_CTRL1_XL            0x10    // 加速度计控制
#define LSM6DSO_CTRL2_G             0x11    // 陀螺仪控制
#define LSM6DSO_CTRL3_C             0x12    // 控制3 (BDU, IF_INC, SW_RESET)
#define LSM6DSO_CTRL4_C             0x13    // 控制4
#define LSM6DSO_CTRL5_C             0x14    // 控制5
#define LSM6DSO_CTRL6_C             0x15    // 控制6 (XL性能模式)
#define LSM6DSO_CTRL7_G             0x16    // 控制7 (G性能模式)
#define LSM6DSO_CTRL8_XL            0x17    // 控制8 (XL滤波)
#define LSM6DSO_CTRL9_XL            0x18    // 控制9
#define LSM6DSO_CTRL10_C            0x19    // 控制10

// 中断配置
#define LSM6DSO_INT1_CTRL           0x0D
#define LSM6DSO_INT2_CTRL           0x0E

// 状态
#define LSM6DSO_STATUS_REG          0x1E

// 数据输出
#define LSM6DSO_OUT_TEMP_L          0x20
#define LSM6DSO_OUT_TEMP_H          0x21
#define LSM6DSO_OUTX_L_G            0x22
#define LSM6DSO_OUTX_H_G            0x23
#define LSM6DSO_OUTY_L_G            0x24
#define LSM6DSO_OUTY_H_G            0x25
#define LSM6DSO_OUTZ_L_G            0x26
#define LSM6DSO_OUTZ_H_G            0x27
#define LSM6DSO_OUTX_L_A            0x28
#define LSM6DSO_OUTX_H_A            0x29
#define LSM6DSO_OUTY_L_A            0x2A
#define LSM6DSO_OUTY_H_A            0x2B
#define LSM6DSO_OUTZ_L_A            0x2C
#define LSM6DSO_OUTZ_H_A            0x2D

// FIFO
#define LSM6DSO_FIFO_CTRL1          0x07
#define LSM6DSO_FIFO_CTRL2          0x08
#define LSM6DSO_FIFO_CTRL3          0x09
#define LSM6DSO_FIFO_CTRL4          0x0A
#define LSM6DSO_FIFO_STATUS1        0x3A
#define LSM6DSO_FIFO_STATUS2        0x3B
#define LSM6DSO_FIFO_DATA_OUT_TAG   0x78
#define LSM6DSO_FIFO_DATA_OUT_X_L   0x79

// ODR (Output Data Rate) 配置值
#define LSM6DSO_ODR_OFF             0x00
#define LSM6DSO_ODR_12_5Hz          0x01
#define LSM6DSO_ODR_26Hz            0x02
#define LSM6DSO_ODR_52Hz            0x03
#define LSM6DSO_ODR_104Hz           0x04
#define LSM6DSO_ODR_208Hz           0x05
#define LSM6DSO_ODR_416Hz           0x06
#define LSM6DSO_ODR_833Hz           0x07
#define LSM6DSO_ODR_1666Hz          0x08
#define LSM6DSO_ODR_3332Hz          0x09
#define LSM6DSO_ODR_6664Hz          0x0A

// 加速度量程
#define LSM6DSO_XL_FS_2g            0x00
#define LSM6DSO_XL_FS_4g            0x02
#define LSM6DSO_XL_FS_8g            0x03
#define LSM6DSO_XL_FS_16g           0x01

// 陀螺仪量程
#define LSM6DSO_GY_FS_125dps        0x01
#define LSM6DSO_GY_FS_250dps        0x00
#define LSM6DSO_GY_FS_500dps        0x02
#define LSM6DSO_GY_FS_1000dps       0x04
#define LSM6DSO_GY_FS_2000dps       0x06

/*============================================================================
 * 驱动状态
 *============================================================================*/

typedef struct {
    bool initialized;
    bool use_spi;           // true: SPI, false: I2C
    uint8_t i2c_addr;       // I2C 地址 (0x6A 或 0x6B)
    
    // 当前配置
    uint8_t accel_odr;
    uint8_t accel_fs;
    uint8_t gyro_odr;
    uint8_t gyro_fs;
    
    // 量程对应的灵敏度
    float accel_sensitivity;  // mg/LSB
    float gyro_sensitivity;   // mdps/LSB
} lsm6dso_ctx_t;

static lsm6dso_ctx_t ctx;

/*============================================================================
 * 底层通信
 *============================================================================*/

static uint8_t read_reg(uint8_t reg)
{
    uint8_t val = 0;
    
#ifdef CH59X
    if (ctx.use_spi) {
        GPIOA_ResetBits(GPIO_Pin_12);  // CS low
        hal_spi_xfer(reg | 0x80);      // Read bit
        val = hal_spi_xfer(0x00);
        GPIOA_SetBits(GPIO_Pin_12);    // CS high
    } else {
        // I2C
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 0);
        I2C_SendByte(reg);
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 1);
        val = I2C_RecvByte(0);  // NACK for single byte read
        I2C_Stop();
    }
#endif
    
    return val;
}

static void write_reg(uint8_t reg, uint8_t val)
{
#ifdef CH59X
    if (ctx.use_spi) {
        GPIOA_ResetBits(GPIO_Pin_12);
        hal_spi_xfer(reg & 0x7F);      // Write bit
        hal_spi_xfer(val);
        GPIOA_SetBits(GPIO_Pin_12);
    } else {
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 0);
        I2C_SendByte(reg);
        I2C_SendByte(val);
        I2C_Stop();
    }
#endif
}

static void read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
#ifdef CH59X
    if (ctx.use_spi) {
        GPIOA_ResetBits(GPIO_Pin_12);
        hal_spi_xfer(reg | 0x80);
        for (uint8_t i = 0; i < len; i++) {
            buf[i] = hal_spi_xfer(0x00);
        }
        GPIOA_SetBits(GPIO_Pin_12);
    } else {
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 0);
        I2C_SendByte(reg);
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 1);
        for (uint8_t i = 0; i < len; i++) {
            buf[i] = I2C_RecvByte(i < len - 1 ? 1 : 0);  // ACK for all except last byte
        }
        I2C_Stop();
    }
#endif
}

/*============================================================================
 * 公共 API
 *============================================================================*/

int lsm6dso_init(bool use_spi, uint8_t i2c_addr)
{
    memset(&ctx, 0, sizeof(ctx));
    ctx.use_spi = use_spi;
    ctx.i2c_addr = i2c_addr;
    
    // 检查 WHO_AM_I
    uint8_t who = read_reg(LSM6DSO_WHO_AM_I);
    if (who != LSM6DSO_WHO_AM_I_VALUE) {
        return -1;  // 设备未找到
    }
    
    // 软复位
    write_reg(LSM6DSO_CTRL3_C, 0x01);
    hal_delay_ms(10);
    
    // 等待复位完成
    while (read_reg(LSM6DSO_CTRL3_C) & 0x01) {
        hal_delay_ms(1);
    }
    
    // 配置 CTRL3_C: BDU=1, IF_INC=1
    write_reg(LSM6DSO_CTRL3_C, 0x44);
    
    // 默认配置: 208Hz, ±4g, ±2000dps
    lsm6dso_set_accel_config(LSM6DSO_ODR_208Hz, LSM6DSO_XL_FS_4g);
    lsm6dso_set_gyro_config(LSM6DSO_ODR_208Hz, LSM6DSO_GY_FS_2000dps);
    
    // 配置中断: 数据就绪到 INT1
    write_reg(LSM6DSO_INT1_CTRL, 0x03);  // DRDY_G, DRDY_XL
    
    ctx.initialized = true;
    return 0;
}

int lsm6dso_set_accel_config(uint8_t odr, uint8_t fs)
{
    ctx.accel_odr = odr;
    ctx.accel_fs = fs;
    
    // 设置灵敏度 (mg/LSB)
    switch (fs) {
        case LSM6DSO_XL_FS_2g:  ctx.accel_sensitivity = 0.061f; break;
        case LSM6DSO_XL_FS_4g:  ctx.accel_sensitivity = 0.122f; break;
        case LSM6DSO_XL_FS_8g:  ctx.accel_sensitivity = 0.244f; break;
        case LSM6DSO_XL_FS_16g: ctx.accel_sensitivity = 0.488f; break;
        default: ctx.accel_sensitivity = 0.122f; break;
    }
    
    // CTRL1_XL: ODR[7:4], FS[3:2]
    uint8_t val = (odr << 4) | (fs << 2);
    write_reg(LSM6DSO_CTRL1_XL, val);
    
    return 0;
}

int lsm6dso_set_gyro_config(uint8_t odr, uint8_t fs)
{
    ctx.gyro_odr = odr;
    ctx.gyro_fs = fs;
    
    // 设置灵敏度 (mdps/LSB)
    switch (fs) {
        case LSM6DSO_GY_FS_125dps:  ctx.gyro_sensitivity = 4.375f;  break;
        case LSM6DSO_GY_FS_250dps:  ctx.gyro_sensitivity = 8.75f;   break;
        case LSM6DSO_GY_FS_500dps:  ctx.gyro_sensitivity = 17.5f;   break;
        case LSM6DSO_GY_FS_1000dps: ctx.gyro_sensitivity = 35.0f;   break;
        case LSM6DSO_GY_FS_2000dps: ctx.gyro_sensitivity = 70.0f;   break;
        default: ctx.gyro_sensitivity = 70.0f; break;
    }
    
    // CTRL2_G: ODR[7:4], FS[3:1]
    uint8_t val = (odr << 4) | (fs << 1);
    write_reg(LSM6DSO_CTRL2_G, val);
    
    return 0;
}

bool lsm6dso_data_ready(void)
{
    uint8_t status = read_reg(LSM6DSO_STATUS_REG);
    return (status & 0x03) == 0x03;  // XLDA && GDA
}

int lsm6dso_read_raw(int16_t gyro[3], int16_t accel[3])
{
    if (!ctx.initialized) return -1;
    
    uint8_t buf[12];
    
    // 读取陀螺仪和加速度计数据 (连续地址)
    read_regs(LSM6DSO_OUTX_L_G, buf, 12);
    
    // 解析陀螺仪 (先)
    gyro[0] = (int16_t)(buf[0] | (buf[1] << 8));
    gyro[1] = (int16_t)(buf[2] | (buf[3] << 8));
    gyro[2] = (int16_t)(buf[4] | (buf[5] << 8));
    
    // 解析加速度计 (后)
    accel[0] = (int16_t)(buf[6] | (buf[7] << 8));
    accel[1] = (int16_t)(buf[8] | (buf[9] << 8));
    accel[2] = (int16_t)(buf[10] | (buf[11] << 8));
    
    return 0;
}

int lsm6dso_read_all(float gyro[3], float accel[3])
{
    int16_t gyro_raw[3], accel_raw[3];
    
    if (lsm6dso_read_raw(gyro_raw, accel_raw) != 0) {
        return -1;
    }
    
    // 转换为物理单位
    // 陀螺仪: rad/s
    float mdps_to_rads = 0.001f * 0.01745329252f;  // mdps -> dps -> rad/s
    gyro[0] = gyro_raw[0] * ctx.gyro_sensitivity * mdps_to_rads;
    gyro[1] = gyro_raw[1] * ctx.gyro_sensitivity * mdps_to_rads;
    gyro[2] = gyro_raw[2] * ctx.gyro_sensitivity * mdps_to_rads;
    
    // 加速度计: g
    float mg_to_g = 0.001f;
    accel[0] = accel_raw[0] * ctx.accel_sensitivity * mg_to_g;
    accel[1] = accel_raw[1] * ctx.accel_sensitivity * mg_to_g;
    accel[2] = accel_raw[2] * ctx.accel_sensitivity * mg_to_g;
    
    return 0;
}

int16_t lsm6dso_read_temp(void)
{
    uint8_t buf[2];
    read_regs(LSM6DSO_OUT_TEMP_L, buf, 2);
    int16_t raw = (int16_t)(buf[0] | (buf[1] << 8));
    // 温度公式: T = raw / 256 + 25
    return (raw / 256) + 25;
}

void lsm6dso_power_down(void)
{
    write_reg(LSM6DSO_CTRL1_XL, 0x00);  // Accel off
    write_reg(LSM6DSO_CTRL2_G, 0x00);   // Gyro off
}

void lsm6dso_power_up(void)
{
    // 恢复之前的配置
    lsm6dso_set_accel_config(ctx.accel_odr, ctx.accel_fs);
    lsm6dso_set_gyro_config(ctx.gyro_odr, ctx.gyro_fs);
}
