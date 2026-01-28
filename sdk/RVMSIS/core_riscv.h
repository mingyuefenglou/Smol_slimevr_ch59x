/**
 * @file core_riscv.h
 * @brief RISC-V Core Definitions for CH592
 * 
 * This file provides core RISC-V intrinsics and register access
 * for the CH592 QingKe V4C processor.
 */

#ifndef __CORE_RISCV_H__
#define __CORE_RISCV_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * RISC-V CSR Addresses
 *============================================================================*/

#define CSR_MSTATUS     0x300
#define CSR_MISA        0x301
#define CSR_MIE         0x304
#define CSR_MTVEC       0x305
#define CSR_MSCRATCH    0x340
#define CSR_MEPC        0x341
#define CSR_MCAUSE      0x342
#define CSR_MTVAL       0x343
#define CSR_MIP         0x344
#define CSR_MCYCLE      0xB00
#define CSR_MCYCLEH     0xB80
#define CSR_MINSTRET    0xB02
#define CSR_MINSTRETH   0xB82

// CH592 specific CSRs
#define CSR_GINTENR     0x800   // Global interrupt enable
#define CSR_INTSYSCR    0x804   // Interrupt system control

/*============================================================================
 * RISC-V CSR Access Macros
 *============================================================================*/

#define __CSR_READ(csr) ({                      \
    uint32_t __tmp;                             \
    __asm__ volatile ("csrr %0, " #csr : "=r"(__tmp)); \
    __tmp;                                      \
})

#define __CSR_WRITE(csr, val) ({                \
    __asm__ volatile ("csrw " #csr ", %0" :: "r"(val)); \
})

#define __CSR_SET(csr, val) ({                  \
    __asm__ volatile ("csrs " #csr ", %0" :: "r"(val)); \
})

#define __CSR_CLEAR(csr, val) ({                \
    __asm__ volatile ("csrc " #csr ", %0" :: "r"(val)); \
})

/*============================================================================
 * Core Intrinsics
 *============================================================================*/

// Wait for interrupt
static inline void __WFI(void)
{
    __asm__ volatile ("wfi");
}

// No operation
static inline void __NOP(void)
{
    __asm__ volatile ("nop");
}

// Enable global interrupts
static inline void __enable_irq(void)
{
    __asm__ volatile ("csrsi mstatus, 0x08");
}

// Disable global interrupts
static inline void __disable_irq(void)
{
    __asm__ volatile ("csrci mstatus, 0x08");
}

// Memory barrier
static inline void __DSB(void)
{
    __asm__ volatile ("fence");
}

static inline void __ISB(void)
{
    __asm__ volatile ("fence.i");
}

// Get cycle count
static inline uint32_t __get_MCYCLE(void)
{
    uint32_t result;
    __asm__ volatile ("csrr %0, mcycle" : "=r"(result));
    return result;
}

// Get 64-bit cycle count
static inline uint64_t __get_MCYCLE64(void)
{
    uint32_t hi1, hi2, lo;
    do {
        __asm__ volatile ("csrr %0, mcycleh" : "=r"(hi1));
        __asm__ volatile ("csrr %0, mcycle" : "=r"(lo));
        __asm__ volatile ("csrr %0, mcycleh" : "=r"(hi2));
    } while (hi1 != hi2);
    return ((uint64_t)hi1 << 32) | lo;
}

/*============================================================================
 * NVIC-like Interface for CH592
 *============================================================================*/

// Interrupt numbers
typedef enum {
    SysTick_IRQn     = 12,
    SWI_IRQn         = 14,
    
    TMR0_IRQn        = 16,
    GPIO_A_IRQn      = 17,
    GPIO_B_IRQn      = 18,
    SPI0_IRQn        = 19,
    BB_IRQn          = 20,
    LLE_IRQn         = 21,
    USB_IRQn         = 22,
    TMR1_IRQn        = 25,
    TMR2_IRQn        = 26,
    UART0_IRQn       = 27,
    UART1_IRQn       = 28,
    RTC_IRQn         = 29,
    ADC_IRQn         = 30,
    I2C_IRQn         = 31,
    PWMX_IRQn        = 32,
    TMR3_IRQn        = 33,
    UART2_IRQn       = 34,
    UART3_IRQn       = 35,
    WDOG_BAT_IRQn    = 36,
} IRQn_Type;

// Interrupt control
void PFIC_EnableIRQ(IRQn_Type IRQn);
void PFIC_DisableIRQ(IRQn_Type IRQn);
void PFIC_SetPriority(IRQn_Type IRQn, uint8_t priority);

// System tick
void SysTick_Config(uint32_t ticks);

/*============================================================================
 * CH592 Specific Macros
 *============================================================================*/

// High-code section (runs from RAM for speed)
#define __HIGH_CODE     __attribute__((section(".highcode")))

// Interrupt handler
#define __INTERRUPT     __attribute__((interrupt("machine")))

// Weak function
#define __WEAK          __attribute__((weak))

// Packed structure
#define __PACKED        __attribute__((packed))

// Aligned
#define __ALIGNED(x)    __attribute__((aligned(x)))

#ifdef __cplusplus
}
#endif

#endif /* __CORE_RISCV_H__ */
