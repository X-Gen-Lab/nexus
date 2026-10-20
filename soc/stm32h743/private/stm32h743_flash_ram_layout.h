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

#endif /* NX_STM32H743_FLASH_RAM_LAYOUT_H */
