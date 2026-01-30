/**
 * @file hal_charging.c
 * @brief 充电状态检测 (简化版)
 * 
 * Charging Status Detection (Simplified)
 * 
 * 硬件说明 / Hardware Note:
 * - 充电 LED 由 TP4054 CHRG 引脚直接控制
 * - CH591D 仅通过 PA10 (CHRG) 检测充电状态
 * - 不需要 MCU 控制 LED
 * 
 * TP4054 CHRG 引脚:
 * - 充电中: 拉低 (LED 亮)
 * - 充满/空闲: 高阻态 (LED 灭)
 */

#include "hal.h"
#include "board.h"

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * 配置 / Configuration
 *============================================================================*/

#ifndef PIN_CHRG_DET
#define PIN_CHRG_DET        5       // PA5 - 连接 TP4054 CHRG
#endif

#ifndef PIN_USB_VBUS
#define PIN_USB_VBUS        10      // PA10 - USB VBUS 检测 (可选)
#endif

/*============================================================================
 * 状态 / State
 *============================================================================*/

static struct {
    uint8_t charging;           // 正在充电
    uint8_t usb_connected;      // USB 已连接
    uint32_t last_update;       // 上次更新时间
    uint32_t charge_start_time; // 充电开始时间
} chrg_state;

/*============================================================================
 * 实现 / Implementation
 *============================================================================*/

/**
 * @brief 初始化充电检测
 */
void hal_charging_init(void)
{
#ifdef CH59X
    // CHRG: 输入, 内部上拉 (检测 TP4054 CHRG)
    hal_gpio_config(PIN_CHRG_DET, HAL_GPIO_INPUT_PULLUP);

    // USB VBUS: 默认下拉检测，如果复用 CHRG 则保持上拉
    if (PIN_USB_VBUS == PIN_CHRG_DET) {
        hal_gpio_config(PIN_USB_VBUS, HAL_GPIO_INPUT_PULLUP);
    } else {
        hal_gpio_config(PIN_USB_VBUS, HAL_GPIO_INPUT_PULLDOWN);
    }
#endif
    
    chrg_state.charging = 0;
    chrg_state.usb_connected = 0;
    chrg_state.last_update = 0;
    chrg_state.charge_start_time = 0;
}

/**
 * @brief 读取充电状态
 * @return 1=正在充电, 0=未充电
 */
uint8_t hal_charging_is_charging(void)
{
#ifdef CH59X
    // TP4054 CHRG 引脚: 充电时拉低
    return (hal_gpio_read(PIN_CHRG_DET) == 0) ? 1 : 0;
#else
    return 0;
#endif
}

/**
 * @brief 检测 USB 是否连接
 */
uint8_t hal_charging_usb_connected(void)
{
#ifdef CH59X
    // USB VBUS 检测 (5V 存在)
    return (hal_gpio_read(PIN_USB_VBUS) != 0) ? 1 : 0;
#else
    return 0;
#endif
}

/**
 * @brief 更新充电状态 (从主循环调用)
 */
void hal_charging_update(void)
{
    uint32_t now = hal_millis();
    
    // 每 100ms 更新一次
    if ((now - chrg_state.last_update) < 100) {
        return;
    }
    chrg_state.last_update = now;
    
    // 读取状态
    uint8_t usb = hal_charging_usb_connected();
    uint8_t chrg = hal_charging_is_charging();
    
    chrg_state.usb_connected = usb;
    
    // 充电状态变化检测
    if (chrg && !chrg_state.charging) {
        // 开始充电
        chrg_state.charge_start_time = now;
    }
    
    chrg_state.charging = chrg;
}

/**
 * @brief 获取充电状态
 */
void hal_charging_get_status(uint8_t *charging, uint8_t *usb_connected)
{
    if (charging) *charging = chrg_state.charging;
    if (usb_connected) *usb_connected = chrg_state.usb_connected;
}

/**
 * @brief 获取充电时长 (毫秒)
 */
uint32_t hal_charging_get_duration(void)
{
    if (!chrg_state.charging) return 0;
    return hal_millis() - chrg_state.charge_start_time;
}

/**
 * @brief 估算电池百分比 (简单线性)
 * @param voltage_mv 电池电压 (mV)
 * @return 0-100%
 */
uint8_t hal_battery_percent(uint16_t voltage_mv)
{
    // 锂电池电压范围: 3.3V (0%) - 4.2V (100%)
    // 使用分段线性更精确
    if (voltage_mv >= 4200) return 100;
    if (voltage_mv >= 4100) return 90 + (voltage_mv - 4100) / 10;
    if (voltage_mv >= 4000) return 75 + (voltage_mv - 4000) / 7;
    if (voltage_mv >= 3800) return 40 + (voltage_mv - 3800) / 6;
    if (voltage_mv >= 3600) return 15 + (voltage_mv - 3600) / 8;
    if (voltage_mv >= 3300) return (voltage_mv - 3300) / 20;
    return 0;
}
