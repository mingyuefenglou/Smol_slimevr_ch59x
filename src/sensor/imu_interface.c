/**
 * @file imu_interface.c
 * @brief IMU 统一接口层 (支持 SPI 和 I2C)
 * 
 * IMU Unified Interface Layer (SPI and I2C Support)
 * 
 * 优先级: SPI > I2C
 * 初始化时自动检测接口类型
 * 
 * 支持的 IMU:
 * - ICM-45686 (默认, 推荐)
 * - ICM-42688
 * - BMI270
 * - LSM6DSV
 * - LSM6DSR
 */

#include "imu_interface.h"
#include "hal.h"
#include "config.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * 配置 / Configuration
 *============================================================================*/

// 接口类型
typedef enum {
    IMU_IF_NONE = 0,
    IMU_IF_SPI,
    IMU_IF_I2C
} imu_interface_type_t;

// IMU 类型 (来自 config.h)
#ifndef IMU_TYPE
#define IMU_TYPE    IMU_ICM45686
#endif

/*============================================================================
 * WHO_AM_I 寄存器定义 / WHO_AM_I Register Definitions
 *============================================================================*/

#define ICM45686_WHO_AM_I_REG   0x75
#define ICM45686_WHO_AM_I_VAL   0xE9

#define ICM42688_WHO_AM_I_REG   0x75
#define ICM42688_WHO_AM_I_VAL   0x47

#define BMI270_WHO_AM_I_REG     0x00
#define BMI270_WHO_AM_I_VAL     0x24

#define LSM6DSV_WHO_AM_I_REG    0x0F
#define LSM6DSV_WHO_AM_I_VAL    0x70

#define LSM6DSR_WHO_AM_I_REG    0x0F
#define LSM6DSR_WHO_AM_I_VAL    0x6B

#define IIM42652_WHO_AM_I_REG   0x75
#define IIM42652_WHO_AM_I_VAL   0x6F

#define SC7I22_WHO_AM_I_REG     0x00
#define SC7I22_WHO_AM_I_VAL     0x22

/*============================================================================
 * I2C 地址定义 / I2C Address Definitions
 *============================================================================*/

#define ICM45686_I2C_ADDR_0     0x68
#define ICM45686_I2C_ADDR_1     0x69
#define BMI270_I2C_ADDR_0       0x68
#define BMI270_I2C_ADDR_1       0x69
#define LSM6DSV_I2C_ADDR_0      0x6A
#define LSM6DSV_I2C_ADDR_1      0x6B

/*============================================================================
 * 状态 / State
 *============================================================================*/

static struct {
    imu_interface_type_t interface;
    uint8_t imu_type;
    uint8_t i2c_addr;
    bool initialized;
    
    // 量程配置
    float gyro_scale;   // dps/LSB
    float accel_scale;  // g/LSB
    
    // 校准数据
    float gyro_bias[3];
    float accel_bias[3];
} imu_ctx;

/*============================================================================
 * 底层通信 / Low-level Communication
 *============================================================================*/

// SPI 读写
static uint8_t spi_read_reg(uint8_t reg)
{
    uint8_t val;
#ifdef CH59X
    GPIOA_ResetBits(GPIO_Pin_4);  // CS low
    hal_spi_xfer(reg | 0x80);     // Read: bit7 = 1
    val = hal_spi_xfer(0x00);
    GPIOA_SetBits(GPIO_Pin_4);    // CS high
#endif
    return val;
}

static void spi_write_reg(uint8_t reg, uint8_t val)
{
#ifdef CH59X
    GPIOA_ResetBits(GPIO_Pin_4);  // CS low
    hal_spi_xfer(reg & 0x7F);     // Write: bit7 = 0
    hal_spi_xfer(val);
    GPIOA_SetBits(GPIO_Pin_4);    // CS high
#endif
}

static void spi_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
#ifdef CH59X
    GPIOA_ResetBits(GPIO_Pin_4);
    hal_spi_xfer(reg | 0x80);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = hal_spi_xfer(0x00);
    }
    GPIOA_SetBits(GPIO_Pin_4);
#endif
}

// I2C 读写
static uint8_t i2c_read_reg(uint8_t reg)
{
    uint8_t val = 0;
#ifdef CH59X
    I2C_Start();
    I2C_SendByte((imu_ctx.i2c_addr << 1) | 0);  // Write
    I2C_SendByte(reg);
    I2C_Start();  // Repeated start
    I2C_SendByte((imu_ctx.i2c_addr << 1) | 1);  // Read
    val = I2C_RecvByte(0);  // NACK for single byte
    I2C_Stop();
#endif
    return val;
}

static void i2c_write_reg(uint8_t reg, uint8_t val)
{
#ifdef CH59X
    I2C_Start();
    I2C_SendByte((imu_ctx.i2c_addr << 1) | 0);
    I2C_SendByte(reg);
    I2C_SendByte(val);
    I2C_Stop();
#endif
}

static void i2c_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
#ifdef CH59X
    I2C_Start();
    I2C_SendByte((imu_ctx.i2c_addr << 1) | 0);
    I2C_SendByte(reg);
    I2C_Start();
    I2C_SendByte((imu_ctx.i2c_addr << 1) | 1);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = I2C_RecvByte(i < len - 1 ? 1 : 0);  // ACK for all except last
    }
    I2C_Stop();
#endif
}

// 统一接口
static uint8_t imu_read_reg(uint8_t reg)
{
    if (imu_ctx.interface == IMU_IF_SPI) {
        return spi_read_reg(reg);
    } else {
        return i2c_read_reg(reg);
    }
}

static void imu_write_reg(uint8_t reg, uint8_t val)
{
    if (imu_ctx.interface == IMU_IF_SPI) {
        spi_write_reg(reg, val);
    } else {
        i2c_write_reg(reg, val);
    }
}

static void imu_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (imu_ctx.interface == IMU_IF_SPI) {
        spi_read_regs(reg, buf, len);
    } else {
        i2c_read_regs(reg, buf, len);
    }
}

/*============================================================================
 * IMU 检测 / IMU Detection
 *============================================================================*/

static bool detect_imu_spi(void)
{
    // 初始化 SPI
#ifdef CH59X
    // SPI CS 引脚
    GPIOA_SetBits(GPIO_Pin_4);
    GPIOA_ModeCfg(GPIO_Pin_4, GPIO_ModeOut_PP_5mA);
    
    // SPI 初始化 (Mode 3, 8MHz)
    SPI0_MasterDefInit();
    SPI0_CLKCfg(8);  // 60MHz / 8 = 7.5MHz
#endif
    
    imu_ctx.interface = IMU_IF_SPI;
    
    // 检测各种 IMU
    uint8_t who;
    
    // ICM-45686
    who = spi_read_reg(ICM45686_WHO_AM_I_REG);
    if (who == ICM45686_WHO_AM_I_VAL) {
        imu_ctx.imu_type = IMU_ICM45686;
        return true;
    }
    
    // ICM-42688
    who = spi_read_reg(ICM42688_WHO_AM_I_REG);
    if (who == ICM42688_WHO_AM_I_VAL) {
        imu_ctx.imu_type = IMU_ICM42688;
        return true;
    }
    
    // BMI270
    who = spi_read_reg(BMI270_WHO_AM_I_REG);
    if (who == BMI270_WHO_AM_I_VAL) {
        imu_ctx.imu_type = IMU_BMI270;
        return true;
    }
    
    // LSM6DSV
    who = spi_read_reg(LSM6DSV_WHO_AM_I_REG);
    if (who == LSM6DSV_WHO_AM_I_VAL) {
        imu_ctx.imu_type = IMU_LSM6DSV;
        return true;
    }
    
    // LSM6DSR
    who = spi_read_reg(LSM6DSR_WHO_AM_I_REG);
    if (who == LSM6DSR_WHO_AM_I_VAL) {
        imu_ctx.imu_type = IMU_LSM6DSR;
        return true;
    }
    
    // IIM-42652
    who = spi_read_reg(IIM42652_WHO_AM_I_REG);
    if (who == IIM42652_WHO_AM_I_VAL) {
        imu_ctx.imu_type = IMU_IIM42652;
        return true;
    }
    
    // SC7I22
    who = spi_read_reg(SC7I22_WHO_AM_I_REG);
    if (who == SC7I22_WHO_AM_I_VAL) {
        imu_ctx.imu_type = IMU_SC7I22;
        return true;
    }
    
    return false;
}

static bool detect_imu_i2c(void)
{
    // 初始化 I2C
#ifdef CH59X
    // CH59x I2C初始化 - 使用hal_i2c_init
    hal_i2c_config_t i2c_cfg = {
        .speed_hz = 400000,  // 400kHz
    };
    hal_i2c_init(&i2c_cfg);
#endif
    
    imu_ctx.interface = IMU_IF_I2C;
    
    // 尝试不同地址
    static const uint8_t addrs[] = {0x68, 0x69, 0x6A, 0x6B};
    
    for (int i = 0; i < 4; i++) {
        imu_ctx.i2c_addr = addrs[i];
        
        uint8_t who;
        
        // ICM-45686 / ICM-42688 / IIM-42652 / SC7I22
        if (addrs[i] == 0x68 || addrs[i] == 0x69) {
            who = i2c_read_reg(ICM45686_WHO_AM_I_REG);
            if (who == ICM45686_WHO_AM_I_VAL) {
                imu_ctx.imu_type = IMU_ICM45686;
                return true;
            }
            if (who == ICM42688_WHO_AM_I_VAL) {
                imu_ctx.imu_type = IMU_ICM42688;
                return true;
            }
            if (who == IIM42652_WHO_AM_I_VAL) {
                imu_ctx.imu_type = IMU_IIM42652;
                return true;
            }
            
            // SC7I22 使用不同的 WHO_AM_I 寄存器
            who = i2c_read_reg(SC7I22_WHO_AM_I_REG);
            if (who == SC7I22_WHO_AM_I_VAL) {
                imu_ctx.imu_type = IMU_SC7I22;
                return true;
            }
            
            who = i2c_read_reg(BMI270_WHO_AM_I_REG);
            if (who == BMI270_WHO_AM_I_VAL) {
                imu_ctx.imu_type = IMU_BMI270;
                return true;
            }
        }
        
        // LSM6DSV / LSM6DSR
        if (addrs[i] == 0x6A || addrs[i] == 0x6B) {
            who = i2c_read_reg(LSM6DSV_WHO_AM_I_REG);
            if (who == LSM6DSV_WHO_AM_I_VAL) {
                imu_ctx.imu_type = IMU_LSM6DSV;
                return true;
            }
            if (who == LSM6DSR_WHO_AM_I_VAL) {
                imu_ctx.imu_type = IMU_LSM6DSR;
                return true;
            }
        }
    }
    
    return false;
}

/*============================================================================
 * IMU 初始化 / IMU Initialization
 *============================================================================*/

static int init_icm45686(void)
{
    // 软复位
    imu_write_reg(0x11, 0x01);  // DEVICE_CONFIG
    hal_delay_ms(10);
    
    // 配置: Accel ODR=200Hz, FS=4g
    imu_write_reg(0x21, 0x06);  // ACCEL_CONFIG0
    
    // 配置: Gyro ODR=200Hz, FS=2000dps
    imu_write_reg(0x20, 0x06);  // GYRO_CONFIG0
    
    // 启用传感器
    imu_write_reg(0x4E, 0x0F);  // PWR_MGMT0
    
    // 配置 INT1 为数据就绪
    imu_write_reg(0x06, 0x02);  // INT_CONFIG0
    imu_write_reg(0x07, 0x01);  // INT_CONFIG1
    
    imu_ctx.gyro_scale = 2000.0f / 32768.0f;
    imu_ctx.accel_scale = 4.0f / 32768.0f;
    
    return 0;
}

static int init_bmi270(void)
{
    // 初始化需要加载配置文件，这里简化
    imu_write_reg(0x7C, 0x00);  // PWR_CONF
    hal_delay_ms(1);
    imu_write_reg(0x59, 0x00);  // INIT_CTRL
    hal_delay_ms(1);
    
    // 配置 Accel: ODR=200Hz, FS=4g
    imu_write_reg(0x40, 0xA8);  // ACC_CONF
    imu_write_reg(0x41, 0x01);  // ACC_RANGE
    
    // 配置 Gyro: ODR=200Hz, FS=2000dps
    imu_write_reg(0x42, 0xA9);  // GYR_CONF
    imu_write_reg(0x43, 0x00);  // GYR_RANGE
    
    // 启用传感器
    imu_write_reg(0x7D, 0x0E);  // PWR_CTRL
    
    imu_ctx.gyro_scale = 2000.0f / 32768.0f;
    imu_ctx.accel_scale = 4.0f / 32768.0f;
    
    return 0;
}

static int init_lsm6dsv(void)
{
    // 软复位
    imu_write_reg(0x12, 0x01);  // CTRL3_C
    hal_delay_ms(10);
    
    // CTRL3_C: BDU=1, IF_INC=1
    imu_write_reg(0x12, 0x44);
    
    // CTRL1: Accel ODR=240Hz, FS=4g
    imu_write_reg(0x10, 0x71);
    
    // CTRL2: Gyro ODR=240Hz, FS=2000dps
    imu_write_reg(0x11, 0x74);
    
    imu_ctx.gyro_scale = 2000.0f / 32768.0f;
    imu_ctx.accel_scale = 4.0f / 32768.0f;
    
    return 0;
}

/*============================================================================
 * 公共 API / Public API
 *============================================================================*/

int imu_init(void)
{
    memset(&imu_ctx, 0, sizeof(imu_ctx));
    
    // 优先尝试 SPI
    if (detect_imu_spi()) {
        // SPI 检测成功
    }
    // 备选 I2C
    else if (detect_imu_i2c()) {
        // I2C 检测成功
    }
    else {
        return -1;  // 未检测到 IMU
    }
    
    // 根据检测到的 IMU 类型初始化
    int ret = -1;
    switch (imu_ctx.imu_type) {
        case IMU_ICM45686:
        case IMU_ICM42688:
            ret = init_icm45686();
            break;
        case IMU_BMI270:
            ret = init_bmi270();
            break;
        case IMU_LSM6DSV:
        case IMU_LSM6DSR:
            ret = init_lsm6dsv();
            break;
    }
    
    if (ret == 0) {
        imu_ctx.initialized = true;
    }
    
    return ret;
}

int imu_read_all(float gyro[3], float accel[3])
{
    if (!imu_ctx.initialized) return -1;
    
    uint8_t buf[12];
    uint8_t gyro_reg, accel_reg;
    
    // 根据 IMU 类型确定寄存器地址
    switch (imu_ctx.imu_type) {
        case IMU_ICM45686:
        case IMU_ICM42688:
            gyro_reg = 0x25;   // GYRO_DATA_X1
            accel_reg = 0x1F;  // ACCEL_DATA_X1
            break;
        case IMU_BMI270:
            gyro_reg = 0x12;   // DATA_8
            accel_reg = 0x0C;  // DATA_0
            break;
        case IMU_LSM6DSV:
        case IMU_LSM6DSR:
            gyro_reg = 0x22;   // OUTX_L_G
            accel_reg = 0x28;  // OUTX_L_A
            break;
        default:
            return -1;
    }
    
    // 读取陀螺仪
    imu_read_regs(gyro_reg, buf, 6);
    int16_t gx = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t gy = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t gz = (int16_t)(buf[4] | (buf[5] << 8));
    
    // 读取加速度计
    imu_read_regs(accel_reg, buf, 6);
    int16_t ax = (int16_t)(buf[0] | (buf[1] << 8));
    int16_t ay = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t az = (int16_t)(buf[4] | (buf[5] << 8));
    
    // 转换为物理单位
    float deg2rad = 0.01745329252f;
    gyro[0] = (gx * imu_ctx.gyro_scale - imu_ctx.gyro_bias[0]) * deg2rad;
    gyro[1] = (gy * imu_ctx.gyro_scale - imu_ctx.gyro_bias[1]) * deg2rad;
    gyro[2] = (gz * imu_ctx.gyro_scale - imu_ctx.gyro_bias[2]) * deg2rad;
    
    accel[0] = ax * imu_ctx.accel_scale - imu_ctx.accel_bias[0];
    accel[1] = ay * imu_ctx.accel_scale - imu_ctx.accel_bias[1];
    accel[2] = az * imu_ctx.accel_scale - imu_ctx.accel_bias[2];
    
    return 0;
}

int imu_read_raw(int16_t gyro[3], int16_t accel[3])
{
    if (!imu_ctx.initialized) return -1;
    
    uint8_t buf[12];
    
    // 读取原始数据 (根据 IMU 类型)
    switch (imu_ctx.imu_type) {
        case IMU_ICM45686:
        case IMU_ICM42688:
            imu_read_regs(0x1F, buf, 12);  // ACCEL + GYRO
            accel[0] = (int16_t)(buf[0] | (buf[1] << 8));
            accel[1] = (int16_t)(buf[2] | (buf[3] << 8));
            accel[2] = (int16_t)(buf[4] | (buf[5] << 8));
            gyro[0] = (int16_t)(buf[6] | (buf[7] << 8));
            gyro[1] = (int16_t)(buf[8] | (buf[9] << 8));
            gyro[2] = (int16_t)(buf[10] | (buf[11] << 8));
            break;
            
        case IMU_LSM6DSV:
        case IMU_LSM6DSR:
            imu_read_regs(0x22, buf, 12);  // GYRO + ACCEL
            gyro[0] = (int16_t)(buf[0] | (buf[1] << 8));
            gyro[1] = (int16_t)(buf[2] | (buf[3] << 8));
            gyro[2] = (int16_t)(buf[4] | (buf[5] << 8));
            accel[0] = (int16_t)(buf[6] | (buf[7] << 8));
            accel[1] = (int16_t)(buf[8] | (buf[9] << 8));
            accel[2] = (int16_t)(buf[10] | (buf[11] << 8));
            break;
            
        default:
            return -1;
    }
    
    return 0;
}

bool imu_data_ready(void)
{
#ifdef CH59X
    // 检查 INT1 引脚 (PB7)
    return (GPIOB_ReadPortPin(GPIO_Pin_7) != 0);
#else
    return true;
#endif
}

uint8_t imu_get_type(void)
{
    return imu_ctx.imu_type;
}

const char* imu_get_type_name(void)
{
    switch (imu_ctx.imu_type) {
        case IMU_ICM45686: return "ICM-45686";
        case IMU_ICM42688: return "ICM-42688";
        case IMU_BMI270:   return "BMI270";
        case IMU_LSM6DSV:  return "LSM6DSV";
        case IMU_LSM6DSR:  return "LSM6DSR";
        default:           return "Unknown";
    }
}

uint8_t imu_get_interface(void)
{
    return imu_ctx.interface;
}

void imu_set_gyro_bias(const float bias[3])
{
    imu_ctx.gyro_bias[0] = bias[0];
    imu_ctx.gyro_bias[1] = bias[1];
    imu_ctx.gyro_bias[2] = bias[2];
}

void imu_suspend(void)
{
    if (!imu_ctx.initialized) return;
    
    switch (imu_ctx.imu_type) {
        case IMU_ICM45686:
        case IMU_ICM42688:
            imu_write_reg(0x4E, 0x00);  // PWR_MGMT0: sleep
            break;
        case IMU_BMI270:
            imu_write_reg(0x7D, 0x00);  // PWR_CTRL: off
            break;
        case IMU_LSM6DSV:
        case IMU_LSM6DSR:
            imu_write_reg(0x10, 0x00);  // CTRL1: power down
            imu_write_reg(0x11, 0x00);  // CTRL2: power down
            break;
    }
}

void imu_resume(void)
{
    if (!imu_ctx.initialized) return;
    
    // 重新初始化
    switch (imu_ctx.imu_type) {
        case IMU_ICM45686:
        case IMU_ICM42688:
            init_icm45686();
            break;
        case IMU_BMI270:
            init_bmi270();
            break;
        case IMU_LSM6DSV:
        case IMU_LSM6DSR:
            init_lsm6dsv();
            break;
    }
}

/*============================================================================
 * v0.4.24: WOM (Wake on Motion) 支持
 *============================================================================*/

int imu_enable_wom(uint16_t threshold_mg)
{
    if (!imu_ctx.initialized) return -1;
    
    switch (imu_ctx.imu_type) {
        case IMU_ICM45686:
        case IMU_ICM42688:
            // ICM-45686/42688 WOM配置
            // 1. 设置WOM阈值 (0x4A = WOM_X_TH, 0x4B = WOM_Y_TH, 0x4C = WOM_Z_TH)
            // 阈值 = threshold_mg / 4 (4mg分辨率)
            {
                uint8_t th = (uint8_t)(threshold_mg / 4);
                if (th > 255) th = 255;
                imu_write_reg(0x4A, th);
                imu_write_reg(0x4B, th);
                imu_write_reg(0x4C, th);
            }
            // 2. 使能WOM模式
            // SMD_CONFIG (0x57): bit1=WOM_INT_EN, bit0=WOM_EN
            imu_write_reg(0x57, 0x03);
            // 3. 配置INT1输出WOM
            // INT_CONFIG (0x14): bit2=INT1_WOM_EN
            imu_write_reg(0x14, 0x04);
            // 4. 进入低功耗模式，只保留加速度计
            // PWR_MGMT0 (0x4E): bit0-1=Accel LP mode
            imu_write_reg(0x4E, 0x02);  // Accel LP, Gyro off
            break;
            
        case IMU_BMI270:
            // BMI270 WOM配置
            // INT1_IO_CTRL (0x53)
            imu_write_reg(0x53, 0x0A);  // INT1 active high, push-pull
            // INT_MAP_DATA (0x58): map WOM to INT1
            imu_write_reg(0x58, 0x02);
            // 设置阈值和使能 (简化配置)
            imu_write_reg(0x59, (uint8_t)(threshold_mg / 8));  // WOM threshold
            // 低功耗模式
            imu_write_reg(0x7D, 0x04);  // ACC normal, GYRO off
            break;
            
        case IMU_LSM6DSV:
        case IMU_LSM6DSR:
            // LSM6DSV/DSR WOM配置
            // 配置wake-up阈值 WAKE_UP_THS (0x5B)
            {
                uint8_t th = (uint8_t)(threshold_mg / 64);  // 64mg分辨率
                if (th > 63) th = 63;
                imu_write_reg(0x5B, th);
            }
            // 配置wake-up时长
            imu_write_reg(0x5C, 0x00);  // WAKE_UP_DUR
            // 路由到INT1
            imu_write_reg(0x0D, 0x20);  // INT1_CTRL: wake-up
            // 使能中断
            imu_write_reg(0x58, 0x20);  // MD1_CFG: wake-up to INT1
            // 低功耗模式
            imu_write_reg(0x10, 0x10);  // CTRL1: Accel 12.5Hz LP
            imu_write_reg(0x11, 0x00);  // CTRL2: Gyro off
            break;
            
        default:
            return -2;  // 不支持的IMU类型
    }
    
    return 0;
}

int imu_disable_wom(void)
{
    if (!imu_ctx.initialized) return -1;
    
    // 恢复正常工作模式 (重新初始化)
    imu_resume();
    
    return 0;
}
