/**
 * \file            cold.h
 * \brief           Static EXTI configuration and retained cold withdrawal
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_EXTI_COLD_H
#define NEXUS_STM32F4_DRIVERS_EXTI_COLD_H

#include "drivers/exti/instance.h"
#include "drivers/gpio/pins.h"
#include "nexus/arch/arch.h"
#include "private/cold_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Static EXTI line, bounded event storage and selected IRQ policy. */
typedef struct {
    GPIO_TypeDef* gpio;
    nx_exti_event_t* storage;
    size_t capacity;
    nx_exti_edge_t edge;
    uint8_t gpio_index;
    uint8_t line;
    uint8_t priority;
    uint8_t pull;
} nx_stm32_exti_config_t;

/** \brief Prepare an admitted line; Platform owns its unique pad snapshot.
 * \note Failure retains initialized EXTI ownership until close succeeds.
 */
static inline nx_result_t
nx_stm32_exti_prepare(nx_stm32_exti_state_t* port,
                      const nx_stm32_exti_config_t* config) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->gpio == NULL ||
        config->storage == NULL || config->capacity == 0U ||
        config->capacity > 4096U || config->gpio_index > 8U ||
        config->line > 15U || config->pull > 2U ||
        config->gpio !=
            (GPIO_TypeDef*)(uintptr_t)(GPIOA_BASE +
                                       (uintptr_t)config->gpio_index *
                                           UINT32_C(0x400)) ||
        config->priority > 15U || config->edge < NX_EXTI_RISING ||
        config->edge > NX_EXTI_BOTH)
        return NX_ERROR_INVALID;
    if (port->initialized || port->cold_owned)
        return NX_ERROR_BUSY;
    /* ES0182: even an input edge on PC13 can disturb a crystal LSE. */
    if (config->gpio == GPIOC && config->line == 13U &&
        (RCC->BDCR & RCC_BDCR_LSEBYP) == 0U &&
        (RCC->BDCR & (RCC_BDCR_LSEON | RCC_BDCR_LSERDY)) != 0U)
        return NX_ERROR_BUSY;
    uint32_t bit = 1U << config->line;
    if (((EXTI->IMR | EXTI->EMR | EXTI->PR) & bit) != 0U)
        return NX_ERROR_BUSY;
    IRQn_Type irq = config->line < 5U
                        ? (IRQn_Type)((int)EXTI0_IRQn + (int)config->line)
                        : (config->line < 10U ? EXTI9_5_IRQn : EXTI15_10_IRQn);
    uint32_t vector_mask =
        config->line < 5U ? bit : (config->line < 10U ? 0x3E0U : 0xFC00U);
    if (NVIC_GetActive(irq) != 0U ||
        ((EXTI->IMR & vector_mask) != 0U && NVIC_GetEnableIRQ(irq) != 0U &&
         NVIC_GetPriority(irq) != config->priority))
        return NX_ERROR_BUSY;
    if ((RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U)
        return NX_ERROR_STATE;
    port->cold_owned = true;
    nx_result_t status = nx_stm32_pin_configure(config->gpio, config->line, 0U,
                                                0U, config->pull, false);
    if (status != NX_SUCCESS)
        return status;
    port->registers = EXTI;
    port->gpio = config->gpio;
    port->storage = config->storage;
    port->capacity = config->capacity;
    port->line = config->line;
    port->edge = config->edge;
    status = nx_stm32_exti_initialize(port, SYSCFG, config->gpio_index);
    if (status != NX_SUCCESS)
        return status;
    /* A newly armed line or selected peer now owns any pending hint. */
    nx_arch_dsb();
    nx_arch_isb();
    NVIC_SetPriority(irq, config->priority);
    NVIC_EnableIRQ(irq);
    nx_arch_dsb();
    nx_arch_isb();
    return NVIC_GetPriority(irq) == config->priority &&
                   NVIC_GetEnableIRQ(irq) != 0U && NVIC_GetActive(irq) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Drain the actual publisher; final pad restoration stays
 * Platform-owned. */
static inline nx_result_t nx_stm32_exti_close(nx_stm32_exti_state_t* port) {
    if (!nx_stm32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL)
        return NX_ERROR_INVALID;
    if (!port->cold_owned)
        return port->initialized ? NX_ERROR_BUSY : NX_SUCCESS;
    nx_result_t result =
        port->initialized ? nx_stm32_exti_ops.stop(port) : NX_SUCCESS;
    if (result == NX_SUCCESS)
        port->cold_owned = false;
    return result;
}

#ifdef __cplusplus
}
#endif
#endif
