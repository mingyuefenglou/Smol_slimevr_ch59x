/**
 * @file CH59x_sys.h
 * @brief CH59X System Functions Header
 */

#ifndef __CH59X_SYS_H__
#define __CH59X_SYS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * Power Management
 *============================================================================*/

typedef enum {
    PWR_MODE_ACTIVE,        // Active mode
    PWR_MODE_IDLE,          // Idle mode
    PWR_MODE_HALT,          // Halt mode
    PWR_MODE_SLEEP,         // Sleep mode
    PWR_MODE_SHUTDOWN,      // Shutdown mode
} PWR_ModeTypeDef;

/**
 * @brief Configure power mode
 * @param mode Power mode to enter
 */
void PWR_EnterMode(PWR_ModeTypeDef mode);

/**
 * @brief Configure wakeup source
 * @param src Wakeup source bits
 */
void PWR_WakeUpCfg(uint32_t src);

/*============================================================================
 * Reset Functions
 *============================================================================*/

/**
 * @brief Software reset
 */
void SYS_ResetExecute(void);

/**
 * @brief Get reset reason
 * @return Reset reason flags
 */
uint8_t SYS_GetResetFlag(void);

/**
 * @brief Clear reset flags
 */
void SYS_ClearResetFlag(void);

/*============================================================================
 * Watchdog Functions
 *============================================================================*/

/**
 * @brief Configure watchdog timer
 * @param period Watchdog period in ms
 */
void WDOG_SetPeriod(uint32_t period);

/**
 * @brief Feed watchdog
 */
void WDOG_Feed(void);

/**
 * @brief Enable/Disable watchdog
 * @param state ENABLE or DISABLE
 */
void WDOG_Enable(uint8_t state);

/*============================================================================
 * RTC Functions
 *============================================================================*/

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} RTC_TimeTypeDef;

void RTC_Init(void);
void RTC_SetTime(RTC_TimeTypeDef *time);
void RTC_GetTime(RTC_TimeTypeDef *time);
void RTC_SetAlarm(uint32_t seconds);
uint8_t RTC_GetAlarmFlag(void);
void RTC_ClearAlarmFlag(void);

/*============================================================================
 * Low Power Functions
 *============================================================================*/

/**
 * @brief Enter low power mode with RAM retention
 */
void LowPower_Idle(void);

/**
 * @brief Enter deep sleep mode
 * @param wakeup_src Wakeup source configuration
 */
void LowPower_Halt(uint32_t wakeup_src);

/**
 * @brief Enter shutdown mode (lowest power)
 * @param wakeup_pin GPIO pin for wakeup
 */
void LowPower_Shutdown(uint8_t wakeup_pin);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_SYS_H__ */
