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

#define NX_UART_COLD_VECTORS 64U
#define NX_UART_COLD_OWNED   128U

/** \brief Match one reviewed controller without authorizing an inferred bank.
 */
static const nx_stm32_uart_hardware_t*
hardware(const nx_stm32_uart_cold_config_t* config) {
    if (config == NULL || config->registers == NULL)
        return NULL;
    for (size_t i = 0U;
         i < sizeof(nx_stm32_uart_hardware) / sizeof(nx_stm32_uart_hardware[0]);
         ++i) {
        if (config->registers == nx_stm32_uart_hardware[i].registers)
            return &nx_stm32_uart_hardware[i];
    }
    return NULL;
}

bool nx_stm32_uart_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Default UARTs keep their original two-pad resource contract. */
static size_t pin_count(const nx_stm32_uart_cold_config_t* config) {
    return config->wire == NULL ? 2U : config->pin_count;
}

/** \brief RTS is the final optional modem pad in the reviewed ordered tuple.
 */
static bool rts_pin(const nx_stm32_uart_cold_config_t* config, size_t index) {
    return config->wire != NULL &&
           config->wire->mode == NX_UART_WIRE_ASYNCHRONOUS &&
           (config->wire->flow == NX_UART_FLOW_RTS ||
            config->wire->flow == NX_UART_FLOW_RTS_CTS) &&
           index + 1U == pin_count(config);
}

static bool config_valid(const nx_stm32_uart_cold_config_t* config) {
    const nx_stm32_uart_hardware_t* selected = hardware(config);
    if (config == NULL || config->pins == NULL || config->vectors == NULL ||
        config->vector_count == 0U || config->vector_count > 3U ||
        config->priority > 15U || config->baud == 0U || selected == NULL)
        return false;
    if (config->clock_hz != selected->clock_hz ||
        config->vectors[0] != selected->irq)
        return false;
    size_t count = pin_count(config);
    size_t expected = 2U;
    if (config->wire != NULL) {
        nx_uart_wire_mode_t mode = config->wire->mode;
        expected = mode == NX_UART_WIRE_HALF_DUPLEX   ? 1U
                   : mode == NX_UART_WIRE_SYNCHRONOUS ? 3U
                                                      : 2U;
        if (mode == NX_UART_WIRE_ASYNCHRONOUS) {
            if (config->wire->flow == NX_UART_FLOW_RTS_CTS)
                expected += 2U;
            else if (config->wire->flow != NX_UART_FLOW_NONE)
                ++expected;
        }
    }
    if (count != expected || count > 4U)
        return false;
    for (size_t i = 0U; i < count; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (!nx_stm32_platform_pin_valid(pin))
            return false;
        for (size_t prior = 0U; prior < i; ++prior) {
            if (pin->snapshot == config->pins[prior].snapshot ||
                (pin->registers == config->pins[prior].registers &&
                 pin->pin == config->pins[prior].pin))
                return false;
        }
    }
    for (size_t i = 0U; i < config->vector_count; ++i) {
        IRQn_Type irq = config->vectors[i];
        if ((int32_t)irq < 0 || (uint32_t)irq >= NX_STM32_EXTERNAL_IRQ_COUNT)
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
    return hardware(config)->apb2 ? &RCC->APB2ENR : &RCC->APB1ENR;
}

static uint32_t gate(const nx_stm32_uart_cold_config_t* config) {
    return hardware(config)->gate;
}

static bool vector_idle(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&
           NVIC_GetActive(irq) == 0U;
}

/** \brief Admit only the reviewed byte/event slice without acquiring gates. */
nx_result_t
nx_stm32_uart_byte_cold_admit(const nx_stm32_uart_state_t* state,
                              const nx_stm32_uart_cold_config_t* config,
                              const nx_stm32_uart_cold_receipt_t* receipt) {
    if (!nx_stm32_uart_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || receipt == NULL || !config_valid(config))
        return NX_ERROR_INVALID;
    if (config->vector_count != 1U || config->format != NULL ||
        (config->profile != NX_UART_RX_BYTES &&
         config->profile != NX_UART_RX_EVENTS))
        return NX_ERROR_UNSUPPORTED;
    if (config->rx_storage == NULL || config->rx_capacity == 0U ||
        config->baud > config->clock_hz / 16U)
        return NX_ERROR_INVALID;
    uint32_t divisor = config->clock_hz / config->baud +
                       (config->clock_hz % config->baud >=
                                config->baud / 2U + config->baud % 2U
                            ? 1U
                            : 0U);
    if (divisor == 0U || divisor > UINT16_MAX)
        return NX_ERROR_UNSUPPORTED;
    if (state->registers != NULL || state->initialized ||
        state->active != NULL || state->closing || receipt->effects != 0U ||
        receipt->clock_added != 0U || !vector_idle(config->vectors[0]))
        return NX_ERROR_BUSY;
    if (config->kernel_bound && NVIC_GetPriorityGrouping() > 3U)
        return NX_ERROR_PERMISSION;
    return NX_SUCCESS;
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
    /* Both pads may occupy different banks. The enclosing Platform owns their
     * gates; no UART may silently acquire an unrelated GPIO or release one. */
    uint32_t gpio_gates = 0U;
    for (size_t i = 0U; i < pin_count(config); ++i)
        gpio_gates |= 1U << config->pins[i].port_index;
    if ((RCC->AHB1ENR & gpio_gates) != gpio_gates)
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
    for (size_t i = 0U; i < pin_count(config); ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        nx_result_t result = nx_stm32_gpio_clock_enable(pin->port_index);
        if (result != NX_SUCCESS)
            return result;
        *pin->snapshot = nx_stm32_pin_capture(pin->registers, pin->pin);
        receipt->effects |= (uint8_t)(1U << i);
        bool open_drain = i == 0U && config->wire != NULL &&
                          (config->wire->mode == NX_UART_WIRE_HALF_DUPLEX ||
                           config->wire->mode == NX_UART_WIRE_SMARTCARD);
        bool rts = rts_pin(config, i);
        if (i == 0U || rts) {
            /* RM0090 SIR is idle-low; other TX personalities idle high. */
            const bool high = rts || config->wire == NULL ||
                              config->wire->mode != NX_UART_WIRE_IRDA;
            const uint32_t pin_mask = 1U << pin->pin;
            NX_PLATFORM_REGISTER_WRITE(pin->registers->BSRR,
                                       high ? pin_mask : pin_mask << 16U);
            nx_arch_dsb();
            if ((pin->registers->ODR & pin_mask) != (high ? pin_mask : 0U))
                return NX_ERROR_IO;
        }
        /* ES0182 2.11.9: keep RTS inactive until both UE and RE exist. */
        uint8_t mode = rts ? 1U : 2U;
        uint8_t pull = (uint8_t)(i == 0U || rts ? 0U : 1U);
        result = nx_stm32_pin_configure(pin->registers, pin->pin, mode,
                                        hardware(config)->alternate, pull,
                                        open_drain);
        if (result != NX_SUCCESS)
            return result;
        nx_arch_dsb();
        uint32_t shift = (uint32_t)pin->pin * 2U;
        if (((pin->registers->MODER >> shift) & 3U) != mode ||
            ((pin->registers->OTYPER >> pin->pin) & 1U) !=
                (open_drain ? 1U : 0U) ||
            ((pin->registers->PUPDR >> shift) & 3U) != pull ||
            ((pin->registers->AFR[pin->pin / 8U] >> ((pin->pin % 8U) * 4U)) &
             15U) != hardware(config)->alternate)
            return NX_ERROR_IO;
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

/** \brief Unselected directions retain no stream, publisher or selector. */
static bool empty_route(const nx_stm32_dma_route_t* selected) {
    return selected->dma == NULL && selected->stream == NULL &&
           (int32_t)selected->irq == 0 && selected->lane == 0U &&
           selected->channel == 0U;
}

bool nx_stm32_uart_dma_route_valid(
    const nx_stm32_uart_dma_cold_config_t* config, bool receive,
    bool transmit) {
    if (config == NULL || config->uart.vectors == NULL ||
        config->resources.regions == NULL ||
        config->resources.region_count == 0U ||
        config->resources.region_count >
            SIZE_MAX / sizeof(*config->resources.regions) ||
        config->uart.vector_count !=
            1U + (receive ? 1U : 0U) + (transmit ? 1U : 0U))
        return false;
    const nx_stm32_uart_hardware_t* selected = hardware(&config->uart);
    if (selected == NULL || config->uart.clock_hz != selected->clock_hz ||
        config->uart.vectors[0] != selected->irq)
        return false;
    const nx_stm32_dma_route_t* rx = &config->resources.receive;
    const nx_stm32_dma_route_t* tx = &config->resources.transmit;
    if (receive
            ? (!nx_stm32_uart_dma_connection_valid(selected, rx, true) ||
               rx->dma != selected->dma || config->uart.vectors[1] != rx->irq)
            : !empty_route(rx))
        return false;
    if (transmit ? (!nx_stm32_uart_dma_connection_valid(selected, tx, false) ||
                    tx->dma != selected->dma ||
                    config->uart.vectors[receive ? 2U : 1U] != tx->irq)
                 : !empty_route(tx))
        return false;
    return !receive || !transmit ||
           (rx->stream != tx->stream && rx->irq != tx->irq);
}

bool nx_stm32_uart_cold_dma_valid(const nx_stm32_uart_dma_cold_config_t* config,
                                  bool receive, bool transmit) {
    return config != NULL && config_valid(&config->uart) &&
           nx_stm32_uart_dma_route_valid(config, receive, transmit);
}

bool nx_stm32_uart_dma_powered(const nx_stm32_uart_dma_cold_config_t* config) {
    const nx_stm32_uart_hardware_t* selected =
        config == NULL ? NULL : hardware(&config->uart);
    return selected != NULL &&
           ((selected->apb2 ? RCC->APB2ENR : RCC->APB1ENR) & selected->gate) !=
               0U &&
           (RCC->AHB1ENR & (selected->dma == DMA2 ? RCC_AHB1ENR_DMA2EN
                                                  : RCC_AHB1ENR_DMA1EN)) != 0U;
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
    size_t last = pin_count(config) - 1U;
    if (rts_pin(config, last)) {
        if ((config->registers->CR1 & (USART_CR1_UE | USART_CR1_RE)) !=
            (USART_CR1_UE | USART_CR1_RE))
            return NX_ERROR_IO;
        const nx_stm32_platform_pin_config_t* pin = &config->pins[last];
        nx_result_t result =
            nx_stm32_pin_configure(pin->registers, pin->pin, 2U,
                                   hardware(config)->alternate, 0U, false);
        if (result != NX_SUCCESS)
            return result;
        nx_arch_dsb();
        if (((pin->registers->MODER >> ((uint32_t)pin->pin * 2U)) & 3U) != 2U)
            return NX_ERROR_IO;
    }
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
    for (size_t i = 0U; i < pin_count(config); ++i) {
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
    for (size_t i = pin_count(config); i != 0U; --i) {
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
