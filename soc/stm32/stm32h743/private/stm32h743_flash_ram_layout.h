/**
 * \file            stm32h743_flash_ram_layout.h
 * \brief           Checked C and assembly RAM workspace prefix ABI
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_FLASH_RAM_LAYOUT_H
#define NX_STM32H743_FLASH_RAM_LAYOUT_H

/* Plain integer expressions are shared with assembler-with-cpp. Only the
 * fixed vector/stack prefix is visible to the bridge; tail state stays C. */
#define NX_H743_FLASH_RAM_VECTOR_WORDS 166
#define NX_H743_FLASH_RAM_STACK_WORDS  256
#define NX_H743_FLASH_RAM_STACK_TOP_BYTES                                      \
    ((NX_H743_FLASH_RAM_VECTOR_WORDS + NX_H743_FLASH_RAM_STACK_WORDS) * 4)

/* Fixed ARM C11/-fshort-enums ABI. Every offset has a C assertion. */
#define NX_H743_RAM_DESTINATION_OFFSET  1696
#define NX_H743_RAM_WORDS_OFFSET        1700
#define NX_H743_RAM_SAVED_CR_OFFSET     1736
#define NX_H743_RAM_BANK_OFFSET         1740
#define NX_H743_RAM_FAULT_OFFSET        1752
#define NX_H743_RAM_GUARD_EFFECT_OFFSET 1788
#define NX_H743_RAM_FLASH_BASE          0x52002000
#define NX_H743_RAM_MPU_BASE            0xe000ed90
#define NX_H743_RAM_VTOR_ADDRESS        0xe000ed08

#endif /* NX_STM32H743_FLASH_RAM_LAYOUT_H */
