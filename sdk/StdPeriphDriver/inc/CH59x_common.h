/**
 * @file CH59x_common.h
 * @brief CH59X Common Definitions
 * 
 * This file provides the core definitions for CH59X microcontrollers.
 * Based on WCH official SDK structure.
 */

#ifndef __CH59X_COMMON_H__
#define __CH59X_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*============================================================================
 * Device Selection
 *============================================================================*/

#if !defined(CH591) && !defined(CH592)
#define CH592   // Default to CH592
#endif

/*============================================================================
 * Core Definitions
 *============================================================================*/

// RISC-V specific
#define __I     volatile const
#define __O     volatile
#define __IO    volatile

// Interrupt macros
#define __INTERRUPT     __attribute__((interrupt("machine")))
#define __HIGH_CODE     __attribute__((section(".highcode")))
#define __WEAK          __attribute__((weak))

// NOP instruction
#define __NOP()         __asm volatile ("nop")
#define __WFI()         __asm volatile ("wfi")
#define __WFE()         __asm volatile ("wfi")  // CH59x uses WFI

// Memory barrier
#define __DMB()         __asm volatile ("fence")
#define __DSB()         __asm volatile ("fence")
#define __ISB()         __asm volatile ("fence.i")

/*============================================================================
 * Bit Definitions
 *============================================================================*/

#define BIT0            (1 << 0)
#define BIT1            (1 << 1)
#define BIT2            (1 << 2)
#define BIT3            (1 << 3)
#define BIT4            (1 << 4)
#define BIT5            (1 << 5)
#define BIT6            (1 << 6)
#define BIT7            (1 << 7)
#define BIT8            (1 << 8)
#define BIT9            (1 << 9)
#define BIT10           (1 << 10)
#define BIT11           (1 << 11)
#define BIT12           (1 << 12)
#define BIT13           (1 << 13)
#define BIT14           (1 << 14)
#define BIT15           (1 << 15)

/*============================================================================
 * Boolean Type
 *============================================================================*/

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

#ifndef NULL
#define NULL            ((void *)0)
#endif

typedef enum {
    DISABLE = 0,
    ENABLE = !DISABLE
} FunctionalState;

typedef enum {
    RESET = 0,
    SET = !RESET
} FlagStatus, ITStatus;

typedef enum {
    ERROR = 0,
    SUCCESS = !ERROR
} ErrorStatus;

/*============================================================================
 * Memory Map
 *============================================================================*/

#define FLASH_BASE          0x00000000
#define RAM_BASE            0x20000000
#define PERIPH_BASE         0x40000000

// Peripheral base addresses
#define SYS_BASE            (PERIPH_BASE + 0x00001000)
#define TMR0_BASE           (PERIPH_BASE + 0x00002000)
#define TMR1_BASE           (PERIPH_BASE + 0x00002400)
#define TMR2_BASE           (PERIPH_BASE + 0x00002800)
#define TMR3_BASE           (PERIPH_BASE + 0x00002C00)
#define UART0_BASE          (PERIPH_BASE + 0x00003000)
#define UART1_BASE          (PERIPH_BASE + 0x00003400)
#define UART2_BASE          (PERIPH_BASE + 0x00003800)
#define UART3_BASE          (PERIPH_BASE + 0x00003C00)
#define SPI0_BASE           (PERIPH_BASE + 0x00004000)
#define SPI1_BASE           (PERIPH_BASE + 0x00004400)
#define I2C_BASE            (PERIPH_BASE + 0x00005000)
#define GPIO_BASE           (PERIPH_BASE + 0x00006000)
#define ADC_BASE            (PERIPH_BASE + 0x00007000)
#define USB_BASE            (PERIPH_BASE + 0x00008000)
#define LCD_BASE            (PERIPH_BASE + 0x00009000)
#define FLASH_CTRL_BASE     (PERIPH_BASE + 0x0000A000)
#define PWM_BASE            (PERIPH_BASE + 0x0000B000)

/*============================================================================
 * Clock Frequencies
 *============================================================================*/

#define HSI_VALUE           32000000UL  // Internal RC oscillator
#define HSE_VALUE           32768UL     // External crystal (32.768kHz)
#define PLL_VALUE           480000000UL // PLL output

#define FREQ_SYS            GetSysClock()

/*============================================================================
 * Interrupt Numbers
 *============================================================================*/

typedef enum {
    // Core interrupts
    Reset_IRQn              = 1,
    NMI_IRQn                = 2,
    EXC_IRQn                = 3,
    SysTick_IRQn            = 12,
    Software_IRQn           = 14,
    
    // Peripheral interrupts
    TMR0_IRQn               = 16,
    GPIO_A_IRQn             = 17,
    GPIO_B_IRQn             = 18,
    SPI0_IRQn               = 19,
    SPI1_IRQn               = 20,
    UART0_IRQn              = 21,
    UART1_IRQn              = 22,
    UART2_IRQn              = 23,
    UART3_IRQn              = 24,
    I2C_IRQn                = 25,
    TMR1_IRQn               = 26,
    TMR2_IRQn               = 27,
    TMR3_IRQn               = 28,
    ADC_IRQn                = 29,
    USB_IRQn                = 30,
    LCD_IRQn                = 31,
    BLE_IRQn                = 32,
    WDOG_IRQn               = 33,
    RTC_IRQn                = 34,
    PWM_IRQn                = 35,
} IRQn_Type;

/*============================================================================
 * Peripheral Clock Enable Bits
 *============================================================================*/

#define BIT_PERI_TMR0       (1 << 0)
#define BIT_PERI_TMR1       (1 << 1)
#define BIT_PERI_TMR2       (1 << 2)
#define BIT_PERI_TMR3       (1 << 3)
#define BIT_PERI_UART0      (1 << 4)
#define BIT_PERI_UART1      (1 << 5)
#define BIT_PERI_UART2      (1 << 6)
#define BIT_PERI_UART3      (1 << 7)
#define BIT_PERI_SPI0       (1 << 8)
#define BIT_PERI_SPI1       (1 << 9)
#define BIT_PERI_I2C        (1 << 10)
#define BIT_PERI_GPIO_A     (1 << 11)
#define BIT_PERI_GPIO_B     (1 << 12)
#define BIT_PERI_ADC        (1 << 13)
#define BIT_PERI_USB        (1 << 14)
#define BIT_PERI_LCD        (1 << 15)
#define BIT_PERI_PWM        (1 << 16)

/*============================================================================
 * Clock Source Selection
 *============================================================================*/

typedef enum {
    CLK_SOURCE_LSI          = 0,    // Low-speed internal RC (32kHz)
    CLK_SOURCE_LSE          = 1,    // Low-speed external (32.768kHz)
    CLK_SOURCE_HSI          = 2,    // High-speed internal RC (32MHz)
    CLK_SOURCE_HSE          = 3,    // High-speed external
    CLK_SOURCE_PLL_32MHz    = 4,    // PLL output 32MHz
    CLK_SOURCE_PLL_48MHz    = 5,    // PLL output 48MHz
    CLK_SOURCE_PLL_60MHz    = 6,    // PLL output 60MHz
} CLK_SOURCE_TypeDef;

/*============================================================================
 * Function Prototypes
 *============================================================================*/

// System functions
void SetSysClock(CLK_SOURCE_TypeDef clk);
uint32_t GetSysClock(void);
void PWR_PeriphClkCfg(FunctionalState state, uint32_t periph);
void PWR_PeriphWakeUpCfg(FunctionalState state, uint32_t periph);
void SYS_ResetExecute(void);
void mDelaymS(uint32_t ms);
void mDelayuS(uint32_t us);

// PFIC (Platform-Level Interrupt Controller)
void PFIC_EnableIRQ(IRQn_Type irq);
void PFIC_DisableIRQ(IRQn_Type irq);
void PFIC_SetPriority(IRQn_Type irq, uint8_t priority);
uint8_t PFIC_GetPriority(IRQn_Type irq);

/*============================================================================
 * Include Peripheral Headers
 *============================================================================*/

#include "CH59x_gpio.h"
#include "CH59x_timer.h"
#include "CH59x_spi.h"
#include "CH59x_i2c.h"
#include "CH59x_uart.h"
#include "CH59x_adc.h"
#include "CH59x_flash.h"
#include "CH59x_sys.h"

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_COMMON_H__ */
