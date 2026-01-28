/**
 * @file startup_CH592.S
 * @brief CH592 RISC-V Startup Code
 */

    .section .init, "ax"
    .global _start
    .type _start, @function
_start:
    .option push
    .option norelax

    /* Initialize global pointer */
    la gp, __global_pointer$
    
    .option pop

    /* Initialize stack pointer */
    la sp, _estack

    /* Initialize .data section */
    la a0, _sidata
    la a1, _sdata
    la a2, _edata
    bgeu a1, a2, 2f
1:
    lw t0, (a0)
    sw t0, (a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b
2:

    /* Clear .bss section */
    la a0, _sbss
    la a1, _ebss
    bgeu a0, a1, 4f
3:
    sw zero, (a0)
    addi a0, a0, 4
    bltu a0, a1, 3b
4:

    /* Call SystemInit */
    call SystemInit

    /* Call static constructors */
    la a0, __libc_init_array
    beqz a0, 5f
    call __libc_init_array
5:

    /* Call main */
    call main

    /* Infinite loop if main returns */
6:
    j 6b


    .section .vectors, "ax"
    .global _vectors
_vectors:
    .word _start            /* 0: Reset */
    .word 0                 /* 1: Reserved */
    .word NMI_Handler       /* 2: NMI */
    .word HardFault_Handler /* 3: HardFault */
    .word 0                 /* 4-11: Reserved */
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word SysTick_Handler   /* 12: SysTick */
    .word 0                 /* 13: Reserved */
    .word SW_Handler        /* 14: Software */
    .word 0                 /* 15: Reserved */
    .word TMR0_IRQHandler   /* 16: TMR0 */
    .word GPIOA_IRQHandler  /* 17: GPIOA */
    .word GPIOB_IRQHandler  /* 18: GPIOB */
    .word SPI0_IRQHandler   /* 19: SPI0 */
    .word SPI1_IRQHandler   /* 20: SPI1 */
    .word UART0_IRQHandler  /* 21: UART0 */
    .word UART1_IRQHandler  /* 22: UART1 */
    .word UART2_IRQHandler  /* 23: UART2 */
    .word UART3_IRQHandler  /* 24: UART3 */
    .word I2C_IRQHandler    /* 25: I2C */
    .word TMR1_IRQHandler   /* 26: TMR1 */
    .word TMR2_IRQHandler   /* 27: TMR2 */
    .word TMR3_IRQHandler   /* 28: TMR3 */
    .word ADC_IRQHandler    /* 29: ADC */
    .word USB_IRQHandler    /* 30: USB */
    .word LCD_IRQHandler    /* 31: LCD */
    .word BLE_IRQHandler    /* 32: BLE */
    .word WDOG_IRQHandler   /* 33: Watchdog */
    .word RTC_IRQHandler    /* 34: RTC */
    .word PWM_IRQHandler    /* 35: PWM */


    /* Weak default handlers */
    .weak NMI_Handler
    .weak HardFault_Handler
    .weak SysTick_Handler
    .weak SW_Handler
    .weak TMR0_IRQHandler
    .weak TMR1_IRQHandler
    .weak TMR2_IRQHandler
    .weak TMR3_IRQHandler
    .weak GPIOA_IRQHandler
    .weak GPIOB_IRQHandler
    .weak SPI0_IRQHandler
    .weak SPI1_IRQHandler
    .weak UART0_IRQHandler
    .weak UART1_IRQHandler
    .weak UART2_IRQHandler
    .weak UART3_IRQHandler
    .weak I2C_IRQHandler
    .weak ADC_IRQHandler
    .weak USB_IRQHandler
    .weak LCD_IRQHandler
    .weak BLE_IRQHandler
    .weak WDOG_IRQHandler
    .weak RTC_IRQHandler
    .weak PWM_IRQHandler

    .section .text.Default_Handler, "ax"
    .global Default_Handler
    .type Default_Handler, @function
Default_Handler:
1:
    j 1b

NMI_Handler         = Default_Handler
HardFault_Handler   = Default_Handler
SysTick_Handler     = Default_Handler
SW_Handler          = Default_Handler
TMR0_IRQHandler     = Default_Handler
TMR1_IRQHandler     = Default_Handler
TMR2_IRQHandler     = Default_Handler
TMR3_IRQHandler     = Default_Handler
GPIOA_IRQHandler    = Default_Handler
GPIOB_IRQHandler    = Default_Handler
SPI0_IRQHandler     = Default_Handler
SPI1_IRQHandler     = Default_Handler
UART0_IRQHandler    = Default_Handler
UART1_IRQHandler    = Default_Handler
UART2_IRQHandler    = Default_Handler
UART3_IRQHandler    = Default_Handler
I2C_IRQHandler      = Default_Handler
ADC_IRQHandler      = Default_Handler
USB_IRQHandler      = Default_Handler
LCD_IRQHandler      = Default_Handler
BLE_IRQHandler      = Default_Handler
WDOG_IRQHandler     = Default_Handler
RTC_IRQHandler      = Default_Handler
PWM_IRQHandler      = Default_Handler


    .section .text.SystemInit, "ax"
    .weak SystemInit
    .type SystemInit, @function
SystemInit:
    ret


    .section .text.__libc_init_array, "ax"
    .weak __libc_init_array
    .type __libc_init_array, @function
__libc_init_array:
    ret
