/**
 * @file core_riscv.c
 * @brief RISC-V Core Functions for CH592
 */

#include "core_riscv.h"

// PFIC (Programmable Fast Interrupt Controller) registers
#define PFIC_BASE       0xE000E000
#define PFIC_IENR1      (*((volatile uint32_t*)(PFIC_BASE + 0x100)))
#define PFIC_IENR2      (*((volatile uint32_t*)(PFIC_BASE + 0x104)))
#define PFIC_IRER1      (*((volatile uint32_t*)(PFIC_BASE + 0x180)))
#define PFIC_IRER2      (*((volatile uint32_t*)(PFIC_BASE + 0x184)))
#define PFIC_IPSR1      (*((volatile uint32_t*)(PFIC_BASE + 0x200)))
#define PFIC_IPSR2      (*((volatile uint32_t*)(PFIC_BASE + 0x204)))
#define PFIC_IPRR1      (*((volatile uint32_t*)(PFIC_BASE + 0x280)))
#define PFIC_IPRR2      (*((volatile uint32_t*)(PFIC_BASE + 0x284)))
#define PFIC_IACTR1     (*((volatile uint32_t*)(PFIC_BASE + 0x300)))
#define PFIC_IACTR2     (*((volatile uint32_t*)(PFIC_BASE + 0x304)))
#define PFIC_IPRIOR     ((volatile uint8_t*)(PFIC_BASE + 0x400))
#define PFIC_SCTLR      (*((volatile uint32_t*)(PFIC_BASE + 0xD10)))

// SysTick registers
#define STK_BASE        0xE000F000
#define STK_CTLR        (*((volatile uint32_t*)(STK_BASE + 0x00)))
#define STK_SR          (*((volatile uint32_t*)(STK_BASE + 0x04)))
#define STK_CNTL        (*((volatile uint32_t*)(STK_BASE + 0x08)))
#define STK_CNTH        (*((volatile uint32_t*)(STK_BASE + 0x0C)))
#define STK_CMPLR       (*((volatile uint32_t*)(STK_BASE + 0x10)))
#define STK_CMPHR       (*((volatile uint32_t*)(STK_BASE + 0x14)))

void PFIC_EnableIRQ(IRQn_Type IRQn)
{
    if ((int32_t)IRQn >= 0) {
        if (IRQn < 32) {
            PFIC_IENR1 = (1 << IRQn);
        } else {
            PFIC_IENR2 = (1 << (IRQn - 32));
        }
    }
}

void PFIC_DisableIRQ(IRQn_Type IRQn)
{
    if ((int32_t)IRQn >= 0) {
        if (IRQn < 32) {
            PFIC_IRER1 = (1 << IRQn);
        } else {
            PFIC_IRER2 = (1 << (IRQn - 32));
        }
    }
}

void PFIC_SetPriority(IRQn_Type IRQn, uint8_t priority)
{
    if ((int32_t)IRQn >= 0) {
        PFIC_IPRIOR[IRQn] = priority << 4;
    }
}

void SysTick_Config(uint32_t ticks)
{
    // Set compare value
    STK_CMPLR = ticks - 1;
    STK_CMPHR = 0;
    
    // Clear counter
    STK_CNTL = 0;
    STK_CNTH = 0;
    
    // Clear status
    STK_SR = 0;
    
    // Enable: STRE=1, STIE=1, STE=1
    STK_CTLR = 0x07;
    
    // Enable SysTick interrupt
    PFIC_EnableIRQ(SysTick_IRQn);
}

// Default exception handler
__attribute__((weak))
void Default_Handler(void)
{
    while(1);
}

// Trap handler (called on exceptions)
__attribute__((weak))
void Trap_Handler(void)
{
    while(1);
}
