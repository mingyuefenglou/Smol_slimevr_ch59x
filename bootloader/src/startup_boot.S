/**
 * @file startup_boot.S
 * @brief Minimal Bootloader Startup for CH592
 */

    .section .init, "ax"
    .global _start
    .type _start, @function
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    /* Initialize stack */
    la sp, _estack

    /* Copy .data section */
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

    /* Call bootloader main */
    call bootloader_main

    /* Should never return, but if it does, infinite loop */
5:
    j 5b


    .section .vectors, "ax"
    .global _vectors
_vectors:
    .word _start            /* 0: Reset */
    .word 0                 /* 1: Reserved */
    .word NMI_Handler       /* 2: NMI */
    .word HardFault_Handler /* 3: HardFault */
    .word 0,0,0,0,0,0,0,0   /* 4-11: Reserved */
    .word 0                 /* 12: SysTick */
    .word 0                 /* 13: Reserved */
    .word 0                 /* 14: Software */
    .word 0                 /* 15: Reserved */
    /* External interrupts - minimal for bootloader */
    .word Default_Handler   /* 16-35 */
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler


    .section .text.Default_Handler, "ax"
    .weak NMI_Handler
    .weak HardFault_Handler
    .weak Default_Handler
    .type Default_Handler, @function
NMI_Handler:
HardFault_Handler:
Default_Handler:
1:
    j 1b

