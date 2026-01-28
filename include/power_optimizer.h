/**
 * @file power_optimizer.h
 * @brief 功耗优化模块
 * 
 * 功能:
 * 1. 动态时钟调节 (60MHz/32MHz/8MHz)
 * 2. 智能睡眠调度
 * 3. 外设按需开关
 * 4. RF占空比优化
 * 5. 功耗估算与报告
 * 
 * 目标: 活动模式功耗 < 20mA
 */

#ifndef __POWER_OPTIMIZER_H__
#define __POWER_OPTIMIZER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 配置常量
 *============================================================================*/

// 时钟模式
#define CLK_MODE_HIGH           0   // 60MHz (全速)
#define CLK_MODE_MEDIUM         1   // 32MHz (平衡)
#define CLK_MODE_LOW            2   // 8MHz (省电)

// RF功率配置
#define RF_PROFILE_HIGH         0   // 高功率 (+7dBm)
#define RF_PROFILE_MEDIUM       1   // 中等功率 (+4dBm)
#define RF_PROFILE_LOW          2   // 低功率 (0dBm)
#define RF_PROFILE_AUTO         3   // 自动调节

/*============================================================================
 * 数据结构
 *============================================================================*/

typedef struct {
    float current_ma;       // 当前估计功耗 (mA)
    float average_ma;       // 平均功耗 (mA)
    uint8_t clock_mode;     // 当前时钟模式
    uint8_t rf_profile;     // 当前RF功率配置
    uint8_t duty_cycle;     // RF占空比 (%)
    uint32_t idle_time_ms;  // 空闲时间
    bool rf_enabled;        // RF是否启用
    bool imu_enabled;       // IMU是否启用
} power_report_t;

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化功耗优化模块
 */
void power_optimizer_init(void);

/**
 * @brief 设置自动睡眠
 * @param enable 是否启用
 * @param timeout_ms 空闲超时时间
 */
void power_optimizer_set_auto_sleep(bool enable, uint32_t timeout_ms);

/**
 * @brief 更新电池状态
 * @param percent 电量百分比
 * @param charging 是否充电中
 */
void power_optimizer_set_battery(uint8_t percent, bool charging);

/**
 * @brief 获取功耗报告
 * @param report 输出报告
 */
void power_optimizer_get_report(power_report_t *report);

/**
 * @brief 设置时钟模式
 * @param mode CLK_MODE_HIGH/MEDIUM/LOW
 */
void power_set_clock_mode(uint8_t mode);

/**
 * @brief 设置RF功率配置
 * @param profile RF_PROFILE_HIGH/MEDIUM/LOW/AUTO
 */
void power_set_rf_profile(uint8_t profile);

/**
 * @brief 更新功耗估算 (主循环中调用)
 * @param gyro_mag 陀螺仪幅度 (用于运动检测)
 * @param accel_mag 加速度计幅度
 */
void power_update(float gyro_mag, float accel_mag);

/**
 * @brief 进入空闲模式 (快速唤醒)
 */
void power_enter_idle(void);

/**
 * @brief 进入停止模式 (中等唤醒)
 * @param wake_ms 唤醒时间
 */
void power_enter_halt(uint32_t wake_ms);

/**
 * @brief 进入关机模式 (需复位)
 */
void power_enter_shutdown(void);

/**
 * @brief 启用/禁用RF
 */
void power_enable_rf(void);
void power_disable_rf(void);

/**
 * @brief 启用/禁用IMU
 */
void power_enable_imu(void);
void power_disable_imu(void);

/**
 * @brief 启用/禁用LED
 */
void power_enable_led(void);
void power_disable_led(void);

/**
 * @brief 获取RF占空比
 * @return 占空比百分比
 */
uint8_t power_optimizer_get_duty_cycle(void);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_OPTIMIZER_H__ */
