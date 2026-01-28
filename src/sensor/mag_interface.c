/**
 * @file mag_interface.c
 * @brief 磁力计统一接口实现
 * 
 * 使用规则:
 * 1. 自动检测磁力计
 * 2. 默认禁用，需手动启用
 * 3. IMU 为主，磁力计辅助
 * 4. 偏差过大自动禁用
 */

#include "mag_interface.h"
#include "hal.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * I2C 地址和寄存器
 *============================================================================*/

// QMC5883P
#define QMC_ADDR            0x2C
#define QMC_REG_DATA        0x00
#define QMC_REG_CTRL1       0x09
#define QMC_REG_CHIP_ID     0x0D

// HMC5883L/HMC5983
#define HMC_ADDR            0x1E
#define HMC_REG_CFG_A       0x00
#define HMC_REG_MODE        0x02
#define HMC_REG_DATA        0x03
#define HMC_REG_ID_A        0x0A

// IIS2MDC
#define IIS2_ADDR           0x1E
#define IIS2_REG_WHO        0x4F
#define IIS2_REG_CFG_A      0x60
#define IIS2_REG_DATA       0x68

/*============================================================================
 * 状态
 *============================================================================*/

static struct {
    mag_type_t type;
    mag_state_t state;
    bool enabled;
    
    // 校准
    float hard_iron[3];
    float scale[3];
    bool calibrated;
    
    float min_vals[3];
    float max_vals[3];
    uint32_t cal_samples;
    
    // 场强参考
    float field_ref;
    uint32_t error_count;
} mag = {0};

/*============================================================================
 * 检测
 *============================================================================*/

mag_type_t mag_detect(void)
{
    uint8_t id;
    
    // QMC5883P
    if (hal_i2c_read_reg(QMC_ADDR, QMC_REG_CHIP_ID, &id, 1) == 0) {
        if (id == 0xFF || id == 0x06) return MAG_TYPE_QMC5883P;
    }
    
    // IIS2MDC
    if (hal_i2c_read_reg(IIS2_ADDR, IIS2_REG_WHO, &id, 1) == 0) {
        if (id == 0x40) return MAG_TYPE_IIS2MDC;
    }
    
    // HMC5883L
    uint8_t hmc_id[3];
    if (hal_i2c_read_reg(HMC_ADDR, HMC_REG_ID_A, hmc_id, 3) == 0) {
        if (hmc_id[0] == 'H' && hmc_id[1] == '4' && hmc_id[2] == '3') {
            // 尝试区分 5883L 和 5983
            return MAG_TYPE_HMC5883L;
        }
    }
    
    return MAG_TYPE_NONE;
}

/*============================================================================
 * 初始化
 *============================================================================*/

int mag_init(void)
{
    memset(&mag, 0, sizeof(mag));
    
    mag.scale[0] = mag.scale[1] = mag.scale[2] = 1.0f;
    for (int i = 0; i < 3; i++) {
        mag.min_vals[i] = 1000.0f;
        mag.max_vals[i] = -1000.0f;
    }
    
    mag.type = mag_detect();
    
    if (mag.type == MAG_TYPE_NONE) {
        mag.state = MAG_STATE_DISABLED;
        return -1;
    }
    
    mag.state = MAG_STATE_INIT;
    return 0;
}

/*============================================================================
 * 启用/禁用
 *============================================================================*/

int mag_enable(void)
{
    if (mag.type == MAG_TYPE_NONE) return -1;
    
    int ret = 0;
    
    switch (mag.type) {
        case MAG_TYPE_QMC5883P:
            ret = hal_i2c_write_byte(QMC_ADDR, QMC_REG_CTRL1, 0x1D);  // 200Hz, 8G
            break;
        case MAG_TYPE_HMC5883L:
        case MAG_TYPE_HMC5983:
            hal_i2c_write_byte(HMC_ADDR, HMC_REG_CFG_A, 0x78);
            ret = hal_i2c_write_byte(HMC_ADDR, HMC_REG_MODE, 0x00);
            break;
        case MAG_TYPE_IIS2MDC:
            ret = hal_i2c_write_byte(IIS2_ADDR, IIS2_REG_CFG_A, 0x8C);
            break;
        default:
            return -1;
    }
    
    if (ret == 0) {
        mag.enabled = true;
        mag.state = MAG_STATE_READY;
    }
    
    return ret;
}

int mag_disable(void)
{
    mag.enabled = false;
    mag.state = MAG_STATE_DISABLED;
    return 0;
}

/*============================================================================
 * 数据读取
 *============================================================================*/

int mag_read(mag_data_t *data)
{
    if (!data || !mag.enabled) {
        if (data) data->valid = false;
        return -1;
    }
    
    uint8_t buf[6];
    int16_t raw[3];
    float scale = 1.0f;
    
    switch (mag.type) {
        case MAG_TYPE_QMC5883P:
            if (hal_i2c_read_reg(QMC_ADDR, QMC_REG_DATA, buf, 6) != 0) {
                data->valid = false;
                return -1;
            }
            raw[0] = (int16_t)(buf[1] << 8 | buf[0]);
            raw[1] = (int16_t)(buf[3] << 8 | buf[2]);
            raw[2] = (int16_t)(buf[5] << 8 | buf[4]);
            scale = 0.0125f;
            break;
            
        case MAG_TYPE_HMC5883L:
        case MAG_TYPE_HMC5983:
            if (hal_i2c_read_reg(HMC_ADDR, HMC_REG_DATA, buf, 6) != 0) {
                data->valid = false;
                return -1;
            }
            raw[0] = (int16_t)(buf[0] << 8 | buf[1]);
            raw[2] = (int16_t)(buf[2] << 8 | buf[3]);
            raw[1] = (int16_t)(buf[4] << 8 | buf[5]);
            scale = 0.092f;
            break;
            
        case MAG_TYPE_IIS2MDC:
            if (hal_i2c_read_reg(IIS2_ADDR, IIS2_REG_DATA, buf, 6) != 0) {
                data->valid = false;
                return -1;
            }
            raw[0] = (int16_t)(buf[1] << 8 | buf[0]);
            raw[1] = (int16_t)(buf[3] << 8 | buf[2]);
            raw[2] = (int16_t)(buf[5] << 8 | buf[4]);
            scale = 0.15f;
            break;
            
        default:
            data->valid = false;
            return -1;
    }
    
    // 转换为 uT
    float val[3];
    val[0] = raw[0] * scale;
    val[1] = raw[1] * scale;
    val[2] = raw[2] * scale;
    
    // 应用校准
    data->x = (val[0] - mag.hard_iron[0]) * mag.scale[0];
    data->y = (val[1] - mag.hard_iron[1]) * mag.scale[1];
    data->z = (val[2] - mag.hard_iron[2]) * mag.scale[2];
    
    // 计算航向
    data->heading = atan2f(data->y, data->x) * 180.0f / 3.14159f;
    if (data->heading < 0) data->heading += 360.0f;
    
    data->valid = true;
    
    // 校准数据收集
    if (mag.state == MAG_STATE_CALIBRATING) {
        for (int i = 0; i < 3; i++) {
            if (val[i] < mag.min_vals[i]) mag.min_vals[i] = val[i];
            if (val[i] > mag.max_vals[i]) mag.max_vals[i] = val[i];
        }
        mag.cal_samples++;
    }
    
    // 场强检查
    float field = sqrtf(data->x*data->x + data->y*data->y + data->z*data->z);
    if (mag.field_ref > 0) {
        float deviation = fabsf(field - mag.field_ref) / mag.field_ref;
        if (deviation > 0.5f) {
            mag.error_count++;
            if (mag.error_count > 100) {
                mag_disable();  // 自动禁用
            }
        } else {
            mag.error_count = 0;
        }
    } else {
        mag.field_ref = field;
    }
    
    return 0;
}

/*============================================================================
 * 校准
 *============================================================================*/

int mag_calibrate_start(void)
{
    if (!mag.enabled) return -1;
    
    for (int i = 0; i < 3; i++) {
        mag.min_vals[i] = 1000.0f;
        mag.max_vals[i] = -1000.0f;
    }
    mag.cal_samples = 0;
    mag.state = MAG_STATE_CALIBRATING;
    
    return 0;
}

int mag_calibrate_stop(void)
{
    if (mag.state != MAG_STATE_CALIBRATING) return -1;
    if (mag.cal_samples < 100) return -1;
    
    // 计算硬铁偏移
    for (int i = 0; i < 3; i++) {
        mag.hard_iron[i] = (mag.max_vals[i] + mag.min_vals[i]) / 2.0f;
    }
    
    // 计算缩放
    float range[3];
    for (int i = 0; i < 3; i++) {
        range[i] = (mag.max_vals[i] - mag.min_vals[i]) / 2.0f;
    }
    float avg_range = (range[0] + range[1] + range[2]) / 3.0f;
    
    for (int i = 0; i < 3; i++) {
        mag.scale[i] = avg_range / range[i];
    }
    
    mag.field_ref = avg_range;
    mag.calibrated = true;
    mag.state = MAG_STATE_READY;
    
    return 0;
}

/*============================================================================
 * 状态查询
 *============================================================================*/

mag_type_t mag_get_type(void) { return mag.type; }
mag_state_t mag_get_state(void) { return mag.state; }
bool mag_is_enabled(void) { return mag.enabled; }
bool mag_is_calibrated(void) { return mag.calibrated; }

const char* mag_get_name(void)
{
    switch (mag.type) {
        case MAG_TYPE_QMC5883P: return "QMC5883P";
        case MAG_TYPE_HMC5883L: return "HMC5883L";
        case MAG_TYPE_HMC5983:  return "HMC5983";
        case MAG_TYPE_IIS2MDC:  return "IIS2MDC";
        default: return "None";
    }
}

float mag_get_heading(void)
{
    mag_data_t data;
    if (mag_read(&data) == 0 && data.valid) {
        return data.heading;
    }
    return 0.0f;
}
