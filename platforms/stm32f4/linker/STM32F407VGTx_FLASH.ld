/**
 * STM32F407VGTx Linker Script for GCC and Clang
 *
 * Memory Configuration:
 *   - FLASH: 1MB (0x08000000 - 0x080FFFFF)
 *   - SRAM:  192KB (128KB + 64KB CCM)
 *     - Main SRAM: 128KB (0x20000000 - 0x2001FFFF)
 *     - CCM RAM:   64KB  (0x10000000 - 0x1000FFFF)
 *
 * Supported Compilers:
 *   - GCC (arm-none-eabi-gcc)
 *   - Clang (armclang / LLVM)
 *
 * Requirements: 11.4, 12.7
 */

/* Entry point - Reset_Handler from startup code */
ENTRY(Reset_Handler)

/* Minimum heap and stack sizes */
_Min_Heap_Size = 0x2000;   /* 8KB heap - adjust as needed */
_Min_Stack_Size = 0x1000;  /* 4KB stack - adjust as needed */

/* Memory regions definition */
MEMORY
{
    /* Main Flash memory - 1MB */
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K

    /* Main SRAM - 128KB */
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 128K

    /* Core Coupled Memory (CCM) RAM - 64KB
     * Note: CCM RAM is only accessible by CPU, not by DMA */
    CCMRAM (rw)     : ORIGIN = 0x10000000, LENGTH = 64K
}

/* Sections definition */
SECTIONS
{
    /*=========================================================================
     * Interrupt Vector Table
     * Must be placed at the start of Flash (0x08000000)
     *=========================================================================*/
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))    /* Startup code vector table */
        KEEP(*(.intvec))        /* Alternative name for IAR compatibility */
        . = ALIGN(4);
    } >FLASH

    /*=========================================================================
     * Program Code (.text section)
     *=========================================================================*/
    .text :
    {
        . = ALIGN(4);
        *(.text)                /* .text sections (code) */
        *(.text*)               /* .text* sections (code) */
        *(.glue_7)              /* ARM/Thumb interworking glue */
        *(.glue_7t)             /* ARM/Thumb interworking glue */
        *(.eh_frame)            /* Exception handling frame */

        KEEP(*(.init))          /* Constructor initialization */
        KEEP(*(.fini))          /* Destructor finalization */

        . = ALIGN(4);
        _etext = .;             /* End of code symbol */
    } >FLASH

    /*=========================================================================
     * Read-Only Data (.rodata section)
     *=========================================================================*/
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)              /* .rodata sections (constants) */
        *(.rodata*)             /* .rodata* sections (constants) */
        . = ALIGN(4);
    } >FLASH

    /*=========================================================================
     * ARM Exception Handling
     *=========================================================================*/
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /*=========================================================================
     * C++ Constructors/Destructors
     *=========================================================================*/
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    /*=========================================================================
     * Initialized Data Section
     * Loaded from Flash, copied to RAM at startup
     *=========================================================================*/

    /* Used by startup code to initialize data */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;             /* Start of data symbol */
        *(.data)                /* .data sections */
        *(.data*)               /* .data* sections */
        . = ALIGN(4);
        _edata = .;             /* End of data symbol */
    } >RAM AT> FLASH

    /*=========================================================================
     * CCM RAM Section
     * For performance-critical data (CPU-only access, no DMA)
     *=========================================================================*/
    .ccmram :
    {
        . = ALIGN(4);
        _sccmram = .;           /* Start of CCM RAM symbol */
        *(.ccmram)              /* .ccmram sections */
        *(.ccmram*)             /* .ccmram* sections */
        . = ALIGN(4);
        _eccmram = .;           /* End of CCM RAM symbol */
    } >CCMRAM

    /*=========================================================================
     * Uninitialized Data Section (.bss)
     * Zero-initialized at startup
     *=========================================================================*/
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;              /* Start of BSS symbol */
        __bss_start__ = _sbss;  /* Alternative symbol for C library */
        *(.bss)                 /* .bss sections */
        *(.bss*)                /* .bss* sections */
        *(COMMON)               /* Common symbols */
        . = ALIGN(4);
        _ebss = .;              /* End of BSS symbol */
        __bss_end__ = _ebss;    /* Alternative symbol for C library */
    } >RAM

    /*=========================================================================
     * User Heap and Stack
     *=========================================================================*/
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);       /* Symbol for sbrk() */
        PROVIDE(_end = .);      /* Alternative symbol */
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /* Initial stack pointer - top of RAM */
    _estack = ORIGIN(RAM) + LENGTH(RAM);

    /*=========================================================================
     * Debug Information Removal
     * Remove debug info from standard libraries to reduce binary size
     *=========================================================================*/
    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    /* ARM build attributes */
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
