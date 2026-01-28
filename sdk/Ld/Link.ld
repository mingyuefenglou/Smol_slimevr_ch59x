/**
 * @file Link.ld
 * @brief CH592 Linker Script (Optimized)
 * 
 * Memory layout:
 * - Flash: 448KB total
 *   - Bootloader: 4KB (0x00000000 - 0x00000FFF)
 *   - Application: 444KB (0x00001000 - 0x0006FFFF)
 * - RAM: 26KB total
 *   - Highcode: ~1KB (copied from flash)
 *   - Data: Variable
 *   - BSS: Variable
 *   - Heap: 512 bytes
 *   - Stack: 1KB
 */

/* Entry Point */
ENTRY(_start)

/* Memory regions */
MEMORY
{
    /* Reserve 4KB for bootloader */
    BOOT  (rx)  : ORIGIN = 0x00000000, LENGTH = 4K
    FLASH (rx)  : ORIGIN = 0x00001000, LENGTH = 444K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 26K
}

/* Highest address of the stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Reduced heap and stack for embedded use */
_Min_Heap_Size = 0x200;   /* 512 bytes heap */
_Min_Stack_Size = 0x400;  /* 1KB stack */

SECTIONS
{
    /* Vector table */
    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors))
        . = ALIGN(4);
    } >FLASH

    /* Init code */
    .init :
    {
        . = ALIGN(4);
        KEEP(*(.init))
        . = ALIGN(4);
    } >FLASH

    /* High-code section (frequently executed, runs from RAM) */
    .highcode :
    {
        . = ALIGN(4);
        _highcode_start = .;
        *(.highcode)
        *(.highcode.*)
        /* Put interrupt handlers in RAM for speed */
        *(.text.GPIOA_IRQHandler)
        *(.text.TMR2_IRQHandler)
        . = ALIGN(4);
        _highcode_end = .;
    } >RAM AT>FLASH

    _highcode_lma = LOADADDR(.highcode);
    _highcode_vma_start = ADDR(.highcode);
    _highcode_vma_end = _highcode_vma_start + SIZEOF(.highcode);

    /* Program code */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* Read-only data */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(4);
    } >FLASH

    /* Constructors (minimal) */
    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    /* Global pointer for small data access */
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
        *(.gnu.linkonce.d.*)
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
        *(.gnu.linkonce.b.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        /* 添加end符号供sbrk使用 */
        PROVIDE(end = .);
        PROVIDE(_end = .);
        PROVIDE(__end = .);
    } >RAM

    /* Heap */
    .heap (NOLOAD) :
    {
        . = ALIGN(4);
        PROVIDE(_heap_start = .);
        . = . + _Min_Heap_Size;
        PROVIDE(_heap_end = .);
    } >RAM

    /* Stack */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + _Min_Stack_Size;
        PROVIDE(_stack = .);
    } >RAM

    /* Memory usage check */
    ASSERT((_stack <= _estack), "RAM overflow!")
    
    /* Calculate remaining RAM */
    _ram_used = _stack - ORIGIN(RAM);
    _ram_free = LENGTH(RAM) - _ram_used;

    /* Discard unused sections */
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.eh_frame*)
        *(.comment)
        *(.note.*)
    }
}
