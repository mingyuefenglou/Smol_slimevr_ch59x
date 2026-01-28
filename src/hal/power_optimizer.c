/**
 * @file power_optimizer.c
 * @brief 深度睡眠和功耗优化模块
 * 
 * 目标: 功耗 < 20mA (活动模式)
 * 
 * 优化方案:
 * 1. 智能睡眠调度
 * 2. 外设按需开关
 * 3. 时钟动态调整
 * 4. RF 占空比优化
 * 5. 传感器省电模式
 */

#include "hal.h"
#include "config.h"
#include "power_optimizer.h"
#include "rf_hw.h"
#include "imu_interface.h"
#include <string.h>

/*============================================================================
 * 功耗预算 (目标 < 20mA)
 *============================================================================*/

/*
 * 组件功耗分析:
 * 
 * | 组件              | 活动 (mA) | 睡眠 (uA) | 优化后 (mA) |
 * |-------------------|-----------|-----------|-------------|
 * | CH592 MCU (60MHz) | 8.0       | 2.0       | 6.0 (降频)  |
 * | CH592 MCU (32MHz) | 4.5       | 2.0       | 4.5         |
 * | RF TX (+4dBm)     | 12.5      | -         | 9.0 (0dBm)  |
 * | RF 平均 (10%占空) | 3.0       | -         | 1.2         |
 * | IMU (200Hz)       | 0.65      | 3.0       | 0.4 (100Hz) |
 * | LED               | 5.0       | 0         | 1.0 (PWM)   |
 * | 其他              | 1.0       | 0.5       | 0.8         |
 * |-------------------|-----------|-----------|-------------|
 * | 总计              | 17.7      | 5.5       | 13.9        |
 */

/*============================================================================
 * 配置
 *============================================================================*/

// 睡眠模式
#define SLEEP_MODE_IDLE         0   // 空闲 (快速唤醒)
#define SLEEP_MODE_HALT         1   // 停止 (慢速唤醒)
#define SLEEP_MODE_SHUTDOWN     2   // 关机 (需复位)

// 时钟模式
#define CLK_MODE_HIGH           0   // 60MHz (全速)
#define CLK_MODE_MEDIUM         1   // 32MHz (平衡)
#define CLK_MODE_LOW            2   // 8MHz (省电)

// 触发阈值
#define IDLE_TIMEOUT_MS         100     // 空闲超时
#define MOTION_THRESHOLD        0.5f    // 运动检测阈值 (rad/s)
#define LOW_BATTERY_PERCENT     15      // 低电量阈值

/*============================================================================
 * 前向声明
 *============================================================================*/

static void power_update_current_estimate(void);

/*============================================================================
 * 状态
 *============================================================================*/

typedef struct {
    // 电源状态
    uint8_t sleep_mode;
    uint8_t clock_mode;
    bool rf_enabled;
    bool imu_enabled;
    bool led_enabled;
    
    // 运动检测
    bool is_moving;
    uint32_t last_motion_ms;
    uint32_t idle_time_ms;
    
    // 功耗统计
    float current_ma;
    float average_ma;
    uint32_t active_time_ms;
    uint32_t sleep_time_ms;
    
    // 电池
    uint8_t battery_percent;
    bool low_battery;
    bool charging;
    
    // 配置
    bool auto_sleep_enabled;
    uint32_t auto_sleep_timeout_ms;
} power_state_t;

static power_state_t pwr = {0};

/*============================================================================
 * CH59X 电源寄存器
 *============================================================================*/

// 系统控制
#define R8_SYS_CFG          (*((volatile uint8_t *)0x40001000))
#define R8_CLK_CFG          (*((volatile uint8_t *)0x40001008))
#define R8_CLK_MOD_AUX      (*((volatile uint8_t *)0x40001009))
#define R8_PWR_CTRL         (*((volatile uint8_t *)0x40001020))
#define R8_PWR_CFG          (*((volatile uint8_t *)0x40001021))

// 电源模式位
#define RB_SLP_USBHS_PWRDN  0x01
#define RB_SLP_USB_PWRDN    0x02
#define RB_SLP_BLE_PWRDN    0x04
#define RB_SLP_RAM_PWRDN    0x08

/*============================================================================
 * 时钟控制
 *============================================================================*/

void power_set_clock_mode(uint8_t mode)
{
    switch (mode) {
        case CLK_MODE_HIGH:     // 60MHz
            R8_CLK_CFG = 0x00;  // PLL / 1
            break;
            
        case CLK_MODE_MEDIUM:   // 32MHz
            R8_CLK_CFG = 0x01;  // PLL / 2
            break;
            
        case CLK_MODE_LOW:      // 8MHz
            R8_CLK_CFG = 0x07;  // PLL / 8
            break;
    }
    
    pwr.clock_mode = mode;
    
    // 更新功耗估算
    power_update_current_estimate();
}

/*============================================================================
 * 睡眠控制
 *============================================================================*/

void power_enter_idle(void)
{
    // 空闲模式: WFI (Wait For Interrupt)
    // 保持所有外设和 RAM
    pwr.sleep_mode = SLEEP_MODE_IDLE;
    
    __asm volatile ("wfi");
}

void power_enter_halt(uint32_t wake_ms)
{
    // 停止模式: 关闭主时钟，保持 RTC
    pwr.sleep_mode = SLEEP_MODE_HALT;
    
    // 保存状态
    uint8_t saved_pwr = R8_PWR_CFG;
    
    // 配置唤醒源
    // RTC 定时唤醒
    if (wake_ms > 0) {
        // 配置 RTC 定时器
        // TODO: 实现RTC闹钟功能
        // hal_rtc_set_alarm(wake_ms);
    }
    
    // 配置低功耗
    R8_PWR_CFG |= RB_SLP_USB_PWRDN;     // 关闭 USB
    R8_PWR_CFG |= RB_SLP_USBHS_PWRDN;   // 关闭 USB HS
    
    // 关闭不必要的外设时钟
    // ...
    
    // 进入 HALT
    __asm volatile ("wfi");
    
    // 唤醒后恢复
    R8_PWR_CFG = saved_pwr;
    pwr.sleep_mode = SLEEP_MODE_IDLE;
}

void power_enter_shutdown(void)
{
    // 关机模式: 仅保留最小功耗
    pwr.sleep_mode = SLEEP_MODE_SHUTDOWN;
    
    // 关闭所有外设
    power_disable_rf();
    power_disable_imu();
    power_disable_led();
    
    // 配置唤醒 (按键)
    hal_gpio_set_interrupt(PIN_SW0, HAL_GPIO_INT_FALLING, NULL);
    
    // 关闭 RAM (如果不需要保持数据)
    R8_PWR_CFG |= RB_SLP_RAM_PWRDN;
    
    // 进入深度睡眠
    __asm volatile ("wfi");
    
    // 如果唤醒，重新初始化
    // 通常需要软复位
}

/*============================================================================
 * 外设电源控制
 *============================================================================*/

void power_enable_rf(void)
{
    if (pwr.rf_enabled) return;
    
    // 使能 RF 电源
    R8_PWR_CFG &= ~RB_SLP_BLE_PWRDN;
    hal_delay_us(100);
    
    // 重新初始化 RF
    rf_hw_init_default();
    
    pwr.rf_enabled = true;
    power_update_current_estimate();
}

void power_disable_rf(void)
{
    if (!pwr.rf_enabled) return;
    
    // 关闭 RF
    // TODO: 实现RF关闭功能
    // rf_hw_shutdown();
    R8_PWR_CFG |= RB_SLP_BLE_PWRDN;
    
    pwr.rf_enabled = false;
    power_update_current_estimate();
}

void power_enable_imu(void)
{
    if (pwr.imu_enabled) return;
    
    // 唤醒 IMU
    imu_resume();
    
    pwr.imu_enabled = true;
    power_update_current_estimate();
}

void power_disable_imu(void)
{
    if (!pwr.imu_enabled) return;
    
    // 让 IMU 进入睡眠
    imu_suspend();
    
    pwr.imu_enabled = false;
    power_update_current_estimate();
}

void power_enable_led(void)
{
    pwr.led_enabled = true;
}

void power_disable_led(void)
{
    hal_gpio_write(PIN_LED, false);
    pwr.led_enabled = false;
}

/*============================================================================
 * 功耗估算
 *============================================================================*/

void power_update_current_estimate(void)
{
    float current = 0.0f;
    
    // MCU 基础功耗
    switch (pwr.clock_mode) {
        case CLK_MODE_HIGH:   current += 8.0f; break;
        case CLK_MODE_MEDIUM: current += 4.5f; break;
        case CLK_MODE_LOW:    current += 2.0f; break;
    }
    
    // RF 功耗 (假设 10% 占空比)
    if (pwr.rf_enabled) {
        current += 1.2f;    // 平均功耗
    }
    
    // IMU 功耗
    if (pwr.imu_enabled) {
        current += 0.65f;
    }
    
    // LED 功耗 (假设 PWM 20%)
    if (pwr.led_enabled) {
        current += 1.0f;
    }
    
    // 其他
    current += 0.8f;
    
    pwr.current_ma = current;
    
    // 移动平均
    pwr.average_ma = pwr.average_ma * 0.95f + current * 0.05f;
}

/*============================================================================
 * 智能睡眠调度
 *============================================================================*/

void power_update(float gyro_mag, float accel_mag)
{
    uint32_t now = hal_get_tick_ms();
    
    // 运动检测
    bool was_moving = pwr.is_moving;
    pwr.is_moving = (gyro_mag > MOTION_THRESHOLD);
    
    if (pwr.is_moving) {
        pwr.last_motion_ms = now;
        pwr.idle_time_ms = 0;
        
        // 运动时切换到高性能模式
        if (pwr.clock_mode != CLK_MODE_HIGH) {
            power_set_clock_mode(CLK_MODE_HIGH);
        }
        
        // 确保 RF 开启
        if (!pwr.rf_enabled) {
            power_enable_rf();
        }
        
        pwr.active_time_ms++;
    } else {
        pwr.idle_time_ms = now - pwr.last_motion_ms;
        
        // 空闲超时处理
        if (pwr.idle_time_ms > IDLE_TIMEOUT_MS) {
            // 降低时钟
            if (pwr.clock_mode == CLK_MODE_HIGH) {
                power_set_clock_mode(CLK_MODE_MEDIUM);
            }
        }
        
        if (pwr.idle_time_ms > IDLE_TIMEOUT_MS * 10) {
            // 进一步降频
            if (pwr.clock_mode != CLK_MODE_LOW) {
                power_set_clock_mode(CLK_MODE_LOW);
            }
        }
        
        // 自动睡眠
        if (pwr.auto_sleep_enabled && 
            pwr.idle_time_ms > pwr.auto_sleep_timeout_ms) {
            power_enter_halt(5000);  // 5秒后唤醒检查
        }
        
        pwr.sleep_time_ms++;
    }
    
    // 电池管理
    if (pwr.low_battery && !pwr.charging) {
        // 低电量时强制省电
        power_set_clock_mode(CLK_MODE_LOW);
        power_disable_led();
    }
}

/*============================================================================
 * RF 占空比优化
 *============================================================================*/

typedef struct {
    uint8_t duty_percent;   // 发射占空比 (0-100)
    uint8_t tx_power_dbm;   // 发射功率
    uint16_t interval_ms;   // 发送间隔
} rf_power_profile_t;

// 预定义配置文件
static const rf_power_profile_t rf_profiles[] = {
    // 高性能 (最低延迟)
    {100, 4, 5},
    // 平衡
    {50, 0, 10},
    // 省电
    {20, -4, 25},
    // 超省电
    {10, -8, 50},
};

static uint8_t current_rf_profile = 0;

void power_set_rf_profile(uint8_t profile)
{
    if (profile >= sizeof(rf_profiles)/sizeof(rf_profiles[0])) return;
    
    current_rf_profile = profile;
    const rf_power_profile_t *p = &rf_profiles[profile];
    
    // 应用配置
    rf_hw_set_tx_power(p->tx_power_dbm);
    // rf_set_interval(p->interval_ms);  // 在 RF 模块中实现
    
    power_update_current_estimate();
}

/*============================================================================
 * 公共接口
 *============================================================================*/

void power_optimizer_init(void)
{
    memset(&pwr, 0, sizeof(pwr));
    
    pwr.clock_mode = CLK_MODE_HIGH;
    pwr.rf_enabled = true;
    pwr.imu_enabled = true;
    pwr.led_enabled = true;
    pwr.auto_sleep_enabled = false;
    pwr.auto_sleep_timeout_ms = 30000;  // 30秒
    
    power_update_current_estimate();
}

void power_optimizer_set_auto_sleep(bool enable, uint32_t timeout_ms)
{
    pwr.auto_sleep_enabled = enable;
    pwr.auto_sleep_timeout_ms = timeout_ms;
}

void power_optimizer_set_battery(uint8_t percent, bool charging)
{
    pwr.battery_percent = percent;
    pwr.charging = charging;
    pwr.low_battery = (percent < LOW_BATTERY_PERCENT);
}

float power_optimizer_get_current(void)
{
    return pwr.current_ma;
}

float power_optimizer_get_average(void)
{
    return pwr.average_ma;
}

uint8_t power_optimizer_get_duty_cycle(void)
{
    uint32_t total = pwr.active_time_ms + pwr.sleep_time_ms;
    if (total == 0) return 100;
    return (pwr.active_time_ms * 100) / total;
}

/*============================================================================
 * 功耗报告
 *============================================================================*/

// power_report_t 已在头文件中定义，这里不再重复定义

void power_optimizer_get_report(power_report_t *report)
{
    if (!report) return;
    
    report->current_ma = pwr.current_ma;
    report->average_ma = pwr.average_ma;
    report->clock_mode = pwr.clock_mode;
    report->rf_profile = current_rf_profile;
    report->duty_cycle = power_optimizer_get_duty_cycle();
    report->idle_time_ms = pwr.idle_time_ms;
    report->rf_enabled = pwr.rf_enabled;
    report->imu_enabled = pwr.imu_enabled;
}

/*============================================================================
 * 电池续航估算
 *============================================================================*/

float power_optimizer_estimate_runtime(uint16_t battery_mah)
{
    if (pwr.average_ma < 0.1f) return 0.0f;
    
    // 简单估算: 容量 / 平均电流
    float hours = battery_mah / pwr.average_ma;
    
    // 考虑效率 (假设 85%)
    hours *= 0.85f;
    
    return hours;
}
