/**
 * @file CH59x_timer.h
 * @brief CH59X Timer Driver Header
 */

#ifndef __CH59X_TIMER_H__
#define __CH59X_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * Timer Interrupt Definitions
 *============================================================================*/

#define TMR0_IT_CYC_END     (1 << 0)    // Cycle end interrupt
#define TMR0_IT_DATA_ACT    (1 << 1)    // Data active interrupt
#define TMR0_IT_FIFO_HF     (1 << 2)    // FIFO half full interrupt

#define TMR1_IT_CYC_END     (1 << 0)
#define TMR1_IT_DATA_ACT    (1 << 1)
#define TMR1_IT_FIFO_HF     (1 << 2)

#define TMR2_IT_CYC_END     (1 << 0)
#define TMR2_IT_DATA_ACT    (1 << 1)
#define TMR2_IT_FIFO_HF     (1 << 2)

#define TMR3_IT_CYC_END     (1 << 0)
#define TMR3_IT_DATA_ACT    (1 << 1)
#define TMR3_IT_FIFO_HF     (1 << 2)

/*============================================================================
 * Timer Mode Definitions
 *============================================================================*/

typedef enum {
    TMR_Mode_FreeRun,       // Free running mode
    TMR_Mode_Count,         // Counter mode
    TMR_Mode_PWM,           // PWM mode
    TMR_Mode_Cap,           // Capture mode
} TMR_ModeTypeDef;

/*============================================================================
 * TMR0 Functions
 *============================================================================*/

/**
 * @brief Initialize TMR0 in timer mode
 * @param period Timer period in system clocks
 */
void TMR0_TimerInit(uint32_t period);

/**
 * @brief Configure TMR0 interrupt
 * @param state ENABLE or DISABLE
 * @param it_flag Interrupt flags (TMR0_IT_xxx)
 */
void TMR0_ITCfg(uint8_t state, uint8_t it_flag);

/**
 * @brief Get TMR0 interrupt flag
 * @param it_flag Interrupt flag to check
 * @return Non-zero if flag is set
 */
uint8_t TMR0_GetITFlag(uint8_t it_flag);

/**
 * @brief Clear TMR0 interrupt flag
 * @param it_flag Interrupt flag to clear
 */
void TMR0_ClearITFlag(uint8_t it_flag);

/**
 * @brief Get TMR0 current count value
 * @return Current count value
 */
uint32_t TMR0_GetCurrentCount(void);

/**
 * @brief Set TMR0 period
 * @param period New period value
 */
void TMR0_SetPeriod(uint32_t period);

/**
 * @brief Enable/Disable TMR0
 * @param state ENABLE or DISABLE
 */
void TMR0_Enable(uint8_t state);

/*============================================================================
 * TMR1 Functions
 *============================================================================*/

void TMR1_TimerInit(uint32_t period);
void TMR1_ITCfg(uint8_t state, uint8_t it_flag);
uint8_t TMR1_GetITFlag(uint8_t it_flag);
void TMR1_ClearITFlag(uint8_t it_flag);
uint32_t TMR1_GetCurrentCount(void);
void TMR1_SetPeriod(uint32_t period);
void TMR1_Enable(uint8_t state);

/*============================================================================
 * TMR2 Functions
 *============================================================================*/

void TMR2_TimerInit(uint32_t period);
void TMR2_ITCfg(uint8_t state, uint8_t it_flag);
uint8_t TMR2_GetITFlag(uint8_t it_flag);
void TMR2_ClearITFlag(uint8_t it_flag);
uint32_t TMR2_GetCurrentCount(void);
void TMR2_SetPeriod(uint32_t period);
void TMR2_Enable(uint8_t state);

/*============================================================================
 * TMR3 Functions
 *============================================================================*/

void TMR3_TimerInit(uint32_t period);
void TMR3_ITCfg(uint8_t state, uint8_t it_flag);
uint8_t TMR3_GetITFlag(uint8_t it_flag);
void TMR3_ClearITFlag(uint8_t it_flag);
uint32_t TMR3_GetCurrentCount(void);
void TMR3_SetPeriod(uint32_t period);
void TMR3_Enable(uint8_t state);

/*============================================================================
 * PWM Functions
 *============================================================================*/

/**
 * @brief Configure PWM output
 * @param ch PWM channel (0-3)
 * @param period PWM period
 * @param duty Duty cycle count
 */
void PWM_SetPWM(uint8_t ch, uint32_t period, uint32_t duty);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_TIMER_H__ */
