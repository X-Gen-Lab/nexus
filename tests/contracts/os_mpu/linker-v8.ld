/* Synthetic CPU software memory. Products own their physical reservations. */
ENTRY(_start)
MEMORY {
    PRIV_FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
    USER_FLASH (rx) : ORIGIN = 0x08080000, LENGTH = 64K
    SVC_FLASH (rx) : ORIGIN = 0x08090000, LENGTH = 64K
    PRIV_RAM (rw) : ORIGIN = 0x20000000, LENGTH = 128K
    USER_RAM (rw) : ORIGIN = 0x20020000, LENGTH = 128K
}
__nexus_privileged_flash_start__ = ORIGIN(PRIV_FLASH);
__nexus_privileged_flash_end__ = ORIGIN(PRIV_FLASH) + LENGTH(PRIV_FLASH);
__nexus_privileged_ram_start__ = ORIGIN(PRIV_RAM);
__nexus_privileged_ram_end__ = ORIGIN(PRIV_RAM) + LENGTH(PRIV_RAM);
__nexus_user_flash_start__ = ORIGIN(USER_FLASH);
__nexus_user_flash_end__ = ORIGIN(USER_FLASH) + LENGTH(USER_FLASH);
__nexus_user_ram_start__ = ORIGIN(USER_RAM);
__nexus_user_ram_end__ = ORIGIN(USER_RAM) + LENGTH(USER_RAM);
__nexus_syscall_flash_start__ = ORIGIN(SVC_FLASH);
__nexus_syscall_flash_end__ = ORIGIN(SVC_FLASH) + LENGTH(SVC_FLASH);
__privileged_functions_start__ = __nexus_privileged_flash_start__;
__syscalls_flash_start__ = __nexus_syscall_flash_start__;
__syscalls_flash_end__ = __nexus_syscall_flash_end__ - 1;
__privileged_functions_end__ = __nexus_privileged_flash_end__ - 1;
__privileged_sram_start__ = __nexus_privileged_ram_start__;
__privileged_sram_end__ = __nexus_privileged_ram_end__ - 1;
__unprivileged_flash_start__ = __nexus_user_flash_start__;
__unprivileged_flash_end__ = __nexus_user_flash_end__ - 1;
SECTIONS {
    .privileged_text : {
        KEEP(*(.runtime.references))
        *(privileged_functions)
        *(.text*)
        *(.rodata*)
    } > PRIV_FLASH
    .user_text : { *(.nexus_user_text) } > USER_FLASH
    .syscalls : { *(freertos_system_calls) } > SVC_FLASH
    .ARM.extab : { *(.ARM.extab*) } > PRIV_FLASH
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > PRIV_FLASH
    .data : {
        __data_start__ = .;
        *(privileged_data)
        *(.data*)
        __data_end__ = .;
    } > PRIV_RAM AT> PRIV_FLASH
    .bss (NOLOAD) : {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    } > PRIV_RAM
    .user_data (NOLOAD) : { *(.nexus_user_data) } > USER_RAM
    /DISCARD/ : { *(.note.GNU-stack) }
}
