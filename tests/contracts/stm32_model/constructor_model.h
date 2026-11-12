/**
 * \file            constructor_model.h
 * \brief           Exact MMIO addresses shared by constructor and driver TUs
 * \author          Nexus Team
 */
#ifndef NX_STM32_CONSTRUCTOR_MODEL_H
#define NX_STM32_CONSTRUCTOR_MODEL_H

#include "io_poll.h"
#include "stm32f407_provider.h"

/* Cold C providers validate physical routes. The harness maps those addresses
 * for every translation unit; only CPU/NVIC leaves and fault injection differ.
 * The ordinary I/O model keeps its separate local GPIOA fixture unchanged.
 */
#undef GPIOA
#define GPIOA ((GPIO_TypeDef*)GPIOA_BASE)

/** \brief Apply cold register writes and the GPIO output-latch side effect. */
void nx_stm32_constructor_write(volatile uint32_t* reg, uint32_t value);
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_stm32_constructor_write(&(reg), (uint32_t)(value))

#endif
