/**
 * \file            uart_cold.c
 * \brief           UART gate, pin and publisher-priority cold ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_uart_cold.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_PLATFORM_PRIORITY_WRITE
#define NX_PLATFORM_PRIORITY_WRITE(reg, value) ((reg) = (uint8_t)(value))
#endif

#define NX_UART_COLD_VECTORS 4U
#define NX_UART_COLD_OWNED   8U

bool nx_stm32_uart_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool config_valid(const nx_stm32_uart_cold_config_t* config) {
    if (config == NULL || config->pins == NULL || config->vectors == NULL ||
        config->vector_count == 0U || config->vector_count > 3U ||
        config->priority > 15U || config->baud == 0U ||
        (config->registers != USART1 && config->registers != USART2))
        return false;
    bool primary = config->registers == USART1;
    if (config->clock_hz != (primary ? 84000000U : 42000000U) ||
        config->vectors[0] != (primary ? USART1_IRQn : USART2_IRQn))
        return false;
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (pin->registers != GPIOA || pin->port_index != 0U ||
            pin->snapshot == NULL || pin->pin != (primary ? 9U + i : 2U + i))
            return false;
    }
    if (config->pins[0].snapshot == config->pins[1].snapshot)
        return false;
    for (size_t i = 0U; i < config->vector_count; ++i) {
        IRQn_Type irq = config->vectors[i];
        if (irq != config->vectors[0] &&
            irq != (primary ? DMA2_Stream7_IRQn : DMA1_Stream6_IRQn) &&
            (!primary || irq != DMA2_Stream2_IRQn))
            return false;
        for (size_t j = 0U; j < i; ++j) {
            if (irq == config->vectors[j])
                return false;
        }
    }
    return true;
}

static volatile uint32_t*
gate_register(const nx_stm32_uart_cold_config_t* config) {
    return config->registers == USART1 ? &RCC->APB2ENR : &RCC->APB1ENR;
}

static uint32_t gate(const nx_stm32_uart_cold_config_t* config) {
    return config->registers == USART1 ? RCC_APB2ENR_USART1EN
                                       : RCC_APB1ENR_USART2EN;
}

static bool vector_idle(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&
           NVIC_GetActive(irq) == 0U;
}

nx_result_t
nx_stm32_uart_cold_acquire(const nx_stm32_uart_cold_config_t* config,
                           nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects != 0U || receipt->clock_added != 0U)
        return NX_ERROR_BUSY;
    /* GPIO is one enclosing Platform lease, never another UART receipt. */
    if ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN) == 0U)
        return NX_ERROR_STATE;
    for (size_t i = 0U; i < config->vector_count; ++i) {
        if (!vector_idle(config->vectors[i]))
            return NX_ERROR_BUSY;
    }
    if (config->kernel_bound && NVIC_GetPriorityGrouping() > 3U)
        return NX_ERROR_PERMISSION;
    volatile uint32_t* apb = gate_register(config);
    uint32_t mask = gate(config);
    receipt->clock_added = ~*apb & mask;
    receipt->effects = NX_UART_COLD_OWNED;
    NX_PLATFORM_REGISTER_WRITE(*apb, *apb | mask);
    nx_arch_dsb();
    if ((*apb & mask) == 0U)
        return NX_ERROR_IO;
    if ((config->registers->CR1 &
         (USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE |
          USART_CR1_IDLEIE | USART_CR1_PEIE)) != 0U ||
        (config->registers->CR3 &
         (USART_CR3_DMAT | USART_CR3_DMAR | USART_CR3_EIE)) != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->vector_count; ++i)
        receipt->priorities[i] = NVIC->IPR[(uint32_t)config->vectors[i]];
    receipt->effects |= NX_UART_COLD_VECTORS;
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        nx_result_t result = nx_stm32_gpio_clock_enable(pin->port_index);
        if (result != NX_SUCCESS)
            return result;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->effects |= (uint8_t)(1U << i);
        if (i == 0U && config->preload_tx) {
            NX_PLATFORM_REGISTER_WRITE(pin->registers->BSRR, 1U << pin->pin);
            nx_arch_dsb();
            if ((pin->registers->ODR & (1U << pin->pin)) == 0U)
                return NX_ERROR_IO;
        }
        result = nx_stm32_pin_configure(pin->registers, pin->pin, 2U, 7U,
                                        (uint8_t)(i == 0U ? 0U : 1U), false);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}

void nx_stm32_uart_cold_bind(nx_stm32_uart_state_t* state,
                             const nx_stm32_uart_cold_config_t* config) {
    state->registers = config->registers;
    state->irq = config->vectors[0];
    state->rx_storage = config->rx_storage;
    state->rx_capacity = config->rx_capacity;
    state->baud = config->baud;
    state->profile = config->profile;
}

bool nx_stm32_uart_cold_dma_valid(const nx_stm32_uart_dma_cold_config_t* config,
                                  bool receive, bool transmit) {
    if (config == NULL || !config_valid(&config->uart) ||
        config->resources.regions == NULL ||
        config->resources.region_count == 0U ||
        config->uart.vector_count !=
            1U + (receive ? 1U : 0U) + (transmit ? 1U : 0U))
        return false;
    bool primary = config->uart.registers == USART1;
    if (config->resources.dma != (primary ? DMA2 : DMA1) ||
        (receive && (!primary || config->receive != DMA2_Stream2 ||
                     config->uart.vectors[1] != DMA2_Stream2_IRQn)))
        return false;
    if (transmit &&
        (config->transmit.registers !=
             (primary ? DMA2_Stream7 : DMA1_Stream6) ||
         config->transmit.irq !=
             (primary ? DMA2_Stream7_IRQn : DMA1_Stream6_IRQn) ||
         config->transmit.stream != (primary ? 7U : 6U) ||
         config->transmit.channel != 4U ||
         config->uart.vectors[receive ? 2U : 1U] != config->transmit.irq))
        return false;
    return true;
}

nx_result_t
nx_stm32_uart_cold_verify(const nx_stm32_uart_cold_config_t* config,
                          const nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL ||
        (receipt->effects & NX_UART_COLD_VECTORS) == 0U)
        return NX_ERROR_INVALID;
    if ((*gate_register(config) & gate(config)) == 0U ||
        (config->registers->CR1 & USART_CR1_UE) == 0U)
        return NX_ERROR_IO;
    for (size_t i = 0U; i < config->vector_count; ++i) {
        IRQn_Type irq = config->vectors[i];
        uint32_t index = (uint32_t)irq;
        NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[index],
                                   (uint8_t)(config->priority << 4U));
        nx_arch_dsb();
        if (NVIC->IPR[index] != (uint8_t)(config->priority << 4U) ||
            NVIC_GetEnableIRQ(irq) == 0U || NVIC_GetActive(irq) != 0U)
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_uart_cold_release(const nx_stm32_uart_cold_config_t* config,
                           nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects == 0U && receipt->clock_added == 0U)
        return NX_SUCCESS;
    if ((receipt->effects & NX_UART_COLD_VECTORS) != 0U) {
        for (size_t i = 0U; i < config->vector_count; ++i) {
            IRQn_Type irq = config->vectors[i];
            uint32_t index = (uint32_t)irq;
            uint32_t bit = 1U << (index % 32U);
            NX_PLATFORM_REGISTER_WRITE(NVIC->ICER[index / 32U], bit);
            NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[index / 32U], bit);
            nx_arch_dsb();
            if (!vector_idle(irq))
                return NX_ERROR_BUSY;
            NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[index],
                                       receipt->priorities[i]);
            nx_arch_dsb();
            if (NVIC->IPR[index] != receipt->priorities[i])
                return NX_ERROR_IO;
        }
        receipt->effects &= (uint8_t)~NX_UART_COLD_VECTORS;
    }
    for (size_t i = 0U; i < 2U; ++i) {
        if ((receipt->effects & (1U << i)) == 0U)
            continue;
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        uint32_t mask = 3U << (pin->pin * 2U);
        uint32_t mode = pin->registers->MODER & mask;
        if (mode != 0U && mode != mask) {
            NX_PLATFORM_REGISTER_WRITE(pin->registers->MODER,
                                       pin->registers->MODER | mask);
            nx_arch_dsb();
            if ((pin->registers->MODER & mask) != mask)
                return NX_ERROR_IO;
        }
    }
    for (size_t i = 2U; i != 0U; --i) {
        uint8_t bit = (uint8_t)(1U << (i - 1U));
        if ((receipt->effects & bit) == 0U)
            continue;
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i - 1U];
        nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
        nx_arch_dsb();
        if (nx_stm32_pin_capture(pin->registers, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        receipt->effects &= (uint8_t)~bit;
    }
    if (receipt->clock_added != 0U) {
        volatile uint32_t* apb = gate_register(config);
        NX_PLATFORM_REGISTER_WRITE(*apb, *apb & ~receipt->clock_added);
        nx_arch_dsb();
        if ((*apb & receipt->clock_added) != 0U)
            return NX_ERROR_IO;
        receipt->clock_added = 0U;
    }
    *receipt = (nx_stm32_uart_cold_receipt_t){0};
    return NX_SUCCESS;
}
