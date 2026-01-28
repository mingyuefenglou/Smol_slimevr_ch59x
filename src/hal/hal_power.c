/**
 * @file hal_power.c
 * @brief 智能功耗管理
 * 
 * 目标: 平均功耗 < 20mA
 * 
 * 功耗模式:
 *   ACTIVE:      ~30mA  (全速运行)
 *   IDLE:        ~15mA  (CPU 空闲)
 *   LIGHT_SLEEP: ~5mA   (外设部分关闭)
 *   DEEP_SLEEP:  ~10uA  (仅 RTC 运行)
 * 
 * 策略:
 *   - 时隙间隔自动进入 IDLE
 *   - 长时间无活动进入 LIGHT_SLEEP
 *   - 关机进入 DEEP_SLEEP
 */

#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * 配置
 *============================================================================*/

#define IDLE_THRESHOLD_MS       5       // 5ms 无活动进入 IDLE
#define LIGHT_SLEEP_MS          30000   // 30s 无活动进入轻睡眠
#define DEEP_SLEEP_MS           300000  // 5min 无活动进入深睡眠

/*============================================================================
 * 状态
 *============================================================================*/

// 重命名枚举以避免与SDK冲突
typedef enum {
    HAL_PWR_MODE_ACTIVE = 0,
    HAL_PWR_MODE_IDLE,
    HAL_PWR_MODE_LIGHT_SLEEP,
    HAL_PWR_MODE_DEEP_SLEEP
} power_mode_t;

typedef struct {
    power_mode_t mode;
    uint32_t mode_enter_ms;
    uint32_t last_activity_ms;
    uint32_t last_rf_activity_ms;
    
    // 唤醒源
    uint8_t wakeup_sources;
    
    // 外设状态
    uint8_t periph_mask;
    
    // 统计
    uint32_t active_ms;
    uint32_t idle_ms;
    uint32_t sleep_ms;
    uint16_t estimated_current_ma;
} power_state_t;

static power_state_t pwr = {0};

/*============================================================================
 * 初始化
 *============================================================================*/

void hal_power_init(void)
{
    memset(&pwr, 0, sizeof(pwr));
    pwr.mode = HAL_PWR_MODE_ACTIVE;
    pwr.mode_enter_ms = hal_get_tick_ms();
    pwr.last_activity_ms = pwr.mode_enter_ms;
    pwr.last_rf_activity_ms = pwr.mode_enter_ms;
    pwr.wakeup_sources = 0x07;  // RF + Timer + GPIO
    pwr.periph_mask = 0xFF;     // 全部使能
}

/*============================================================================
 * 外设电源控制
 *============================================================================*/

#define PERIPH_SPI      (1 << 0)
#define PERIPH_I2C      (1 << 1)
#define PERIPH_ADC      (1 << 2)
#define PERIPH_USB      (1 << 3)
#define PERIPH_RF       (1 << 4)

void hal_power_periph_enable(uint8_t periph)
{
    pwr.periph_mask |= periph;
    
#ifdef CH59X
    // TODO: 实现外设时钟使能
    // R32_SAFE_ACCESS_SIG = 0x57;
    // R32_SAFE_ACCESS_SIG = 0xA8;
    // if (periph & PERIPH_SPI) R8_SLP_CLK_OFF0 &= ~(1 << 3);
    // if (periph & PERIPH_I2C) R8_SLP_CLK_OFF0 &= ~(1 << 4);
    // if (periph & PERIPH_ADC) R8_SLP_CLK_OFF0 &= ~(1 << 5);
    // if (periph & PERIPH_USB) R8_SLP_CLK_OFF0 &= ~(1 << 1);
    // R32_SAFE_ACCESS_SIG = 0;
#endif
}

void hal_power_periph_disable(uint8_t periph)
{
    pwr.periph_mask &= ~periph;
    
#ifdef CH59X
    // TODO: 实现外设时钟禁用
    // R32_SAFE_ACCESS_SIG = 0x57;
    // R32_SAFE_ACCESS_SIG = 0xA8;
    // if (periph & PERIPH_SPI) R8_SLP_CLK_OFF0 |= (1 << 3);
    // if (periph & PERIPH_I2C) R8_SLP_CLK_OFF0 |= (1 << 4);
    // if (periph & PERIPH_ADC) R8_SLP_CLK_OFF0 |= (1 << 5);
    // R32_SAFE_ACCESS_SIG = 0;
#endif
}

/*============================================================================
 * 功耗模式切换
 *============================================================================*/

static void enter_mode(power_mode_t mode)
{
    if (mode == pwr.mode) return;
    
    uint32_t now = hal_get_tick_ms();
    uint32_t duration = now - pwr.mode_enter_ms;
    
    // 统计时间
    switch (pwr.mode) {
        case HAL_PWR_MODE_ACTIVE:
            pwr.active_ms += duration;
            break;
        case HAL_PWR_MODE_IDLE:
            pwr.idle_ms += duration;
            break;
        case HAL_PWR_MODE_LIGHT_SLEEP:
        case HAL_PWR_MODE_DEEP_SLEEP:
            pwr.sleep_ms += duration;
            break;
    }
    
#ifdef CH59X
    switch (mode) {
        case HAL_PWR_MODE_ACTIVE:
            SetSysClock(CLK_SOURCE_PLL_60MHz);
            break;
            
        case HAL_PWR_MODE_IDLE:
            // WFI 等待中断
            __WFI();
            break;
            
        case HAL_PWR_MODE_LIGHT_SLEEP:
            // 关闭不需要的外设
            hal_power_periph_disable(PERIPH_SPI | PERIPH_I2C | PERIPH_ADC);
            
            // 降频
            SetSysClock(CLK_SOURCE_HSI);  // 使用HSI (32MHz)
            
            // 进入轻睡眠
            LowPower_Idle();  // 使用Idle模式
            
            // 唤醒后恢复
            SetSysClock(CLK_SOURCE_PLL_60MHz);
            hal_power_periph_enable(PERIPH_SPI | PERIPH_I2C);
            break;
            
        case HAL_PWR_MODE_DEEP_SLEEP:
            // 配置唤醒源
            if (pwr.wakeup_sources & 0x04) {  // GPIO
                GPIOA_ITModeCfg(GPIO_Pin_4, GPIO_ITMode_FallEdge);
                PFIC_EnableIRQ(GPIO_A_IRQn);
            }
            
            // 进入深睡眠 (系统会复位)
            LowPower_Shutdown(0);
            break;
    }
#endif
    
    pwr.mode = mode;
    pwr.mode_enter_ms = hal_get_tick_ms();
}

/*============================================================================
 * 活动标记
 *============================================================================*/

void hal_power_activity(void)
{
    pwr.last_activity_ms = hal_get_tick_ms();
    
    // 如果在睡眠状态，唤醒
    if (pwr.mode != HAL_PWR_MODE_ACTIVE) {
        enter_mode(HAL_PWR_MODE_ACTIVE);
    }
}

void hal_power_rf_activity(void)
{
    pwr.last_rf_activity_ms = hal_get_tick_ms();
    hal_power_activity();
}

/*============================================================================
 * 自动功耗管理 (在主循环调用)
 *============================================================================*/

void hal_power_manage(void)
{
    uint32_t now = hal_get_tick_ms();
    uint32_t idle_time = now - pwr.last_activity_ms;
    uint32_t rf_idle = now - pwr.last_rf_activity_ms;
    
    // 深睡眠检查 (5 分钟无活动)
    if (idle_time > DEEP_SLEEP_MS) {
        enter_mode(HAL_PWR_MODE_DEEP_SLEEP);
        return;
    }
    
    // 轻睡眠检查 (30 秒无 RF 活动)
    if (rf_idle > LIGHT_SLEEP_MS) {
        enter_mode(HAL_PWR_MODE_LIGHT_SLEEP);
        return;
    }
    
    // IDLE 检查 (时隙间隔)
    if (idle_time > IDLE_THRESHOLD_MS) {
        enter_mode(HAL_PWR_MODE_IDLE);
        return;
    }
    
    // 正常运行
    if (pwr.mode != HAL_PWR_MODE_ACTIVE) {
        enter_mode(HAL_PWR_MODE_ACTIVE);
    }
}

/*============================================================================
 * 功耗估算
 *============================================================================*/

uint16_t hal_power_get_current_ma(void)
{
    uint32_t total = pwr.active_ms + pwr.idle_ms + pwr.sleep_ms;
    if (total == 0) return 30;
    
    // 加权平均: ACTIVE=30mA, IDLE=15mA, SLEEP=5mA
    uint32_t energy = pwr.active_ms * 30 + pwr.idle_ms * 15 + pwr.sleep_ms * 5;
    return (uint16_t)(energy / total);
}

void hal_power_get_stats(uint32_t *active, uint32_t *idle, uint32_t *sleep)
{
    if (active) *active = pwr.active_ms;
    if (idle) *idle = pwr.idle_ms;
    if (sleep) *sleep = pwr.sleep_ms;
}

power_mode_t hal_power_get_mode(void)
{
    return pwr.mode;
}

/*============================================================================
 * 唤醒源配置
 *============================================================================*/

void hal_power_set_wakeup(uint8_t sources)
{
    pwr.wakeup_sources = sources;
}
