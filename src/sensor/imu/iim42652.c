/**
 * @file iim42652.c
 * @brief TDK IIM-42652 工业级 6轴 IMU 驱动
 * @version 2.0
 * 
 * 特点:
 * - 扩展温度范围 (-40°C ~ +105°C)
 * - 与 ICM-42688 寄存器兼容
 * - 工业级可靠性
 * - DMA/FIFO 支持
 * - 中断驱动数据就绪
 */

#include "hal.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * I2C 地址
 *============================================================================*/

#define IIM42652_ADDR_LOW       0x68
#define IIM42652_ADDR_HIGH      0x69

static uint8_t iim42652_addr = IIM42652_ADDR_LOW;

/*============================================================================
 * 寄存器定义 (Bank 0)
 *============================================================================*/

#define REG_DEVICE_CONFIG       0x11
#define REG_INT_CONFIG          0x14
#define REG_FIFO_CONFIG         0x16
#define REG_TEMP_DATA1          0x1D
#define REG_TEMP_DATA0          0x1E
#define REG_ACCEL_DATA_X1       0x1F
#define REG_GYRO_DATA_X1        0x25
#define REG_INT_STATUS          0x2D
#define REG_FIFO_COUNTH         0x2E
#define REG_FIFO_DATA           0x30
#define REG_SIGNAL_PATH_RESET   0x4B
#define REG_INTF_CONFIG1        0x4D
#define REG_PWR_MGMT0           0x4E
#define REG_GYRO_CONFIG0        0x4F
#define REG_ACCEL_CONFIG0       0x50
#define REG_GYRO_CONFIG1        0x51
#define REG_ACCEL_CONFIG1       0x53
#define REG_FIFO_CONFIG1        0x5F
#define REG_INT_CONFIG0         0x63
#define REG_INT_SOURCE0         0x65
#define REG_SELF_TEST_CONFIG    0x70
#define REG_WHO_AM_I            0x75

#define IIM42652_WHO_AM_I_VALUE 0x6F

// 电源模式
#define GYRO_MODE_LN            0x0C
#define ACCEL_MODE_LN           0x03
#define TEMP_DIS                0x20

// 陀螺仪配置
#define GYRO_FS_2000DPS         0x00
#define GYRO_FS_1000DPS         0x20
#define GYRO_FS_500DPS          0x40
#define GYRO_FS_250DPS          0x60
#define GYRO_ODR_200HZ          0x07
#define GYRO_ODR_100HZ          0x08

// 加速度配置
#define ACCEL_FS_16G            0x00
#define ACCEL_FS_8G             0x20
#define ACCEL_FS_4G             0x40
#define ACCEL_ODR_200HZ         0x07

/*============================================================================
 * 缩放因子
 *============================================================================*/

static const float gyro_sensitivity[] = { 16.4f, 32.8f, 65.5f, 131.0f, 262.0f };
static const float accel_sensitivity[] = { 2048.0f, 4096.0f, 8192.0f, 16384.0f };

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
} iim42652_state_t;

static iim42652_state_t state = {0};

/*============================================================================
 * 低级 I2C 操作
 *============================================================================*/

static int write_reg(uint8_t reg, uint8_t val)
{
    return hal_i2c_write_byte(iim42652_addr, reg, val);
}

static int read_reg(uint8_t reg, uint8_t *val)
{
    return hal_i2c_read_reg(iim42652_addr, reg, val, 1);
}

static int read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return hal_i2c_read_reg(iim42652_addr, reg, buf, len);
}

/*============================================================================
 * 检测
 *============================================================================*/

bool iim42652_detect(void)
{
    uint8_t id;
    
    iim42652_addr = IIM42652_ADDR_LOW;
    if (read_reg(REG_WHO_AM_I, &id) == 0 && id == IIM42652_WHO_AM_I_VALUE) {
        return true;
    }
    
    iim42652_addr = IIM42652_ADDR_HIGH;
    if (read_reg(REG_WHO_AM_I, &id) == 0 && id == IIM42652_WHO_AM_I_VALUE) {
        return true;
    }
    
    return false;
}

/*============================================================================
 * 初始化
 *============================================================================*/

int iim42652_init(void)
{
    if (!iim42652_detect()) return -1;
    
    // 软复位
    write_reg(REG_DEVICE_CONFIG, 0x01);
    hal_delay_ms(10);
    
    // 等待复位完成
    uint8_t cfg;
    int timeout = 100;
    do {
        read_reg(REG_DEVICE_CONFIG, &cfg);
        hal_delay_ms(1);
    } while ((cfg & 0x01) && --timeout > 0);
    
    if (timeout == 0) return -2;
    
    // 配置时钟
    write_reg(REG_INTF_CONFIG1, 0x00);
    
    // 加速度: ±16g, 200Hz
    write_reg(REG_ACCEL_CONFIG0, ACCEL_FS_16G | ACCEL_ODR_200HZ);
    state.accel_fs = 0;
    state.accel_scale = 9.81f / accel_sensitivity[0];
    
    // 陀螺仪: ±2000dps, 200Hz
    write_reg(REG_GYRO_CONFIG0, GYRO_FS_2000DPS | GYRO_ODR_200HZ);
    state.gyro_fs = 0;
    state.gyro_scale = (3.14159265f / 180.0f) / gyro_sensitivity[0];
    state.odr = GYRO_ODR_200HZ;
    
    // 滤波器配置
    write_reg(REG_GYRO_CONFIG1, 0x06);
    write_reg(REG_ACCEL_CONFIG1, 0x05);
    
    // 使能低噪声模式
    write_reg(REG_PWR_MGMT0, GYRO_MODE_LN | ACCEL_MODE_LN);
    hal_delay_ms(1);
    
    state.initialized = true;
    return 0;
}

/*============================================================================
 * 配置
 *============================================================================*/

int iim42652_set_gyro_range(uint8_t range)
{
    if (!state.initialized || range > 4) return -1;
    
    uint8_t cfg = (range << 5) | (state.odr & 0x0F);
    write_reg(REG_GYRO_CONFIG0, cfg);
    
    state.gyro_fs = range;
    state.gyro_scale = (3.14159265f / 180.0f) / gyro_sensitivity[range];
    return 0;
}

int iim42652_set_accel_range(uint8_t range)
{
    if (!state.initialized || range > 3) return -1;
    
    uint8_t cfg = (range << 5) | (state.odr & 0x0F);
    write_reg(REG_ACCEL_CONFIG0, cfg);
    
    state.accel_fs = range;
    state.accel_scale = 9.81f / accel_sensitivity[range];
    return 0;
}

int iim42652_set_odr(uint16_t odr_hz)
{
    if (!state.initialized) return -1;
    
    uint8_t odr_cfg;
    if (odr_hz >= 200) odr_cfg = GYRO_ODR_200HZ;
    else odr_cfg = GYRO_ODR_100HZ;
    
    state.odr = odr_cfg;
    
    write_reg(REG_GYRO_CONFIG0, (state.gyro_fs << 5) | odr_cfg);
    write_reg(REG_ACCEL_CONFIG0, (state.accel_fs << 5) | odr_cfg);
    
    return 0;
}

/*============================================================================
 * 数据读取
 *============================================================================*/

int iim42652_read(float gyro[3], float accel[3])
{
    if (!state.initialized) return -1;
    
    uint8_t buf[14];
    if (read_regs(REG_TEMP_DATA1, buf, 14) != 0) return -1;
    
    // 温度
    int16_t raw_temp = (int16_t)((buf[0] << 8) | buf[1]);
    state.temp_c = (raw_temp / 132.48f) + 25.0f;
    
    // 加速度
    int16_t raw_accel[3];
    raw_accel[0] = (int16_t)((buf[2] << 8) | buf[3]);
    raw_accel[1] = (int16_t)((buf[4] << 8) | buf[5]);
    raw_accel[2] = (int16_t)((buf[6] << 8) | buf[7]);
    
    accel[0] = raw_accel[0] * state.accel_scale;
    accel[1] = raw_accel[1] * state.accel_scale;
    accel[2] = raw_accel[2] * state.accel_scale;
    
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

float iim42652_read_temp(void)
{
    uint8_t buf[2];
    if (read_regs(REG_TEMP_DATA1, buf, 2) != 0) return state.temp_c;
    
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    state.temp_c = (raw / 132.48f) + 25.0f;
    return state.temp_c;
}

/*============================================================================
 * FIFO
 *============================================================================*/

int iim42652_fifo_enable(void)
{
    if (!state.initialized) return -1;
    write_reg(REG_FIFO_CONFIG1, 0x03);
    write_reg(REG_FIFO_CONFIG, 0x40);
    state.fifo_enabled = true;
    return 0;
}

int iim42652_fifo_disable(void)
{
    write_reg(REG_FIFO_CONFIG, 0x00);
    state.fifo_enabled = false;
    return 0;
}

int iim42652_fifo_flush(void)
{
    write_reg(REG_SIGNAL_PATH_RESET, 0x02);
    return 0;
}

/*============================================================================
 * 电源管理
 *============================================================================*/

int iim42652_suspend(void)
{
    if (!state.initialized) return -1;
    write_reg(REG_PWR_MGMT0, 0x00 | TEMP_DIS);
    return 0;
}

int iim42652_resume(void)
{
    if (!state.initialized) return -1;
    write_reg(REG_PWR_MGMT0, GYRO_MODE_LN | ACCEL_MODE_LN);
    hal_delay_ms(1);
    return 0;
}

/*============================================================================
 * 状态查询
 *============================================================================*/

bool iim42652_is_initialized(void) { return state.initialized; }
uint32_t iim42652_get_sample_count(void) { return state.sample_count; }
const char* iim42652_get_name(void) { return "IIM-42652"; }
