/**
 * @file gyro_noise_filter.c
 * @brief 陀螺仪噪音优化模块
 * 
 * Gyroscope Noise Optimization Module
 * 
 * 实现多级噪音抑制:
 * 1. 硬件级: 配置 IMU 内部低通滤波器
 * 2. 原始数据级: 移动平均 + 中值滤波
 * 3. 算法级: 自适应卡尔曼滤波
 * 4. 静止检测: 零速更新 (ZUPT)
 * 
 * 噪音特性分析:
 * - 白噪声: 高频随机波动 → 低通滤波
 * - 偏差漂移: 缓慢变化 → 偏差估计
 * - 量化噪声: ADC 分辨率限制 → 抖动抑制
 * - 温漂: 温度变化影响 → 温度补偿
 */

#include "gyro_noise_filter.h"
#include "hal.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * 配置常量 / Configuration Constants
 *============================================================================*/

// 滤波器窗口大小 / Filter window sizes
#define MEDIAN_WINDOW_SIZE      5       // 中值滤波窗口
#define MOVING_AVG_SIZE         4       // 移动平均窗口
#define CALIBRATION_SAMPLES     500     // 校准采样数

// 静止检测阈值 / Rest detection thresholds
#define REST_GYRO_THRESHOLD     0.02f   // rad/s (约 1°/s)
#define REST_ACCEL_THRESHOLD    0.05f   // g
#define REST_TIME_MS            1500    // 静止确认时间

// 自适应滤波参数 / Adaptive filter parameters
#define NOISE_FLOOR_DPS         0.004f  // 0.004 dps 噪声底限
#define ADAPTIVE_ALPHA_MIN      0.1f    // 最小滤波系数
#define ADAPTIVE_ALPHA_MAX      0.9f    // 最大滤波系数

/*============================================================================
 * 数据结构 / Data Structures
 *============================================================================*/

// 中值滤波缓冲区 / Median filter buffer
typedef struct {
    float buffer[MEDIAN_WINDOW_SIZE];
    uint8_t index;
    uint8_t count;
} median_filter_t;

// 移动平均缓冲区 / Moving average buffer
typedef struct {
    float buffer[MOVING_AVG_SIZE];
    float sum;
    uint8_t index;
    uint8_t count;
} moving_avg_t;

// 卡尔曼滤波状态 / Kalman filter state
typedef struct {
    float x;        // 估计值
    float p;        // 估计协方差
    float q;        // 过程噪声
    float r;        // 测量噪声
} kalman_1d_t;

/*============================================================================
 * 静态变量 / Static Variables
 *============================================================================*/

static gyro_filter_state_t filter_state;

// 每轴滤波器 / Per-axis filters
static median_filter_t median_x, median_y, median_z;
static moving_avg_t mavg_x, mavg_y, mavg_z;
static kalman_1d_t kalman_x, kalman_y, kalman_z;

// 校准数据 / Calibration data
static float gyro_offset[3] = {0, 0, 0};
static float gyro_scale[3] = {1.0f, 1.0f, 1.0f};
static float temp_coeff[3] = {0, 0, 0};     // 温度系数
static float calibration_temp = 25.0f;      // 校准温度

// 静止检测 / Rest detection
static uint32_t rest_start_time = 0;
static bool is_resting = false;

/*============================================================================
 * 辅助函数 / Helper Functions
 *============================================================================*/

// 快速排序用于中值滤波 / Quick sort for median filter
static void sort_float(float *arr, int n)
{
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                float temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

// 中值滤波 / Median filter
static float median_filter_update(median_filter_t *f, float input)
{
    f->buffer[f->index] = input;
    f->index = (f->index + 1) % MEDIAN_WINDOW_SIZE;
    if (f->count < MEDIAN_WINDOW_SIZE) f->count++;
    
    // 复制并排序
    float sorted[MEDIAN_WINDOW_SIZE];
    memcpy(sorted, f->buffer, f->count * sizeof(float));
    sort_float(sorted, f->count);
    
    return sorted[f->count / 2];
}

// 移动平均 / Moving average
static float moving_avg_update(moving_avg_t *f, float input)
{
    if (f->count >= MOVING_AVG_SIZE) {
        f->sum -= f->buffer[f->index];
    }
    
    f->buffer[f->index] = input;
    f->sum += input;
    f->index = (f->index + 1) % MOVING_AVG_SIZE;
    if (f->count < MOVING_AVG_SIZE) f->count++;
    
    return f->sum / f->count;
}

// 1D 卡尔曼滤波 / 1D Kalman filter
static float kalman_update(kalman_1d_t *kf, float measurement)
{
    // 预测 / Predict
    // x_pred = x (假设静态模型)
    float p_pred = kf->p + kf->q;
    
    // 更新 / Update
    float k = p_pred / (p_pred + kf->r);
    kf->x = kf->x + k * (measurement - kf->x);
    kf->p = (1.0f - k) * p_pred;
    
    return kf->x;
}

// 初始化 1D 卡尔曼 / Initialize 1D Kalman
static void kalman_init(kalman_1d_t *kf, float q, float r)
{
    kf->x = 0;
    kf->p = 1.0f;
    kf->q = q;
    kf->r = r;
}

/*============================================================================
 * API 实现 / API Implementation
 *============================================================================*/

void gyro_filter_init(const gyro_filter_config_t *config)
{
    memset(&filter_state, 0, sizeof(filter_state));
    
    // 初始化中值滤波器 / Initialize median filters
    memset(&median_x, 0, sizeof(median_filter_t));
    memset(&median_y, 0, sizeof(median_filter_t));
    memset(&median_z, 0, sizeof(median_filter_t));
    
    // 初始化移动平均 / Initialize moving average
    memset(&mavg_x, 0, sizeof(moving_avg_t));
    memset(&mavg_y, 0, sizeof(moving_avg_t));
    memset(&mavg_z, 0, sizeof(moving_avg_t));
    
    // 初始化卡尔曼滤波 / Initialize Kalman filters
    float q = config ? config->process_noise : 0.001f;
    float r = config ? config->measurement_noise : 0.01f;
    kalman_init(&kalman_x, q, r);
    kalman_init(&kalman_y, q, r);
    kalman_init(&kalman_z, q, r);
    
    // 使用配置或默认值 / Use config or defaults
    if (config) {
        filter_state.config = *config;
    } else {
        filter_state.config.enable_median = true;
        filter_state.config.enable_moving_avg = true;
        filter_state.config.enable_kalman = true;
        filter_state.config.enable_rest_detection = true;
        filter_state.config.enable_temp_comp = false;
        filter_state.config.process_noise = 0.001f;
        filter_state.config.measurement_noise = 0.01f;
    }
    
    filter_state.calibrated = false;
    filter_state.sample_count = 0;
}

void gyro_filter_reset(void)
{
    memset(&median_x, 0, sizeof(median_filter_t));
    memset(&median_y, 0, sizeof(median_filter_t));
    memset(&median_z, 0, sizeof(median_filter_t));
    memset(&mavg_x, 0, sizeof(moving_avg_t));
    memset(&mavg_y, 0, sizeof(moving_avg_t));
    memset(&mavg_z, 0, sizeof(moving_avg_t));
    
    kalman_x.x = 0; kalman_x.p = 1.0f;
    kalman_y.x = 0; kalman_y.p = 1.0f;
    kalman_z.x = 0; kalman_z.p = 1.0f;
    
    filter_state.sample_count = 0;
    is_resting = false;
    rest_start_time = 0;
}

int gyro_filter_calibrate(float gyro_samples[][3], int count, float temperature)
{
    if (count < 50) return -1;
    
    // 计算平均偏差 / Calculate average offset
    float sum[3] = {0, 0, 0};
    for (int i = 0; i < count; i++) {
        sum[0] += gyro_samples[i][0];
        sum[1] += gyro_samples[i][1];
        sum[2] += gyro_samples[i][2];
    }
    
    gyro_offset[0] = sum[0] / count;
    gyro_offset[1] = sum[1] / count;
    gyro_offset[2] = sum[2] / count;
    
    // 计算标准差 (用于噪声估计) / Calculate std dev for noise estimation
    float var[3] = {0, 0, 0};
    for (int i = 0; i < count; i++) {
        float dx = gyro_samples[i][0] - gyro_offset[0];
        float dy = gyro_samples[i][1] - gyro_offset[1];
        float dz = gyro_samples[i][2] - gyro_offset[2];
        var[0] += dx * dx;
        var[1] += dy * dy;
        var[2] += dz * dz;
    }
    
    filter_state.noise_level[0] = sqrtf(var[0] / count);
    filter_state.noise_level[1] = sqrtf(var[1] / count);
    filter_state.noise_level[2] = sqrtf(var[2] / count);
    
    // 更新卡尔曼滤波器测量噪声 / Update Kalman measurement noise
    float avg_noise = (filter_state.noise_level[0] + 
                       filter_state.noise_level[1] + 
                       filter_state.noise_level[2]) / 3.0f;
    kalman_x.r = avg_noise * avg_noise;
    kalman_y.r = avg_noise * avg_noise;
    kalman_z.r = avg_noise * avg_noise;
    
    calibration_temp = temperature;
    filter_state.calibrated = true;
    
    return 0;
}

void gyro_filter_process(const float gyro_in[3], float gyro_out[3],
                         const float accel[3], float temperature)
{
    float gx = gyro_in[0];
    float gy = gyro_in[1];
    float gz = gyro_in[2];
    
    // 1. 偏差校正 / Offset correction
    if (filter_state.calibrated) {
        gx -= gyro_offset[0];
        gy -= gyro_offset[1];
        gz -= gyro_offset[2];
        
        // 温度补偿 / Temperature compensation
        if (filter_state.config.enable_temp_comp) {
            float temp_delta = temperature - calibration_temp;
            gx -= temp_coeff[0] * temp_delta;
            gy -= temp_coeff[1] * temp_delta;
            gz -= temp_coeff[2] * temp_delta;
        }
    }
    
    // 2. 中值滤波 (去除尖峰噪声) / Median filter (remove spikes)
    if (filter_state.config.enable_median) {
        gx = median_filter_update(&median_x, gx);
        gy = median_filter_update(&median_y, gy);
        gz = median_filter_update(&median_z, gz);
    }
    
    // 3. 移动平均 (平滑) / Moving average (smoothing)
    if (filter_state.config.enable_moving_avg) {
        gx = moving_avg_update(&mavg_x, gx);
        gy = moving_avg_update(&mavg_y, gy);
        gz = moving_avg_update(&mavg_z, gz);
    }
    
    // 4. 卡尔曼滤波 (自适应噪声抑制) / Kalman filter
    if (filter_state.config.enable_kalman) {
        gx = kalman_update(&kalman_x, gx);
        gy = kalman_update(&kalman_y, gy);
        gz = kalman_update(&kalman_z, gz);
    }
    
    // 5. 静止检测与零速更新 / Rest detection and ZUPT
    if (filter_state.config.enable_rest_detection && accel != NULL) {
        // 计算陀螺仪幅值 / Calculate gyro magnitude
        float gyro_mag = sqrtf(gx*gx + gy*gy + gz*gz);
        
        // 计算加速度幅值偏离 1g 的程度 / Accel magnitude deviation from 1g
        float accel_mag = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
        float accel_dev = fabsf(accel_mag - 1.0f);
        
        // 判断是否静止 / Check if resting
        if (gyro_mag < REST_GYRO_THRESHOLD && accel_dev < REST_ACCEL_THRESHOLD) {
            if (!is_resting) {
                rest_start_time = hal_millis();
                is_resting = true;
            }
            
            // 静止超过阈值时间，更新偏差估计
            // If resting for threshold time, update bias estimate
            uint32_t rest_duration = hal_millis() - rest_start_time;
            if (rest_duration > REST_TIME_MS) {
                // 缓慢更新偏差 / Slowly update bias
                float alpha = 0.001f;
                gyro_offset[0] += alpha * gx;
                gyro_offset[1] += alpha * gy;
                gyro_offset[2] += alpha * gz;
                
                // 零速更新: 强制陀螺仪输出为零
                // ZUPT: Force gyro output to zero
                gx = 0;
                gy = 0;
                gz = 0;
                
                filter_state.is_stationary = true;
            }
        } else {
            is_resting = false;
            filter_state.is_stationary = false;
        }
    }
    
    // 6. 死区处理 (抑制微小噪声) / Deadzone (suppress tiny noise)
    float deadzone = NOISE_FLOOR_DPS * 0.01745329252f;  // 转换为 rad/s
    if (fabsf(gx) < deadzone) gx = 0;
    if (fabsf(gy) < deadzone) gy = 0;
    if (fabsf(gz) < deadzone) gz = 0;
    
    // 输出 / Output
    gyro_out[0] = gx;
    gyro_out[1] = gy;
    gyro_out[2] = gz;
    
    filter_state.sample_count++;
}

void gyro_filter_get_bias(float bias[3])
{
    bias[0] = gyro_offset[0];
    bias[1] = gyro_offset[1];
    bias[2] = gyro_offset[2];
}

void gyro_filter_set_bias(const float bias[3])
{
    gyro_offset[0] = bias[0];
    gyro_offset[1] = bias[1];
    gyro_offset[2] = bias[2];
}

bool gyro_filter_is_stationary(void)
{
    return filter_state.is_stationary;
}

void gyro_filter_get_noise_level(float noise[3])
{
    noise[0] = filter_state.noise_level[0];
    noise[1] = filter_state.noise_level[1];
    noise[2] = filter_state.noise_level[2];
}

/*============================================================================
 * IMU 硬件滤波器配置 / IMU Hardware Filter Configuration
 *============================================================================*/

/**
 * @brief 配置 ICM-45686 内部低通滤波器
 * 
 * ICM-45686 支持多级硬件 LPF:
 * - GYRO_UI_FILT_BW: 0=ODR/2, 1=ODR/4, 2=ODR/8, ...
 * - 推荐: ODR=200Hz 时使用 BW=2 (25Hz 截止)
 */
void gyro_configure_icm45686_filter(uint8_t bandwidth)
{
    // 通过 hal_spi 配置 ICM-45686 寄存器
    // GYRO_CONFIG1 (0x23): GYRO_UI_FILT_BW
    // 这里只是示例，实际需要调用 icm45686 驱动
    (void)bandwidth;
}

/**
 * @brief 配置 LSM6DSV 内部滤波器
 * 
 * LSM6DSV 支持:
 * - CTRL8_XL: 加速度计 LPF2
 * - CTRL6_C/CTRL7_G: 陀螺仪 LPF
 */
void gyro_configure_lsm6dsv_filter(uint8_t bandwidth)
{
    (void)bandwidth;
}
