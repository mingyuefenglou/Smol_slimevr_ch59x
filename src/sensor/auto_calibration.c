/**
 * @file auto_calibration.c
 * @brief 运动中自动校准
 * 
 * 功能:
 * 1. 静止检测 + 陀螺仪偏移校准
 * 2. 运动中漂移补偿
 * 3. 加速度计校准
 * 4. 磁力计融合校准
 */

#include "hal.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * 配置
 *============================================================================*/

#define GYRO_STILL_THRESHOLD    0.02f   // rad/s
#define ACCEL_STILL_THRESHOLD   0.3f    // m/s^2 偏离 1g
#define STILL_TIME_MS           1000    // 静止判定时间

#define GYRO_OFFSET_ALPHA       0.001f  // 偏移更新率
#define GYRO_DRIFT_ALPHA        0.0001f // 漂移补偿率
#define ACCEL_SCALE_ALPHA       0.01f   // 加速度缩放率

#define MAG_HEADING_THRESHOLD   30.0f   // 航向偏差阈值 (度)
#define MAG_CORRECTION_ALPHA    0.005f  // 磁力计修正率

/*============================================================================
 * 校准状态
 *============================================================================*/

typedef struct {
    // 陀螺仪偏移
    float gyro_offset[3];
    float gyro_offset_accum[3];
    uint32_t gyro_samples;
    
    // 漂移补偿
    float gyro_drift_rate[3];
    float gyro_drift_accum[3];
    uint32_t last_drift_ms;
    
    // 加速度缩放
    float accel_scale[3];
    
    // 磁力计
    bool mag_enabled;
    bool mag_calibrated;
    float mag_heading_offset;
    float mag_hard_iron[3];
    
    // 静止检测
    bool is_still;
    uint32_t still_start_ms;
    float gyro_variance;
    
    // 校准状态
    bool calibration_valid;
    uint32_t calibration_count;
} auto_calib_t;

static auto_calib_t ac = {0};

/*============================================================================
 * 初始化
 *============================================================================*/

void auto_calib_init(void)
{
    memset(&ac, 0, sizeof(ac));
    
    // 默认缩放
    ac.accel_scale[0] = 1.0f;
    ac.accel_scale[1] = 1.0f;
    ac.accel_scale[2] = 1.0f;
    
    ac.last_drift_ms = hal_get_tick_ms();
}

/*============================================================================
 * 静止检测
 *============================================================================*/

static void update_still_detection(const float gyro[3], const float accel[3])
{
    // 陀螺仪幅值
    float gyro_mag = sqrtf(gyro[0]*gyro[0] + gyro[1]*gyro[1] + gyro[2]*gyro[2]);
    
    // 加速度偏离 1g
    float accel_mag = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
    float accel_dev = fabsf(accel_mag - 9.81f);
    
    // 判断静止
    bool currently_still = (gyro_mag < GYRO_STILL_THRESHOLD) && 
                           (accel_dev < ACCEL_STILL_THRESHOLD);
    
    if (currently_still) {
        if (!ac.is_still) {
            ac.still_start_ms = hal_get_tick_ms();
            ac.is_still = true;
        }
        ac.gyro_variance = ac.gyro_variance * 0.9f + gyro_mag * 0.1f;
    } else {
        ac.is_still = false;
    }
}

/*============================================================================
 * 陀螺仪偏移校准 (静止时)
 *============================================================================*/

static void calibrate_gyro_offset(const float gyro[3])
{
    uint32_t still_duration = hal_get_tick_ms() - ac.still_start_ms;
    
    if (still_duration > STILL_TIME_MS) {
        // 累积样本
        ac.gyro_offset_accum[0] += gyro[0];
        ac.gyro_offset_accum[1] += gyro[1];
        ac.gyro_offset_accum[2] += gyro[2];
        ac.gyro_samples++;
        
        // 200 个样本后更新
        if (ac.gyro_samples >= 200) {
            float new_offset[3];
            new_offset[0] = ac.gyro_offset_accum[0] / ac.gyro_samples;
            new_offset[1] = ac.gyro_offset_accum[1] / ac.gyro_samples;
            new_offset[2] = ac.gyro_offset_accum[2] / ac.gyro_samples;
            
            // 平滑更新
            for (int i = 0; i < 3; i++) {
                ac.gyro_offset[i] += (new_offset[i] - ac.gyro_offset[i]) * GYRO_OFFSET_ALPHA;
            }
            
            // 重置
            memset(ac.gyro_offset_accum, 0, sizeof(ac.gyro_offset_accum));
            ac.gyro_samples = 0;
            ac.calibration_count++;
            ac.calibration_valid = true;
        }
    }
}

/*============================================================================
 * 漂移补偿 (运动中)
 *============================================================================*/

static void compensate_gyro_drift(float gyro[3])
{
    uint32_t now = hal_get_tick_ms();
    float dt = (now - ac.last_drift_ms) / 1000.0f;
    ac.last_drift_ms = now;
    
    if (dt > 0 && dt < 1.0f) {
        // 累积漂移
        for (int i = 0; i < 3; i++) {
            ac.gyro_drift_accum[i] += ac.gyro_drift_rate[i] * dt;
            gyro[i] -= ac.gyro_drift_accum[i];
        }
    }
    
    // 静止时更新漂移率
    if (ac.is_still && ac.calibration_valid) {
        for (int i = 0; i < 3; i++) {
            ac.gyro_drift_rate[i] += gyro[i] * GYRO_DRIFT_ALPHA;
        }
    }
}

/*============================================================================
 * 加速度缩放校准
 *============================================================================*/

static void calibrate_accel_scale(const float accel[3])
{
    if (!ac.is_still) return;
    
    uint32_t still_duration = hal_get_tick_ms() - ac.still_start_ms;
    if (still_duration < STILL_TIME_MS * 2) return;
    
    float accel_mag = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
    
    if (accel_mag > 8.0f && accel_mag < 12.0f) {
        float scale = 9.81f / accel_mag;
        for (int i = 0; i < 3; i++) {
            ac.accel_scale[i] += (scale - ac.accel_scale[i]) * ACCEL_SCALE_ALPHA;
        }
    }
}

/*============================================================================
 * 磁力计辅助校正
 *============================================================================*/

void auto_calib_mag_update(float imu_heading, float mag_heading)
{
    if (!ac.mag_enabled) return;
    
    // 计算航向差
    float diff = mag_heading - imu_heading - ac.mag_heading_offset;
    
    // 归一化到 ±180°
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    
    if (fabsf(diff) < MAG_HEADING_THRESHOLD) {
        // 偏差小，使用磁力计修正
        ac.mag_heading_offset += diff * MAG_CORRECTION_ALPHA;
        ac.mag_calibrated = true;
    } else if (fabsf(diff) > MAG_HEADING_THRESHOLD * 2) {
        // 偏差过大，重新校准
        ac.mag_heading_offset = mag_heading - imu_heading;
    }
}

bool auto_calib_should_use_mag(float imu_heading, float mag_heading)
{
    if (!ac.mag_enabled || !ac.mag_calibrated) return false;
    
    float diff = mag_heading - imu_heading - ac.mag_heading_offset;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    
    return fabsf(diff) < MAG_HEADING_THRESHOLD;
}

/*============================================================================
 * 主处理函数
 *============================================================================*/

void auto_calib_update(float gyro[3], float accel[3])
{
    // 静止检测
    update_still_detection(gyro, accel);
    
    // 应用偏移
    gyro[0] -= ac.gyro_offset[0];
    gyro[1] -= ac.gyro_offset[1];
    gyro[2] -= ac.gyro_offset[2];
    
    // 静止校准
    if (ac.is_still) {
        calibrate_gyro_offset(gyro);
        calibrate_accel_scale(accel);
    }
    
    // 漂移补偿
    compensate_gyro_drift(gyro);
    
    // 应用加速度缩放
    accel[0] *= ac.accel_scale[0];
    accel[1] *= ac.accel_scale[1];
    accel[2] *= ac.accel_scale[2];
}

/*============================================================================
 * 状态查询
 *============================================================================*/

bool auto_calib_is_still(void)     { return ac.is_still; }
bool auto_calib_is_valid(void)     { return ac.calibration_valid; }
uint32_t auto_calib_count(void)    { return ac.calibration_count; }

void auto_calib_get_gyro_offset(float offset[3])
{
    memcpy(offset, ac.gyro_offset, sizeof(float) * 3);
}

void auto_calib_enable_mag(bool enable) { ac.mag_enabled = enable; }
bool auto_calib_mag_calibrated(void)    { return ac.mag_calibrated; }

void auto_calib_force(void)
{
    memset(ac.gyro_offset, 0, sizeof(ac.gyro_offset));
    memset(ac.gyro_drift_rate, 0, sizeof(ac.gyro_drift_rate));
    memset(ac.gyro_drift_accum, 0, sizeof(ac.gyro_drift_accum));
    ac.calibration_valid = false;
    ac.calibration_count = 0;
}
