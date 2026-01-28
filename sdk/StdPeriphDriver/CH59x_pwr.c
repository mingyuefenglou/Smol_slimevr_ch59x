/**
 * @file CH59x_pwr.c
 * @brief CH592 Power Management (Stub)
 */

#include "CH59x_common.h"

__attribute__((weak))
void PWR_DCDCCfg(uint8_t enable)
{
    (void)enable;
    // Configure DC-DC converter
}

__attribute__((weak))
void PWR_UnitModCfg(uint8_t enable, uint8_t unit)
{
    (void)enable;
    (void)unit;
    // Configure power unit modules
}

__attribute__((weak))
void LowPower_Idle(void)
{
    __WFI();
}

__attribute__((weak))
void LowPower_Halt(uint32_t wakeup_src)
{
    (void)wakeup_src;  // 忽略参数，简化实现
    __WFI();
}

__attribute__((weak))
void LowPower_Sleep(uint8_t rm)
{
    (void)rm;
    __WFI();
}

__attribute__((weak))
void LowPower_Shutdown(uint8_t rm)
{
    (void)rm;
    while(1) __WFI();
}
