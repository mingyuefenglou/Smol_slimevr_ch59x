/**
 * @file CH59x_timer.c
 * @brief CH59X Timer Driver Implementation
 */

#include "CH59x_common.h"

/*============================================================================
 * Timer Register Structure
 *============================================================================*/

typedef struct {
    __IO uint32_t CTRL;
    __IO uint32_t CNT;
    __IO uint32_t END;
    __IO uint32_t FIFO;
    __IO uint32_t INT_EN;
    __IO uint32_t INT_FLAG;
    __IO uint32_t DMA_NOW;
    __IO uint32_t DMA_BEG;
    __IO uint32_t DMA_END;
} TMR_TypeDef;

#define TMR0    ((TMR_TypeDef *)TMR0_BASE)
#define TMR1    ((TMR_TypeDef *)TMR1_BASE)
#define TMR2    ((TMR_TypeDef *)TMR2_BASE)
#define TMR3    ((TMR_TypeDef *)TMR3_BASE)

/*============================================================================
 * TMR0 Implementation
 *============================================================================*/

void TMR0_TimerInit(uint32_t period)
{
    TMR0->END = period;
    TMR0->CNT = 0;
    TMR0->CTRL = 0x01;  // Enable timer, free-run mode
}

void TMR0_ITCfg(uint8_t state, uint8_t it_flag)
{
    if (state == ENABLE) {
        TMR0->INT_EN |= it_flag;
    } else {
        TMR0->INT_EN &= ~it_flag;
    }
}

uint8_t TMR0_GetITFlag(uint8_t it_flag)
{
    return (TMR0->INT_FLAG & it_flag) ? 1 : 0;
}

void TMR0_ClearITFlag(uint8_t it_flag)
{
    TMR0->INT_FLAG = it_flag;
}

uint32_t TMR0_GetCurrentCount(void)
{
    return TMR0->CNT;
}

void TMR0_SetPeriod(uint32_t period)
{
    TMR0->END = period;
}

void TMR0_Enable(uint8_t state)
{
    if (state == ENABLE) {
        TMR0->CTRL |= 0x01;
    } else {
        TMR0->CTRL &= ~0x01;
    }
}

/*============================================================================
 * TMR1 Implementation
 *============================================================================*/

void TMR1_TimerInit(uint32_t period)
{
    TMR1->END = period;
    TMR1->CNT = 0;
    TMR1->CTRL = 0x01;
}

void TMR1_ITCfg(uint8_t state, uint8_t it_flag)
{
    if (state == ENABLE) {
        TMR1->INT_EN |= it_flag;
    } else {
        TMR1->INT_EN &= ~it_flag;
    }
}

uint8_t TMR1_GetITFlag(uint8_t it_flag)
{
    return (TMR1->INT_FLAG & it_flag) ? 1 : 0;
}

void TMR1_ClearITFlag(uint8_t it_flag)
{
    TMR1->INT_FLAG = it_flag;
}

uint32_t TMR1_GetCurrentCount(void)
{
    return TMR1->CNT;
}

void TMR1_SetPeriod(uint32_t period)
{
    TMR1->END = period;
}

void TMR1_Enable(uint8_t state)
{
    if (state == ENABLE) {
        TMR1->CTRL |= 0x01;
    } else {
        TMR1->CTRL &= ~0x01;
    }
}

/*============================================================================
 * TMR2/TMR3 Implementation (Similar)
 *============================================================================*/

void TMR2_TimerInit(uint32_t period) { TMR2->END = period; TMR2->CNT = 0; TMR2->CTRL = 0x01; }
void TMR2_ITCfg(uint8_t state, uint8_t it_flag) { if (state) TMR2->INT_EN |= it_flag; else TMR2->INT_EN &= ~it_flag; }
uint8_t TMR2_GetITFlag(uint8_t it_flag) { return (TMR2->INT_FLAG & it_flag) ? 1 : 0; }
void TMR2_ClearITFlag(uint8_t it_flag) { TMR2->INT_FLAG = it_flag; }
uint32_t TMR2_GetCurrentCount(void) { return TMR2->CNT; }
void TMR2_SetPeriod(uint32_t period) { TMR2->END = period; }
void TMR2_Enable(uint8_t state) { if (state) TMR2->CTRL |= 0x01; else TMR2->CTRL &= ~0x01; }

void TMR3_TimerInit(uint32_t period) { TMR3->END = period; TMR3->CNT = 0; TMR3->CTRL = 0x01; }
void TMR3_ITCfg(uint8_t state, uint8_t it_flag) { if (state) TMR3->INT_EN |= it_flag; else TMR3->INT_EN &= ~it_flag; }
uint8_t TMR3_GetITFlag(uint8_t it_flag) { return (TMR3->INT_FLAG & it_flag) ? 1 : 0; }
void TMR3_ClearITFlag(uint8_t it_flag) { TMR3->INT_FLAG = it_flag; }
uint32_t TMR3_GetCurrentCount(void) { return TMR3->CNT; }
void TMR3_SetPeriod(uint32_t period) { TMR3->END = period; }
void TMR3_Enable(uint8_t state) { if (state) TMR3->CTRL |= 0x01; else TMR3->CTRL &= ~0x01; }

/*============================================================================
 * PWM Functions
 *============================================================================*/

void PWM_SetPWM(uint8_t ch, uint32_t period, uint32_t duty)
{
    // Configure PWM output on timer
    (void)ch;
    (void)period;
    (void)duty;
}
