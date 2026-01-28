/**
 * @file temp_compensation.c
 * @brief 陀螺仪温度漂移补偿
 * 
 * 原理: 陀螺仪偏移随温度变化 (约 0.03 dps/°C)
 * 模型: offset = a + b*(T-Tref) + c*(T-Tref)^2
 */

#include "hal.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * 配置
 *============================================================================*/

#define TEMP_REF_C          25.0f       // 参考温度
#define TEMP_MIN_C          -10.0f
#define TEMP_MAX_C          60.0f
#define TEMP_LEARN_ALPHA    0.001f      // 系数学习率
#define TEMP_STABLE_THRESH  0.5f        // 温度稳定阈值

/*============================================================================
 * 状态
 *============================================================================*/

typedef struct {
    // 多项式系数 (每轴)
    float coeff_a[3];   // 常数项
    float coeff_b[3];   // 一次项
    float coeff_c[3];   // 二次项
    
    // 当前温度
    float current_temp;
    float temp_rate;    // °C/s
    uint32_t last_update_ms;
    
    // 补偿值
    float compensation[3];
    
    // 统计
    float temp_min;
    float temp_max;
} temp_comp_t;

static temp_comp_t tc = {0};

/*============================================================================
 * 默认系数
 *============================================================================*/

static void load_defaults(void)
{
    // 典型 ICM42688 温度系数 (rad/s/°C)
    for (int i = 0; i < 3; i++) {
        tc.coeff_a[i] = 0.0f;
        tc.coeff_b[i] = 0.00052f;   // ~0.03 dps/°C
        tc.coeff_c[i] = 0.000001f;  // 小的二次项
    }
}

/*============================================================================
 * 初始化
 *============================================================================*/

void temp_comp_init(void)
{
    memset(&tc, 0, sizeof(tc));
    tc.current_temp = TEMP_REF_C;
    tc.last_update_ms = hal_get_tick_ms();
    
    // 尝试从 Flash 加载
    uint8_t data[48];
    if (hal_storage_read(0x100, data, 48) == 0) {
        uint32_t magic;
        memcpy(&magic, data, 4);
        if (magic == 0x54454D50) {  // "TEMP"
            memcpy(tc.coeff_a, &data[4], 12);
            memcpy(tc.coeff_b, &data[16], 12);
            memcpy(tc.coeff_c, &data[28], 12);
            return;
        }
    }
    
    load_defaults();
}

/*============================================================================
 * 温度更新
 *============================================================================*/

void temp_comp_update_temp(float temp_c)
{
    uint32_t now = hal_get_tick_ms();
    float dt = (now - tc.last_update_ms) / 1000.0f;
    tc.last_update_ms = now;
    
    // 限制范围
    if (temp_c < TEMP_MIN_C) temp_c = TEMP_MIN_C;
    if (temp_c > TEMP_MAX_C) temp_c = TEMP_MAX_C;
    
    // 计算变化率
    if (dt > 0 && dt < 1.0f) {
        float new_rate = (temp_c - tc.current_temp) / dt;
        tc.temp_rate = tc.temp_rate * 0.9f + new_rate * 0.1f;
    }
    
    tc.current_temp = temp_c;
    
    // 更新统计
    if (temp_c < tc.temp_min || tc.temp_min == 0) tc.temp_min = temp_c;
    if (temp_c > tc.temp_max) tc.temp_max = temp_c;
}

/*============================================================================
 * 应用补偿
 *============================================================================*/

void temp_comp_apply(float gyro[3])
{
    float t = tc.current_temp - TEMP_REF_C;
    float t2 = t * t;
    
    for (int i = 0; i < 3; i++) {
        tc.compensation[i] = tc.coeff_a[i] + tc.coeff_b[i] * t + tc.coeff_c[i] * t2;
        gyro[i] -= tc.compensation[i];
    }
}

/*============================================================================
 * 自适应学习
 *============================================================================*/

void temp_comp_learn(const float observed_offset[3])
{
    // 只在温度稳定时学习
    if (fabsf(tc.temp_rate) > TEMP_STABLE_THRESH) return;
    
    float t = tc.current_temp - TEMP_REF_C;
    float t2 = t * t;
    
    for (int i = 0; i < 3; i++) {
        float predicted = tc.coeff_a[i] + tc.coeff_b[i] * t + tc.coeff_c[i] * t2;
        float error = observed_offset[i] - predicted;
        
        // 梯度下降更新
        tc.coeff_a[i] += error * TEMP_LEARN_ALPHA;
        tc.coeff_b[i] += error * t * TEMP_LEARN_ALPHA;
        tc.coeff_c[i] += error * t2 * TEMP_LEARN_ALPHA * 0.1f;
    }
}

/*============================================================================
 * 保存系数
 *============================================================================*/

void temp_comp_save(void)
{
    uint8_t data[48];
    uint32_t magic = 0x54454D50;
    memcpy(data, &magic, 4);
    memcpy(&data[4], tc.coeff_a, 12);
    memcpy(&data[16], tc.coeff_b, 12);
    memcpy(&data[28], tc.coeff_c, 12);
    hal_storage_write(0x100, data, 44);
}

/*============================================================================
 * 状态查询
 *============================================================================*/

float temp_comp_get_temp(void)  { return tc.current_temp; }
float temp_comp_get_rate(void)  { return tc.temp_rate; }

void temp_comp_get_compensation(float comp[3])
{
    memcpy(comp, tc.compensation, sizeof(float) * 3);
}
