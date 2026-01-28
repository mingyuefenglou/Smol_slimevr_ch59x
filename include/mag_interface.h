/**
 * @file mag_interface.h
 * @brief 磁力计统一接口
 * 
 * 支持: QMC5883P, HMC5883L, HMC5983, IIS2MDC
 */

#ifndef __MAG_INTERFACE_H__
#define __MAG_INTERFACE_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MAG_TYPE_NONE = 0,
    MAG_TYPE_QMC5883P,
    MAG_TYPE_HMC5883L,
    MAG_TYPE_HMC5983,
    MAG_TYPE_IIS2MDC
} mag_type_t;

typedef enum {
    MAG_STATE_DISABLED = 0,
    MAG_STATE_INIT,
    MAG_STATE_READY,
    MAG_STATE_CALIBRATING,
    MAG_STATE_ERROR
} mag_state_t;

typedef struct {
    float x, y, z;          // uT
    float heading;          // degrees
    bool valid;
} mag_data_t;

// 初始化和检测
int mag_init(void);
mag_type_t mag_detect(void);
mag_type_t mag_get_type(void);
const char* mag_get_name(void);

// 控制
int mag_enable(void);
int mag_disable(void);
bool mag_is_enabled(void);
mag_state_t mag_get_state(void);

// 数据
int mag_read(mag_data_t *data);
float mag_get_heading(void);

// 校准
int mag_calibrate_start(void);
int mag_calibrate_stop(void);
bool mag_is_calibrated(void);

#endif
