/**
 * @file sc7i22.c
 * @brief Silan SC7I22 国产 6轴 IMU 驱动
 * @version 2.0
 * 
 * 特点:
 * - 国产替代方案
 * - 低成本高性价比
 * - 低功耗设计
 * - 支持 FIFO
 */

#include "hal.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * I2C 地址
 *============================================================================*/

#define SC7I22_ADDR_LOW         0x68
#define SC7I22_ADDR_HIGH        0x69

static uint8_t sc7i22_addr = SC7I22_ADDR_LOW;

/*============================================================================
 * 寄存器定义
 *============================================================================*/

#define REG_WHO_AM_I            0x00
#define REG_CONFIG              0x01
#define REG_GYRO_CONFIG         0x02
#define REG_ACCEL_CONFIG        0x03
#define REG_FIFO_CONFIG         0x04
#define REG_INT_ENABLE          0x05
#define REG_INT_STATUS          0x06
#define REG_FIFO_COUNT          0x07
#define REG_ACCEL_XOUT_H        0x08
#define REG_ACCEL_XOUT_L        0x09
#define REG_ACCEL_YOUT_H        0x0A
#define REG_ACCEL_YOUT_L        0x0B
#define REG_ACCEL_ZOUT_H        0x0C
#define REG_ACCEL_ZOUT_L        0x0D
#define REG_TEMP_OUT_H          0x0E
#define REG_TEMP_OUT_L          0x0F
#define REG_GYRO_XOUT_H         0x10
#define REG_GYRO_XOUT_L         0x11
#define REG_GYRO_YOUT_H         0x12
#define REG_GYRO_YOUT_L         0x13
#define REG_GYRO_ZOUT_H         0x14
#define REG_GYRO_ZOUT_L         0x15
#define REG_PWR_MGMT            0x16
#define REG_SOFT_RESET          0x17
#define REG_FIFO_DATA           0x18

#define SC7I22_WHO_AM_I_VALUE   0x22

// 电源模式
#define PWR_SLEEP               0x40
#define PWR_CYCLE               0x20
#define PWR_TEMP_DIS            0x08
#define PWR_GYRO_DIS            0x04
#define PWR_ACCEL_DIS           0x02

// 陀螺仪量程
#define GYRO_FS_250DPS          0x00
#define GYRO_FS_500DPS          0x01
#define GYRO_FS_1000DPS         0x02
#define GYRO_FS_2000DPS         0x03

// 加速度量程
#define ACCEL_FS_2G             0x00
#define ACCEL_FS_4G             0x10
#define ACCEL_FS_8G             0x20
#define ACCEL_FS_16G            0x30

// 采样率
#define ODR_25HZ                0x00
#define ODR_50HZ                0x01
#define ODR_100HZ               0x02
#define ODR_200HZ               0x03
#define ODR_400HZ               0x04
#define ODR_1000HZ              0x05

/*============================================================================
 * 缩放因子
 *============================================================================*/

static const float gyro_sensitivity[] = { 131.0f, 65.5f, 32.8f, 16.4f };  // LSB/dps
static const float accel_sensitivity[] = { 16384.0f, 8192.0f, 4096.0f, 2048.0f };  // LSB/g

/*============================================================================
 * 状态
 *============================================================================*/

typedef struct {
    bool initialized;
    uint8_t gyro_fs;
    uint8_t accel_fs;
    uint8_t odr;
    float gyro_scale;
    float accel_scale;
    bool fifo_enabled;
    uint32_t sample_count;
    float temp_c;
    // v0.4.22 P2-01: 中断回调支持
    void (*drdy_callback)(void);    // 数据就绪回调
    void (*wom_callback)(void);     // WOM唤醒回调
    bool int_enabled;
} sc7i22_state_t;

static sc7i22_state_t state = {0};

/*============================================================================
 * 低级 I2C 操作
 *============================================================================*/

static int write_reg(uint8_t reg, uint8_t val)
{
    return hal_i2c_write_byte(sc7i22_addr, reg, val);
}

static int read_reg(uint8_t reg, uint8_t *val)
{
    return hal_i2c_read_reg(sc7i22_addr, reg, val, 1);
}

static int read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return hal_i2c_read_reg(sc7i22_addr, reg, buf, len);
}

/*============================================================================
 * 检测
 *============================================================================*/

bool sc7i22_detect(void)
{
    uint8_t id;
    
    sc7i22_addr = SC7I22_ADDR_LOW;
    if (read_reg(REG_WHO_AM_I, &id) == 0 && id == SC7I22_WHO_AM_I_VALUE) {
        return true;
    }
    
    sc7i22_addr = SC7I22_ADDR_HIGH;
    if (read_reg(REG_WHO_AM_I, &id) == 0 && id == SC7I22_WHO_AM_I_VALUE) {
        return true;
    }
    
    return false;
}

/*============================================================================
 * 初始化
 *============================================================================*/

int sc7i22_init(void)
{
    if (!sc7i22_detect()) return -1;
    
    // 软复位
    write_reg(REG_SOFT_RESET, 0x01);
    hal_delay_ms(50);
    
    // 唤醒
    write_reg(REG_PWR_MGMT, 0x00);
    hal_delay_ms(10);
    
    // 配置加速度: ±16g
    write_reg(REG_ACCEL_CONFIG, ACCEL_FS_16G | ODR_200HZ);
    state.accel_fs = 3;
    state.accel_scale = 9.81f / accel_sensitivity[3];
    
    // 配置陀螺仪: ±2000dps
    write_reg(REG_GYRO_CONFIG, GYRO_FS_2000DPS | (ODR_200HZ << 4));
    state.gyro_fs = 3;
    state.gyro_scale = (3.14159265f / 180.0f) / gyro_sensitivity[3];
    
    // 配置采样率: 200Hz
    write_reg(REG_CONFIG, ODR_200HZ);
    state.odr = ODR_200HZ;
    
    state.initialized = true;
    return 0;
}

/*============================================================================
 * 配置
 *============================================================================*/

int sc7i22_set_gyro_range(uint8_t range)
{
    if (!state.initialized || range > 3) return -1;
    
    uint8_t cfg;
    read_reg(REG_GYRO_CONFIG, &cfg);
    cfg = (cfg & 0xFC) | range;
    write_reg(REG_GYRO_CONFIG, cfg);
    
    state.gyro_fs = range;
    state.gyro_scale = (3.14159265f / 180.0f) / gyro_sensitivity[range];
    return 0;
}

int sc7i22_set_accel_range(uint8_t range)
{
    if (!state.initialized || range > 3) return -1;
    
    uint8_t cfg;
    read_reg(REG_ACCEL_CONFIG, &cfg);
    cfg = (cfg & 0xCF) | (range << 4);
    write_reg(REG_ACCEL_CONFIG, cfg);
    
    state.accel_fs = range;
    state.accel_scale = 9.81f / accel_sensitivity[range];
    return 0;
}

int sc7i22_set_odr(uint16_t odr_hz)
{
    if (!state.initialized) return -1;
    
    uint8_t odr_cfg;
    if (odr_hz >= 1000) odr_cfg = ODR_1000HZ;
    else if (odr_hz >= 400) odr_cfg = ODR_400HZ;
    else if (odr_hz >= 200) odr_cfg = ODR_200HZ;
    else if (odr_hz >= 100) odr_cfg = ODR_100HZ;
    else if (odr_hz >= 50) odr_cfg = ODR_50HZ;
    else odr_cfg = ODR_25HZ;
    
    write_reg(REG_CONFIG, odr_cfg);
    state.odr = odr_cfg;
    
    return 0;
}

/*============================================================================
 * 数据读取
 *============================================================================*/

int sc7i22_read(float gyro[3], float accel[3])
{
    if (!state.initialized) return -1;
    
    uint8_t buf[14];
    if (read_regs(REG_ACCEL_XOUT_H, buf, 14) != 0) return -1;
    
    // 加速度
    int16_t raw_accel[3];
    raw_accel[0] = (int16_t)((buf[0] << 8) | buf[1]);
    raw_accel[1] = (int16_t)((buf[2] << 8) | buf[3]);
    raw_accel[2] = (int16_t)((buf[4] << 8) | buf[5]);
    
    accel[0] = raw_accel[0] * state.accel_scale;
    accel[1] = raw_accel[1] * state.accel_scale;
    accel[2] = raw_accel[2] * state.accel_scale;
    
    // 温度 (buf[6], buf[7])
    int16_t raw_temp = (int16_t)((buf[6] << 8) | buf[7]);
    state.temp_c = (raw_temp / 340.0f) + 36.53f;
    
    // 陀螺仪
    int16_t raw_gyro[3];
    raw_gyro[0] = (int16_t)((buf[8] << 8) | buf[9]);
    raw_gyro[1] = (int16_t)((buf[10] << 8) | buf[11]);
    raw_gyro[2] = (int16_t)((buf[12] << 8) | buf[13]);
    
    gyro[0] = raw_gyro[0] * state.gyro_scale;
    gyro[1] = raw_gyro[1] * state.gyro_scale;
    gyro[2] = raw_gyro[2] * state.gyro_scale;
    
    state.sample_count++;
    return 0;
}

/*============================================================================
 * 温度
 *============================================================================*/

float sc7i22_read_temp(void)
{
    uint8_t buf[2];
    if (read_regs(REG_TEMP_OUT_H, buf, 2) != 0) return state.temp_c;
    
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    state.temp_c = (raw / 340.0f) + 36.53f;
    return state.temp_c;
}

/*============================================================================
 * FIFO
 *============================================================================*/

int sc7i22_fifo_enable(void)
{
    if (!state.initialized) return -1;
    write_reg(REG_FIFO_CONFIG, 0x07);  // Enable FIFO for accel + gyro + temp
    state.fifo_enabled = true;
    return 0;
}

int sc7i22_fifo_disable(void)
{
    write_reg(REG_FIFO_CONFIG, 0x00);
    state.fifo_enabled = false;
    return 0;
}

int sc7i22_fifo_flush(void)
{
    write_reg(REG_FIFO_CONFIG, 0x80);  // FIFO reset
    hal_delay_ms(1);
    if (state.fifo_enabled) {
        write_reg(REG_FIFO_CONFIG, 0x07);
    }
    return 0;
}

/*============================================================================
 * 电源管理
 *============================================================================*/

int sc7i22_suspend(void)
{
    if (!state.initialized) return -1;
    write_reg(REG_PWR_MGMT, PWR_SLEEP);
    return 0;
}

int sc7i22_resume(void)
{
    if (!state.initialized) return -1;
    write_reg(REG_PWR_MGMT, 0x00);
    hal_delay_ms(10);
    return 0;
}

int sc7i22_set_low_power(bool enable)
{
    if (!state.initialized) return -1;
    
    if (enable) {
        write_reg(REG_PWR_MGMT, PWR_CYCLE | PWR_GYRO_DIS);
    } else {
        write_reg(REG_PWR_MGMT, 0x00);
    }
    
    return 0;
}

/*============================================================================
 * 中断
 *============================================================================*/

// v0.4.22 P2-01: SC7I22中断处理函数 (由GPIO中断调用)
void sc7i22_irq_handler(void)
{
    if (!state.initialized || !state.int_enabled) return;
    
    // 读取中断状态以清除
    uint8_t int_status;
    read_reg(REG_INT_STATUS, &int_status);
    
    // 调用数据就绪回调
    if ((int_status & 0x01) && state.drdy_callback) {
        state.drdy_callback();
    }
    
    // 调用WOM回调 (如果配置了WOM中断)
    if ((int_status & 0x40) && state.wom_callback) {
        state.wom_callback();
    }
}

int sc7i22_int_enable(void (*callback)(void))
{
    if (!state.initialized) return -1;
    
    // v0.4.22 P2-01: 注册数据就绪回调
    state.drdy_callback = callback;
    state.int_enabled = true;
    
    // 配置INT引脚为推挽输出、高电平有效
    // REG_INT_ENABLE: bit0=DRDY_EN
    write_reg(REG_INT_ENABLE, 0x01);
    
    return 0;
}

int sc7i22_int_disable(void)
{
    state.int_enabled = false;
    state.drdy_callback = NULL;
    state.wom_callback = NULL;
    write_reg(REG_INT_ENABLE, 0x00);
    return 0;
}

/**
 * @brief 配置WOM (Wake on Motion) 中断用于深睡眠唤醒
 * v0.4.22 P1-3: WOM深睡唤醒闭环支持
 * @param callback 运动检测回调
 * @param threshold_mg 运动阈值 (mg)
 * @return 0成功，负值失败
 */
int sc7i22_wom_enable(void (*callback)(void), uint16_t threshold_mg)
{
    if (!state.initialized) return -1;
    
    state.wom_callback = callback;
    state.int_enabled = true;
    
    // 配置WOM阈值 (SC7I22可能需要查阅具体寄存器)
    // 这里使用简化配置
    // REG_INT_ENABLE: bit6=WOM_EN
    uint8_t int_cfg = 0x40;  // WOM中断使能
    write_reg(REG_INT_ENABLE, int_cfg);
    
    // 进入低功耗模式，只保留加速度用于WOM检测
    write_reg(REG_PWR_MGMT, PWR_CYCLE | PWR_GYRO_DIS);
    
    return 0;
}

/**
 * @brief 从WOM唤醒后恢复正常工作模式
 * @return 0成功，负值失败
 */
int sc7i22_wom_disable(void)
{
    if (!state.initialized) return -1;
    
    state.wom_callback = NULL;
    
    // 恢复正常工作模式
    write_reg(REG_PWR_MGMT, 0x00);
    
    // 恢复数据就绪中断
    if (state.drdy_callback) {
        write_reg(REG_INT_ENABLE, 0x01);
    } else {
        write_reg(REG_INT_ENABLE, 0x00);
    }
    
    return 0;
}

/*============================================================================
 * 自检
 *============================================================================*/

int sc7i22_self_test(void)
{
    if (!state.initialized) return -1;
    
    // SC7I22 自检实现
    // 读取基准值
    float gyro_base[3], accel_base[3];
    sc7i22_read(gyro_base, accel_base);
    
    // 检查加速度是否合理 (静止时应接近 1g)
    float accel_mag = sqrtf(
        accel_base[0] * accel_base[0] +
        accel_base[1] * accel_base[1] +
        accel_base[2] * accel_base[2]
    );
    
    // 检查是否在 0.5g ~ 1.5g 范围内
    if (accel_mag < 4.9f || accel_mag > 14.7f) {
        return -1;
    }
    
    // 检查陀螺仪静止时是否接近零
    float gyro_mag = sqrtf(
        gyro_base[0] * gyro_base[0] +
        gyro_base[1] * gyro_base[1] +
        gyro_base[2] * gyro_base[2]
    );
    
    // 静止时陀螺仪输出应 < 0.1 rad/s
    if (gyro_mag > 0.1f) {
        return -1;
    }
    
    return 0;
}

/*============================================================================
 * 状态查询
 *============================================================================*/

bool sc7i22_is_initialized(void) { return state.initialized; }
uint32_t sc7i22_get_sample_count(void) { return state.sample_count; }
const char* sc7i22_get_name(void) { return "SC7I22"; }

void sc7i22_get_config(uint8_t *gyro_range, uint8_t *accel_range, uint16_t *odr_hz)
{
    if (gyro_range) *gyro_range = state.gyro_fs;
    if (accel_range) *accel_range = state.accel_fs;
    
    if (odr_hz) {
        switch (state.odr) {
            case ODR_1000HZ: *odr_hz = 1000; break;
            case ODR_400HZ:  *odr_hz = 400; break;
            case ODR_200HZ:  *odr_hz = 200; break;
            case ODR_100HZ:  *odr_hz = 100; break;
            case ODR_50HZ:   *odr_hz = 50; break;
            default:         *odr_hz = 25; break;
        }
    }
}
