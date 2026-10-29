/**
 * \file            uart_cold.c
 * \brief           Concrete UART pin, clock and publisher cold ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_uart_cold.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_usart.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_PLATFORM_PRIORITY_WRITE
#define NX_PLATFORM_PRIORITY_WRITE(reg, value) ((reg) = (uint8_t)(value))
#endif
#define NX_UART_COLD_VECTORS 4U
#define NX_UART_COLD_OWNED   8U
_Static_assert(sizeof(nx_gd32_uart_cold_t) == 1U,
               "Cold receipt retains only unique effects");

bool nx_gd32_uart_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool config_valid(const nx_gd32_uart_cold_config_t* config) {
    if (config == NULL || config->pins == NULL || config->snapshots == NULL ||
        config->vectors == NULL || config->priorities == NULL ||
        config->vector_count == 0U || config->vector_count > 3U ||
        config->priority > 15U || config->baud == 0U ||
        config->baud > 1000000U ||
        (config->controller != &nx_gd32_usart0_controller &&
         config->controller != &nx_gd32_usart1_controller))
        return false;
    bool primary = config->controller == &nx_gd32_usart0_controller;
    if (config->vectors[0] != config->controller->irq)
        return false;
    for (unsigned i = 0U; i < 2U; ++i) {
        const nx_gd32_cold_pin_config_t* pin = &config->pins[i];
        if (pin->registers != GPIOA ||
            pin->pin != (primary ? 9U + i : 2U + i) || pin->alternate != 7U ||
            pin->mode != 2U || pin->pull > 2U || pin->open_drain)
            return false;
    }
    for (unsigned i = 0U; i < config->vector_count; ++i) {
        IRQn_Type irq = config->vectors[i];
        if (irq != config->controller->irq &&
            irq != (primary ? DMA1_Channel7_IRQn : DMA0_Channel6_IRQn) &&
            (!primary || irq != DMA1_Channel2_IRQn))
            return false;
        for (unsigned j = 0U; j < i; ++j) {
            if (irq == config->vectors[j])
                return false;
        }
    }
    return true;
}

static volatile uint32_t*
clock_register(const nx_gd32_uart_cold_config_t* config) {
    return config->controller == &nx_gd32_usart0_controller ? &RCU_APB2EN
                                                            : &RCU_APB1EN;
}

static uint32_t gate(const nx_gd32_uart_cold_config_t* config) {
    return (uint32_t)BIT(RCU_BIT_POS(config->controller->clock));
}

static bool publisher_quiet(IRQn_Type irq) {
    unsigned number = (unsigned)irq;
    return ((NVIC->ISER[number / 32U] | NVIC->ISPR[number / 32U] |
             NVIC->IABR[number / 32U]) &
            (1U << (number % 32U))) == 0U;
}

bool nx_gd32_uart_cold_dma_valid(const nx_gd32_uart_dma_cold_config_t* config,
                                 bool receive, bool transmit) {
    if (config == NULL || !config_valid(&config->uart) ||
        config->resources.regions == NULL ||
        config->resources.region_count == 0U ||
        config->uart.vector_count !=
            1U + (receive ? 1U : 0U) + (transmit ? 1U : 0U))
        return false;
    bool primary = config->uart.controller == &nx_gd32_usart0_controller;
    if (receive && (!primary || config->uart.vectors[1] != DMA1_Channel2_IRQn))
        return false;
    if (transmit && config->uart.vectors[receive ? 2U : 1U] !=
                        (primary ? DMA1_Channel7_IRQn : DMA0_Channel6_IRQn))
        return false;
    if (!receive &&
        (config->uart.receive == NULL || config->uart.capacity == 0U ||
         (config->uart.profile != NX_UART_RX_BYTES &&
          config->uart.profile != NX_UART_RX_EVENTS)))
        return false;
    return true;
}

nx_result_t nx_gd32_uart_cold_acquire(const nx_gd32_uart_cold_config_t* config,
                                      nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || cold == NULL)
        return NX_ERROR_INVALID;
    if (cold->effects != 0U)
        return NX_ERROR_BUSY;
    for (unsigned i = 0U; i < config->vector_count; ++i) {
        if (!publisher_quiet(config->vectors[i]))
            return NX_ERROR_BUSY;
        if (config->vectors[i] == DMA1_Channel2_IRQn &&
            (DMA_CHCTL(DMA1, 2U) & DMA_CHXCTL_CHEN) != 0U)
            return NX_ERROR_BUSY;
        if (config->vectors[i] == DMA1_Channel7_IRQn &&
            (DMA_CHCTL(DMA1, 7U) & DMA_CHXCTL_CHEN) != 0U)
            return NX_ERROR_BUSY;
        if (config->vectors[i] == DMA0_Channel6_IRQn &&
            (DMA_CHCTL(DMA0, 6U) & DMA_CHXCTL_CHEN) != 0U)
            return NX_ERROR_BUSY;
    }
    if (config->kernel_bound && NVIC_GetPriorityGrouping() > 3U)
        return NX_ERROR_PERMISSION;
    if ((*clock_register(config) & gate(config)) != 0U)
        return NX_ERROR_BUSY;
    /* No foreign gate is admitted. The concrete initializer enables it; the
     * same receipt must release even a partially initialized stopping UART. */
    cold->effects = NX_UART_COLD_OWNED | NX_UART_COLD_VECTORS;
    for (unsigned i = 0U; i < config->vector_count; ++i)
        config->priorities[i] = NVIC->IPR[(unsigned)config->vectors[i]];
    for (unsigned i = 0U; i < 2U; ++i) {
        const nx_gd32_cold_pin_config_t* pin = &config->pins[i];
        nx_result_t status = nx_gd32_gpio_clock_enable(0U);
        if (status != NX_SUCCESS)
            return status;
        config->snapshots[i] = nx_gd32_pin_capture(pin->registers, pin->pin);
        cold->effects |= (uint8_t)(1U << i);
        if (config->preload_transmit && i == 0U) {
            uint32_t bit = 1U << pin->pin;
            NX_PLATFORM_REGISTER_WRITE(GPIO_BOP(pin->registers), bit);
            nx_arch_dsb();
            if ((GPIO_OCTL(pin->registers) & bit) == 0U)
                return NX_ERROR_IO;
        }
        status = nx_gd32_pin_configure(pin->registers, pin->pin, pin->mode,
                                       pin->alternate, pin->pull, false);
        if (status != NX_SUCCESS)
            return status;
    }
    return NX_SUCCESS;
}

nx_result_t nx_gd32_uart_cold_verify(const nx_gd32_uart_cold_config_t* config,
                                     const nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || cold == NULL ||
        (cold->effects & NX_UART_COLD_VECTORS) == 0U)
        return NX_ERROR_INVALID;
    if ((*clock_register(config) & gate(config)) == 0U ||
        (USART_CTL0(config->controller->registers) & USART_CTL0_UEN) == 0U)
        return NX_ERROR_IO;
    for (unsigned i = 0U; i < config->vector_count; ++i) {
        unsigned irq = (unsigned)config->vectors[i];
        NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[irq], config->priority << 4U);
        nx_arch_dsb();
        if (NVIC->IPR[irq] != config->priority << 4U ||
            (NVIC->ISER[irq / 32U] & (1U << (irq % 32U))) == 0U ||
            (NVIC->IABR[irq / 32U] & (1U << (irq % 32U))) != 0U)
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

nx_result_t nx_gd32_uart_cold_release(const nx_gd32_uart_cold_config_t* config,
                                      nx_gd32_uart_cold_t* cold) {
    if (!nx_gd32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || cold == NULL)
        return NX_ERROR_INVALID;
    if (cold->effects == 0U)
        return NX_SUCCESS;
    if ((cold->effects & NX_UART_COLD_VECTORS) != 0U) {
        for (unsigned i = 0U; i < config->vector_count; ++i) {
            unsigned irq = (unsigned)config->vectors[i];
            uint32_t bit = 1U << (irq % 32U);
            NX_PLATFORM_REGISTER_WRITE(NVIC->ICER[irq / 32U], bit);
            NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[irq / 32U], bit);
            nx_arch_dsb();
            if (!publisher_quiet(config->vectors[i]))
                return NX_ERROR_BUSY;
            NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[irq], config->priorities[i]);
            nx_arch_dsb();
            if (NVIC->IPR[irq] != config->priorities[i])
                return NX_ERROR_IO;
        }
        cold->effects &= (uint8_t)~NX_UART_COLD_VECTORS;
    }
    /* Disconnect both pads before restoring either original pad. */
    for (unsigned i = 0U; i < 2U; ++i) {
        if ((cold->effects & (1U << i)) == 0U)
            continue;
        const nx_gd32_cold_pin_config_t* pin = &config->pins[i];
        uint32_t mask = 3U << (pin->pin * 2U);
        uint32_t mode = GPIO_CTL(pin->registers) & mask;
        if (mode != 0U && mode != mask) {
            NX_PLATFORM_REGISTER_WRITE(GPIO_CTL(pin->registers),
                                       GPIO_CTL(pin->registers) | mask);
            nx_arch_dsb();
            if ((GPIO_CTL(pin->registers) & mask) != mask)
                return NX_ERROR_IO;
        }
    }
    for (unsigned i = 2U; i != 0U; --i) {
        unsigned index = i - 1U;
        uint8_t bit = (uint8_t)(1U << index);
        if ((cold->effects & bit) == 0U)
            continue;
        const nx_gd32_cold_pin_config_t* pin = &config->pins[index];
        nx_gd32_pin_restore(pin->registers, pin->pin, config->snapshots[index]);
        nx_arch_dsb();
        if (nx_gd32_pin_capture(pin->registers, pin->pin) !=
            config->snapshots[index])
            return NX_ERROR_IO;
        cold->effects &= (uint8_t)~bit;
    }
    if ((cold->effects & NX_UART_COLD_OWNED) != 0U) {
        volatile uint32_t* clock = clock_register(config);
        NX_PLATFORM_REGISTER_WRITE(*clock, *clock & ~gate(config));
        nx_arch_dsb();
        if ((*clock & gate(config)) != 0U)
            return NX_ERROR_IO;
        cold->effects &= (uint8_t)~NX_UART_COLD_OWNED;
    }
    return NX_SUCCESS;
}
