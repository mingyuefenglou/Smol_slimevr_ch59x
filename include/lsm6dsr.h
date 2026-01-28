/**
 * @file lsm6dsr.h
 * @brief LSM6DSR IMU Driver Header
 */

#ifndef __LSM6DSR_H__
#define __LSM6DSR_H__

#include "optimize.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int lsm6dsr_init(void);
int lsm6dsr_read_accel(float *ax, float *ay, float *az);
int lsm6dsr_read_gyro(float *gx, float *gy, float *gz);
int lsm6dsr_read_all(float gyro[3], float accel[3]);
int lsm6dsr_read_raw(int16_t gyro[3], int16_t accel[3]);
int lsm6dsr_read_temp(float *temp);
bool lsm6dsr_data_ready(void);
int lsm6dsr_set_odr(uint16_t odr_hz);
void lsm6dsr_suspend(void);
void lsm6dsr_resume(void);

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DSR_H__ */
