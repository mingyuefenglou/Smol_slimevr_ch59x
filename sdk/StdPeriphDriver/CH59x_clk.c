/**
 * @file CH59x_clk.c
 * @brief CH59X Clock and System Implementation
 */

#include "CH59x_common.h"

/*============================================================================
 * System Registers
 *============================================================================*/

typedef struct {
    __IO uint32_t CLK_CFG;
    __IO uint32_t CLK_DIV;
    __IO uint32_t PLL_CFG;
    __IO uint32_t PWR_CFG;
    __IO uint32_t RST_CFG;
    __IO uint32_t PERI_CLK;
} SYS_TypeDef;

#define SYS     ((SYS_TypeDef *)SYS_BASE)

static uint32_t SystemCoreClock = 60000000UL;

/*============================================================================
 * Clock Functions
 *============================================================================*/

void SetSysClock(CLK_SOURCE_TypeDef clk)
{
    switch (clk) {
        case CLK_SOURCE_LSI:
            SystemCoreClock = 32000;
            break;
        case CLK_SOURCE_LSE:
            SystemCoreClock = 32768;
            break;
        case CLK_SOURCE_HSI:
            SystemCoreClock = 32000000;
            break;
        case CLK_SOURCE_PLL_32MHz:
            SystemCoreClock = 32000000;
            break;
        case CLK_SOURCE_PLL_48MHz:
            SystemCoreClock = 48000000;
            break;
        case CLK_SOURCE_PLL_60MHz:
        default:
            SystemCoreClock = 60000000;
            break;
    }
    
    // Configure PLL and clock source
    // Actual implementation would configure hardware registers
    SYS->CLK_CFG = (uint32_t)clk;
}

uint32_t GetSysClock(void)
{
    return SystemCoreClock;
}

void PWR_PeriphClkCfg(FunctionalState state, uint32_t periph)
{
    if (state == ENABLE) {
        SYS->PERI_CLK |= periph;
    } else {
        SYS->PERI_CLK &= ~periph;
    }
}

void PWR_PeriphWakeUpCfg(FunctionalState state, uint32_t periph)
{
    // Configure wakeup source
    (void)state;
    (void)periph;
}

void SYS_ResetExecute(void)
{
    // Trigger software reset
    SYS->RST_CFG = 0x5AA5;
    while (1);  // Wait for reset
}

/*============================================================================
 * Delay Functions
 *============================================================================*/

void mDelaymS(uint32_t ms)
{
    uint32_t cycles = (SystemCoreClock / 1000) * ms;
    volatile uint32_t count = cycles / 4;  // Approximate cycles per loop
    while (count--) {
        __NOP();
    }
}

/*============================================================================
 * Watchdog Functions
 *============================================================================*/

// Watchdog registers (approximate addresses)
#define R32_WDOG_CFG      (*((volatile uint32_t*)(0x40001000)))
#define R32_WDOG_CNT      (*((volatile uint32_t*)(0x40001004)))
#define R8_WDOG_CTRL      (*((volatile uint8_t*)(0x40001008)))

#define RB_WDOG_ENABLE    0x01
#define RB_WDOG_RESET     0x02

__attribute__((weak))
void WDOG_SetPeriod(uint32_t period)
{
    // Set watchdog period (in units of 32kHz clock cycles)
    R32_WDOG_CFG = period;
}

__attribute__((weak))
void WDOG_Enable(uint8_t state)
{
    if (state == ENABLE) {
        R8_WDOG_CTRL |= RB_WDOG_ENABLE;
    } else {
        R8_WDOG_CTRL &= ~RB_WDOG_ENABLE;
    }
}

__attribute__((weak))
void WDOG_Feed(void)
{
    // Feed watchdog by writing to counter register
    R32_WDOG_CNT = 0;
}

void mDelayuS(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000) * us;
    volatile uint32_t count = cycles / 4;
    while (count--) {
        __NOP();
    }
}

/*============================================================================
 * PFIC (Interrupt Controller)
 *============================================================================*/

typedef struct {
    __IO uint32_t ISR[8];
    __IO uint32_t IPR[8];
    __IO uint32_t ITHRESDR;
    __IO uint32_t RESERVED0;
    __IO uint32_t CFGR;
    __IO uint32_t GISR;
    __IO uint32_t RESERVED1[4];
    __IO uint32_t FIFOADDRR0;
    __IO uint32_t FIFOADDRR1;
    __IO uint32_t FIFOADDRR2;
    __IO uint32_t FIFOADDRR3;
    __IO uint32_t RESERVED2[12];
    __IO uint32_t IENR[8];
    __IO uint32_t RESERVED3[8];
    __IO uint32_t IRER[8];
    __IO uint32_t RESERVED4[8];
    __IO uint32_t IPSR[8];
    __IO uint32_t RESERVED5[8];
    __IO uint32_t IPRR[8];
    __IO uint32_t RESERVED6[8];
    __IO uint32_t IACTR[8];
} PFIC_TypeDef;

#define PFIC    ((PFIC_TypeDef *)0xE000E000)

// PFIC functions are defined in core_riscv.c
// Use extern declarations here
extern void PFIC_EnableIRQ(IRQn_Type irq);
extern void PFIC_DisableIRQ(IRQn_Type irq);
extern void PFIC_SetPriority(IRQn_Type irq, uint8_t priority);
