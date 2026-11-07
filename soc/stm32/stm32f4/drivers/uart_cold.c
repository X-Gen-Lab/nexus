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

/** \brief Immutable reviewed pads, clocks and publishers for cold admission. */
typedef struct {
    USART_TypeDef* registers;
    GPIO_TypeDef* tx_gpio;
    GPIO_TypeDef* rx_gpio;
    uint32_t clock_hz;
    uint32_t gate;
    IRQn_Type irq;
    uint8_t tx_port;
    uint8_t tx_pin;
    uint8_t rx_port;
    uint8_t rx_pin;
    uint8_t alternate;
    bool apb2;
} nx_stm32_uart_cold_hardware_t;

/* DS8626 alternate functions and RM0090 APB clocks. This readonly cold table
 * adds no per-instance state or byte-path lookup. Only listed pad pairs are
 * admitted; additional silicon alternatives need independent route review. */
static const nx_stm32_uart_cold_hardware_t s_nx_uart_hardware[] = {
    {USART1, GPIOA, GPIOA, 84000000U, RCC_APB2ENR_USART1EN, USART1_IRQn, 0U, 9U,
     0U, 10U, 7U, true},
    {USART2, GPIOA, GPIOA, 42000000U, RCC_APB1ENR_USART2EN, USART2_IRQn, 0U, 2U,
     0U, 3U, 7U, false},
    {USART3, GPIOB, GPIOB, 42000000U, RCC_APB1ENR_USART3EN, USART3_IRQn, 1U,
     10U, 1U, 11U, 7U, false},
    {UART4, GPIOA, GPIOA, 42000000U, RCC_APB1ENR_UART4EN, UART4_IRQn, 0U, 0U,
     0U, 1U, 8U, false},
    {UART5, GPIOC, GPIOD, 42000000U, RCC_APB1ENR_UART5EN, UART5_IRQn, 2U, 12U,
     3U, 2U, 8U, false},
    {USART6, GPIOC, GPIOC, 84000000U, RCC_APB2ENR_USART6EN, USART6_IRQn, 2U, 6U,
     2U, 7U, 8U, true},
};

/** \brief Match one reviewed controller without authorizing an inferred bank.
 */
static const nx_stm32_uart_cold_hardware_t*
hardware(const nx_stm32_uart_cold_config_t* config) {
    if (config == NULL)
        return NULL;
    for (size_t i = 0U;
         i < sizeof(s_nx_uart_hardware) / sizeof(s_nx_uart_hardware[0]); ++i) {
        if (config->registers == s_nx_uart_hardware[i].registers)
            return &s_nx_uart_hardware[i];
    }
    return NULL;
}

bool nx_stm32_uart_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool config_valid(const nx_stm32_uart_cold_config_t* config) {
    const nx_stm32_uart_cold_hardware_t* selected = hardware(config);
    if (config == NULL || config->pins == NULL || config->vectors == NULL ||
        config->vector_count == 0U || config->vector_count > 3U ||
        config->priority > 15U || config->baud == 0U || selected == NULL)
        return false;
    bool primary = config->registers == USART1;
    if (config->clock_hz != selected->clock_hz ||
        config->vectors[0] != selected->irq ||
        (!primary && config->registers != USART2 && config->vector_count != 1U))
        return false;
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_stm32_platform_pin_config_t* pin = &config->pins[i];
        if (pin->registers !=
                (i == 0U ? selected->tx_gpio : selected->rx_gpio) ||
            pin->port_index !=
                (i == 0U ? selected->tx_port : selected->rx_port) ||
            pin->snapshot == NULL ||
            pin->pin != (i == 0U ? selected->tx_pin : selected->rx_pin))
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
    uint32_t gpio_gates =
        (1U << config->pins[0].port_index) | (1U << config->pins[1].port_index);
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
        result = nx_stm32_pin_configure(pin->registers, pin->pin, 2U,
                                        hardware(config)->alternate,
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
    if ((!primary && config->uart.registers != USART2) ||
        config->resources.dma != (primary ? DMA2 : DMA1) ||
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
