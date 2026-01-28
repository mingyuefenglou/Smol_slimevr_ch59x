/**
 * @file Link_boot.ld
 * @brief CH592 Bootloader Linker Script
 * 
 * Memory layout:
 * - Bootloader: 4KB (0x00000000 - 0x00000FFF)
 * - Application: starts at 0x00001000
 */

ENTRY(_start)

MEMORY
{
    /* Bootloader region only */
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 26K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 0x200;  /* 512 bytes stack for bootloader */

SECTIONS
{
    /* Vector table at 0x0000 */
    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors))
        . = ALIGN(4);
    } >FLASH

    /* Init and startup code */
    .init :
    {
        . = ALIGN(4);
        KEEP(*(.init))
        . = ALIGN(4);
    } >FLASH

    /* Program code */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* Read-only data */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
    } >FLASH

    /* Global pointer */
    . = ALIGN(4);
    PROVIDE(__global_pointer$ = . + 0x800);

    /* Initialized data */
    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT>FLASH

    /* Uninitialized data */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    /* Stack */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + _Min_Stack_Size;
        PROVIDE(_stack = .);
    } >RAM

    /* Size check - must fit in 4KB */
    ASSERT((_etext + (_edata - _sdata)) <= 4096, "Bootloader exceeds 4KB!")

    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.comment)
        *(.note.*)
    }
}
