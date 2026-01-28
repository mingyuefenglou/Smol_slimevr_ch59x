/**
 * @file bmi323.c
 * @brief BMI323 IMU Driver
 * 
 * Bosch BMI323 6轴 IMU 驱动
 * - 加速度计: ±2/±4/±8/±16g
 * - 陀螺仪: ±125/±250/±500/±1000/±2000 dps
 * - 支持 SPI/I2C 接口
 * - 16位数据分辨率
 */

#include "bmi323.h"
#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * BMI323 寄存器定义
 *============================================================================*/

// 识别寄存器
#define BMI323_CHIP_ID              0x00
#define BMI323_CHIP_ID_VALUE        0x0043

// 错误寄存器
#define BMI323_ERR_REG              0x01

// 状态寄存器
#define BMI323_STATUS               0x02

// 数据寄存器
#define BMI323_ACC_DATA_X           0x03
#define BMI323_ACC_DATA_Y           0x04
#define BMI323_ACC_DATA_Z           0x05
#define BMI323_GYR_DATA_X           0x06
#define BMI323_GYR_DATA_Y           0x07
#define BMI323_GYR_DATA_Z           0x08
#define BMI323_TEMP_DATA            0x09
#define BMI323_SENSOR_TIME_0        0x0A
#define BMI323_SENSOR_TIME_1        0x0B

// INT 状态
#define BMI323_INT_STATUS_INT1      0x0D
#define BMI323_INT_STATUS_INT2      0x0E
#define BMI323_INT_STATUS_IBI       0x0F

// FIFO
#define BMI323_FIFO_FILL_LEVEL      0x15
#define BMI323_FIFO_DATA            0x16

// 配置寄存器
#define BMI323_ACC_CONF             0x20
#define BMI323_GYR_CONF             0x21
#define BMI323_ALT_ACC_CONF         0x28
#define BMI323_ALT_GYR_CONF         0x29
#define BMI323_ALT_CONF             0x2A
#define BMI323_ALT_STATUS           0x2B

// 中断配置
#define BMI323_INT_MAP1             0x3A
#define BMI323_INT_MAP2             0x3B
#define BMI323_INT_CONF             0x38
#define BMI323_IO_INT_CTRL          0x39

// 命令
#define BMI323_CMD                  0x7E

// 命令值
#define BMI323_CMD_SOFT_RESET       0xB6

// ODR 配置
#define BMI323_ODR_0_78Hz           0x01
#define BMI323_ODR_1_5Hz            0x02
#define BMI323_ODR_3_1Hz            0x03
#define BMI323_ODR_6_25Hz           0x04
#define BMI323_ODR_12_5Hz           0x05
#define BMI323_ODR_25Hz             0x06
#define BMI323_ODR_50Hz             0x07
#define BMI323_ODR_100Hz            0x08
#define BMI323_ODR_200Hz            0x09
#define BMI323_ODR_400Hz            0x0A
#define BMI323_ODR_800Hz            0x0B
#define BMI323_ODR_1600Hz           0x0C
#define BMI323_ODR_3200Hz           0x0D
#define BMI323_ODR_6400Hz           0x0E

// 加速度量程
#define BMI323_ACC_RANGE_2G         0x00
#define BMI323_ACC_RANGE_4G         0x01
#define BMI323_ACC_RANGE_8G         0x02
#define BMI323_ACC_RANGE_16G        0x03

// 陀螺仪量程
#define BMI323_GYR_RANGE_125DPS     0x00
#define BMI323_GYR_RANGE_250DPS     0x01
#define BMI323_GYR_RANGE_500DPS     0x02
#define BMI323_GYR_RANGE_1000DPS    0x03
#define BMI323_GYR_RANGE_2000DPS    0x04

// BWP (Bandwidth Parameter)
#define BMI323_BWP_ODR_4            0x00  // ODR/4
#define BMI323_BWP_ODR_2            0x01  // ODR/2

// 性能模式
#define BMI323_MODE_SUSPEND         0x00
#define BMI323_MODE_LOW_POWER       0x03
#define BMI323_MODE_NORMAL          0x04
#define BMI323_MODE_HIGH_PERF       0x07

/*============================================================================
 * 驱动状态
 *============================================================================*/

typedef struct {
    bool initialized;
    bool use_spi;
    uint8_t i2c_addr;  // 0x68 或 0x69
    
    uint8_t acc_odr;
    uint8_t acc_range;
    uint8_t gyr_odr;
    uint8_t gyr_range;
    
    float acc_sensitivity;  // mg/LSB
    float gyr_sensitivity;  // mdps/LSB
} bmi323_ctx_t;

static bmi323_ctx_t ctx;

/*============================================================================
 * 底层通信 (BMI323 使用 16-bit 寄存器访问)
 *============================================================================*/

static uint16_t read_reg16(uint8_t reg)
{
    uint16_t val = 0;
    
#ifdef CH59X
    if (ctx.use_spi) {
        GPIOA_ResetBits(GPIO_Pin_12);
        hal_spi_xfer(reg | 0x80);  // Read
        hal_spi_xfer(0x00);        // Dummy byte (BMI323 requirement)
        val = hal_spi_xfer(0x00);
        val |= hal_spi_xfer(0x00) << 8;
        GPIOA_SetBits(GPIO_Pin_12);
    } else {
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 0);
        I2C_SendByte(reg);
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 1);
        val = I2C_RecvByte(1);  // ACK
        val |= I2C_RecvByte(0) << 8;  // NACK
        I2C_Stop();
    }
#endif
    
    return val;
}

static void write_reg16(uint8_t reg, uint16_t val)
{
#ifdef CH59X
    if (ctx.use_spi) {
        GPIOA_ResetBits(GPIO_Pin_12);
        hal_spi_xfer(reg & 0x7F);  // Write
        hal_spi_xfer(val & 0xFF);
        hal_spi_xfer(val >> 8);
        GPIOA_SetBits(GPIO_Pin_12);
    } else {
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 0);
        I2C_SendByte(reg);
        I2C_SendByte(val & 0xFF);
        I2C_SendByte(val >> 8);
        I2C_Stop();
    }
#endif
}

static void read_burst(uint8_t reg, uint16_t *buf, uint8_t count)
{
#ifdef CH59X
    if (ctx.use_spi) {
        GPIOA_ResetBits(GPIO_Pin_12);
        hal_spi_xfer(reg | 0x80);
        hal_spi_xfer(0x00);  // Dummy
        for (uint8_t i = 0; i < count; i++) {
            buf[i] = hal_spi_xfer(0x00);
            buf[i] |= hal_spi_xfer(0x00) << 8;
        }
        GPIOA_SetBits(GPIO_Pin_12);
    } else {
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 0);
        I2C_SendByte(reg);
        I2C_Start();
        I2C_SendByte((ctx.i2c_addr << 1) | 1);
        for (uint8_t i = 0; i < count; i++) {
            buf[i] = I2C_RecvByte(1);  // ACK for low byte
            buf[i] |= I2C_RecvByte(i < count - 1 ? 1 : 0) << 8;  // ACK or NACK for high byte
        }
        I2C_Stop();
    }
#endif
}

/*============================================================================
 * 公共 API
 *============================================================================*/

int bmi323_init(bool use_spi, uint8_t i2c_addr)
{
    memset(&ctx, 0, sizeof(ctx));
    ctx.use_spi = use_spi;
    ctx.i2c_addr = i2c_addr;
    
    // SPI 初始化需要先发送 dummy 读取
    if (use_spi) {
        read_reg16(BMI323_CHIP_ID);
        hal_delay_ms(1);
    }
    
    // 读取 CHIP_ID
    uint16_t chip_id = read_reg16(BMI323_CHIP_ID);
    if (chip_id != BMI323_CHIP_ID_VALUE) {
        return -1;  // 设备未找到
    }
    
    // 软复位
    write_reg16(BMI323_CMD, BMI323_CMD_SOFT_RESET);
    hal_delay_ms(10);
    
    // 再次读取 CHIP_ID 确认
    if (use_spi) {
        read_reg16(BMI323_CHIP_ID);
        hal_delay_ms(1);
    }
    chip_id = read_reg16(BMI323_CHIP_ID);
    if (chip_id != BMI323_CHIP_ID_VALUE) {
        return -2;  // 复位后设备异常
    }
    
    // 默认配置: 200Hz, ±4g, ±2000dps, 高性能模式
    bmi323_set_accel_config(BMI323_ODR_200Hz, BMI323_ACC_RANGE_4G);
    bmi323_set_gyro_config(BMI323_ODR_200Hz, BMI323_GYR_RANGE_2000DPS);
    
    // 配置中断: DRDY 到 INT1
    write_reg16(BMI323_INT_MAP1, 0x0001);  // acc_drdy -> INT1
    write_reg16(BMI323_INT_MAP2, 0x0100);  // gyr_drdy -> INT1
    write_reg16(BMI323_IO_INT_CTRL, 0x0101);  // INT1: push-pull, active high
    
    ctx.initialized = true;
    return 0;
}

int bmi323_set_accel_config(uint8_t odr, uint8_t range)
{
    ctx.acc_odr = odr;
    ctx.acc_range = range;
    
    // 灵敏度 (mg/LSB)
    switch (range) {
        case BMI323_ACC_RANGE_2G:  ctx.acc_sensitivity = 0.061f; break;
        case BMI323_ACC_RANGE_4G:  ctx.acc_sensitivity = 0.122f; break;
        case BMI323_ACC_RANGE_8G:  ctx.acc_sensitivity = 0.244f; break;
        case BMI323_ACC_RANGE_16G: ctx.acc_sensitivity = 0.488f; break;
        default: ctx.acc_sensitivity = 0.122f; break;
    }
    
    // ACC_CONF: [15:12]=mode(高性能), [10:8]=avg, [7:4]=bwp, [3:0]=odr
    uint16_t conf = ((uint16_t)BMI323_MODE_HIGH_PERF << 12) |
                    ((uint16_t)range << 4) |
                    odr;
    write_reg16(BMI323_ACC_CONF, conf);
    
    return 0;
}

int bmi323_set_gyro_config(uint8_t odr, uint8_t range)
{
    ctx.gyr_odr = odr;
    ctx.gyr_range = range;
    
    // 灵敏度 (mdps/LSB)
    switch (range) {
        case BMI323_GYR_RANGE_125DPS:  ctx.gyr_sensitivity = 3.8f;  break;
        case BMI323_GYR_RANGE_250DPS:  ctx.gyr_sensitivity = 7.6f;  break;
        case BMI323_GYR_RANGE_500DPS:  ctx.gyr_sensitivity = 15.3f; break;
        case BMI323_GYR_RANGE_1000DPS: ctx.gyr_sensitivity = 30.5f; break;
        case BMI323_GYR_RANGE_2000DPS: ctx.gyr_sensitivity = 61.0f; break;
        default: ctx.gyr_sensitivity = 61.0f; break;
    }
    
    // GYR_CONF: [15:12]=mode, [10:8]=avg, [7:4]=bwp, [3:0]=odr
    uint16_t conf = ((uint16_t)BMI323_MODE_HIGH_PERF << 12) |
                    ((uint16_t)range << 4) |
                    odr;
    write_reg16(BMI323_GYR_CONF, conf);
    
    return 0;
}

bool bmi323_data_ready(void)
{
    uint16_t status = read_reg16(BMI323_STATUS);
    return (status & 0x60) == 0x60;  // Bit6: gyr_drdy, Bit5: acc_drdy
}

int bmi323_read_raw(int16_t gyro[3], int16_t accel[3])
{
    if (!ctx.initialized) return -1;
    
    uint16_t buf[6];
    
    // 读取加速度和陀螺仪数据
    read_burst(BMI323_ACC_DATA_X, buf, 6);
    
    accel[0] = (int16_t)buf[0];
    accel[1] = (int16_t)buf[1];
    accel[2] = (int16_t)buf[2];
    
    gyro[0] = (int16_t)buf[3];
    gyro[1] = (int16_t)buf[4];
    gyro[2] = (int16_t)buf[5];
    
    return 0;
}

int bmi323_read_all(float gyro[3], float accel[3])
{
    int16_t gyro_raw[3], accel_raw[3];
    
    if (bmi323_read_raw(gyro_raw, accel_raw) != 0) {
        return -1;
    }
    
    // 转换为物理单位
    float mdps_to_rads = 0.001f * 0.01745329252f;
    gyro[0] = gyro_raw[0] * ctx.gyr_sensitivity * mdps_to_rads;
    gyro[1] = gyro_raw[1] * ctx.gyr_sensitivity * mdps_to_rads;
    gyro[2] = gyro_raw[2] * ctx.gyr_sensitivity * mdps_to_rads;
    
    float mg_to_g = 0.001f;
    accel[0] = accel_raw[0] * ctx.acc_sensitivity * mg_to_g;
    accel[1] = accel_raw[1] * ctx.acc_sensitivity * mg_to_g;
    accel[2] = accel_raw[2] * ctx.acc_sensitivity * mg_to_g;
    
    return 0;
}

int16_t bmi323_read_temp(void)
{
    uint16_t raw = read_reg16(BMI323_TEMP_DATA);
    // 温度公式: T = raw / 512 + 23
    return ((int16_t)raw / 512) + 23;
}

void bmi323_power_down(void)
{
    // 设置为 suspend 模式
    uint16_t acc_conf = read_reg16(BMI323_ACC_CONF);
    acc_conf = (acc_conf & 0x0FFF) | ((uint16_t)BMI323_MODE_SUSPEND << 12);
    write_reg16(BMI323_ACC_CONF, acc_conf);
    
    uint16_t gyr_conf = read_reg16(BMI323_GYR_CONF);
    gyr_conf = (gyr_conf & 0x0FFF) | ((uint16_t)BMI323_MODE_SUSPEND << 12);
    write_reg16(BMI323_GYR_CONF, gyr_conf);
}

void bmi323_power_up(void)
{
    bmi323_set_accel_config(ctx.acc_odr, ctx.acc_range);
    bmi323_set_gyro_config(ctx.gyr_odr, ctx.gyr_range);
}
