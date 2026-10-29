/**
 * \file            platform_cold.c
 * \brief           Concrete H743 selected pad and gate transactions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_platform_cold.h"

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool gate_valid(const nx_stm32h743_gate_lease_config_t* gate) {
    return gate->mask == 0U || gate->added != NULL;
}
static bool config_valid(const nx_stm32h743_platform_config_t* config) {
    if (config == NULL || config->ports == NULL || config->port_count == 0U ||
        config->port_count > 11U || config->gpio_added == NULL ||
        !gate_valid(&config->apb2) || !gate_valid(&config->apb1) ||
        !gate_valid(&config->ahb1) || !gate_valid(&config->apb4) ||
        (config->dma_lanes & ~15U) != 0U ||
        (config->dma_strict & ~config->dma_lanes) != 0U ||
        (config->dma_lanes != 0U && config->dma_added == NULL))
        return false;
    for (size_t i = 0U; i < config->port_count; ++i) {
        const nx_stm32h743_gpio_lease_config_t* port = &config->ports[i];
        if (port->registers == NULL || port->receipt == NULL ||
            port->pins == 0U || port->bank >= 11U)
            return false;
        for (size_t j = 0U; j < i; ++j)
            if (port->registers == config->ports[j].registers ||
                port->receipt == config->ports[j].receipt ||
                port->bank == config->ports[j].bank)
                return false;
    }
    return true;
}
static uint32_t fields(uint16_t pins) {
    uint32_t value = 0U;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((pins & (1U << pin)) != 0U)
            value |= 3U << (pin * 2U);
    return value;
}
static uint32_t alternate_fields(uint16_t pins, unsigned half) {
    uint32_t value = 0U;
    for (unsigned pin = 0U; pin < 8U; ++pin)
        if ((pins & (1U << (pin + half * 8U))) != 0U)
            value |= 15U << (pin * 4U);
    return value;
}
static bool pins_available(const nx_stm32h743_gpio_lease_config_t* port) {
    GPIO_TypeDef* gpio = port->registers;
    if ((NX_H743_READ(&gpio->LCKR) & port->pins) != 0U)
        return false;
    uint32_t mode = NX_H743_READ(&gpio->MODER);
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((port->pins & (1U << pin)) != 0U) {
            uint32_t value = (mode >> (pin * 2U)) & 3U;
            if (value != 0U && value != 3U)
                return false;
        }
    return true;
}
static nx_result_t dma_idle(uint8_t lanes, uint8_t strict, bool acquired) {
    if ((NX_H743_READ(&RCC->AHB1RSTR) &
         (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U)
        return NX_ERROR_BUSY;
    bool powered = (NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0U;
    if (acquired && !powered)
        return NX_ERROR_IO;
    for (unsigned lane = 0U; lane < 4U; ++lane) {
        if ((lanes & (1U << lane)) == 0U)
            continue;
        IRQn_Type irq = (IRQn_Type)((int)DMA1_Stream0_IRQn + (int)lane);
        if (!NX_H743_IRQ_AVAILABLE(irq))
            return NX_ERROR_BUSY;
        if (!powered)
            continue;
        DMA_Stream_TypeDef* const streams[4] = {DMA1_Stream0, DMA1_Stream1,
                                                DMA1_Stream2, DMA1_Stream3};
        DMAMUX_Channel_TypeDef* const muxes[4] = {
            DMAMUX1_Channel0, DMAMUX1_Channel1, DMAMUX1_Channel2,
            DMAMUX1_Channel3};
        DMA_Stream_TypeDef* stream = streams[lane];
        DMAMUX_Channel_TypeDef* mux = muxes[lane];
        if (NX_H743_READ(&stream->CR) != 0U || NX_H743_READ(&mux->CCR) != 0U)
            return NX_ERROR_BUSY;
        if ((strict & (1U << lane)) != 0U) {
            const uint32_t masks[4] = {0x3DU, 0xF40U, 0x3D0000U, 0xF400000U};
            if (NX_H743_READ(&stream->NDTR) != 0U ||
                NX_H743_READ(&stream->PAR) != 0U ||
                NX_H743_READ(&stream->M0AR) != 0U ||
                NX_H743_READ(&stream->M1AR) != 0U ||
                (NX_H743_READ(&stream->FCR) & 0x87U) != 1U ||
                (NX_H743_READ(&DMA1->LISR) & masks[lane]) != 0U)
                return NX_ERROR_BUSY;
        }
    }
    return NX_SUCCESS;
}
nx_result_t nx_stm32h743_dma_preflight(uint8_t lanes, uint8_t strict) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (lanes == 0U || (lanes & ~15U) != 0U || (strict & ~lanes) != 0U)
        return NX_ERROR_INVALID;
    return dma_idle(lanes, strict, false);
}
nx_result_t
nx_stm32h743_platform_preflight(const nx_stm32h743_platform_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    uint32_t powered = NX_H743_READ(&RCC->AHB4ENR);
    for (size_t i = 0U; i < config->port_count; ++i) {
        const nx_stm32h743_gpio_lease_config_t* port = &config->ports[i];
        if ((powered & (1U << port->bank)) != 0U && !pins_available(port))
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
static nx_result_t acquire_gate(volatile uint32_t* reg,
                                const nx_stm32h743_gate_lease_config_t* gate) {
    if (gate->mask == 0U)
        return NX_SUCCESS;
    if (gate->added == NULL)
        return NX_ERROR_INVALID;
    *gate->added = ~NX_H743_READ(reg) & gate->mask;
    NX_H743_WRITE(reg, NX_H743_READ(reg) | gate->mask);
    nx_arch_dsb();
    return (NX_H743_READ(reg) & gate->mask) == gate->mask ? NX_SUCCESS
                                                          : NX_ERROR_IO;
}
static nx_result_t release_gate(volatile uint32_t* reg, uint32_t* added) {
    if (added == NULL || *added == 0U)
        return NX_SUCCESS;
    NX_H743_WRITE(reg, NX_H743_READ(reg) & ~*added);
    nx_arch_dsb();
    if ((NX_H743_READ(reg) & *added) != 0U)
        return NX_ERROR_IO;
    *added = 0U;
    return NX_SUCCESS;
}
static void capture(const nx_stm32h743_gpio_lease_config_t* port) {
    nx_stm32h743_gpio_lease_t* receipt = port->receipt;
    GPIO_TypeDef* gpio = port->registers;
    uint32_t pairs = fields(port->pins);
    receipt->mode = NX_H743_READ(&gpio->MODER) & pairs;
    receipt->type = NX_H743_READ(&gpio->OTYPER) & port->pins;
    receipt->speed = NX_H743_READ(&gpio->OSPEEDR) & pairs;
    receipt->pull = NX_H743_READ(&gpio->PUPDR) & pairs;
    receipt->af0 =
        NX_H743_READ(&gpio->AFR[0]) & alternate_fields(port->pins, 0U);
    receipt->af1 =
        NX_H743_READ(&gpio->AFR[1]) & alternate_fields(port->pins, 1U);
    receipt->odr = NX_H743_READ(&gpio->ODR) & port->pins;
    receipt->captured = true;
}
nx_result_t
nx_stm32h743_platform_acquire(const nx_stm32h743_platform_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    /* A failed acquisition or release may retain only gate effects. No pin
     * capture is needed for that responsibility, so test every receipt before
     * replacing any original clock observation. */
    if (*config->gpio_added != 0U ||
        (config->uart_added != NULL && *config->uart_added != 0U) ||
        (config->dma_added != NULL && *config->dma_added != 0U) ||
        (config->apb2.added != NULL && *config->apb2.added != 0U) ||
        (config->apb1.added != NULL && *config->apb1.added != 0U) ||
        (config->ahb1.added != NULL && *config->ahb1.added != 0U) ||
        (config->apb4.added != NULL && *config->apb4.added != 0U))
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->port_count; ++i)
        if (config->ports[i].receipt->captured)
            return NX_ERROR_BUSY;
    nx_result_t result = acquire_gate(&RCC->APB2ENR, &config->apb2);
    if (result != NX_SUCCESS)
        return result;
    result = acquire_gate(&RCC->APB1LENR, &config->apb1);
    if (result != NX_SUCCESS)
        return result;
    result = acquire_gate(&RCC->AHB1ENR, &config->ahb1);
    if (result != NX_SUCCESS)
        return result;
    result = acquire_gate(&RCC->APB4ENR, &config->apb4);
    if (result != NX_SUCCESS)
        return result;
    uint32_t banks = 0U;
    for (size_t i = 0U; i < config->port_count; ++i)
        banks |= 1U << config->ports[i].bank;
    if (config->uart_added != NULL)
        *config->uart_added =
            ~NX_H743_READ(&RCC->APB1LENR) & RCC_APB1LENR_USART3EN;
    if (config->dma_added != NULL) {
        *config->dma_added = ~NX_H743_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN;
        NX_H743_WRITE(&RCC->AHB1ENR,
                      NX_H743_READ(&RCC->AHB1ENR) | RCC_AHB1ENR_DMA1EN);
        nx_arch_dsb();
        result = dma_idle(config->dma_lanes, config->dma_strict, true);
        if (result != NX_SUCCESS)
            return result;
    }
    const nx_stm32h743_gate_lease_config_t gpio = {banks, config->gpio_added};
    result = acquire_gate(&RCC->AHB4ENR, &gpio);
    if (result != NX_SUCCESS)
        return result;
    /* Admit every bank before capturing any bank. A later rejection cannot
     * authorize restoration writes to a previously untouched pad. */
    for (size_t i = 0U; i < config->port_count; ++i)
        if (!pins_available(&config->ports[i]))
            return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->port_count; ++i)
        capture(&config->ports[i]);
    return NX_SUCCESS;
}
static bool restore_field(volatile uint32_t* reg, uint32_t mask,
                          uint32_t value) {
    if (mask == 0U)
        return true;
    NX_H743_WRITE(reg, (NX_H743_READ(reg) & ~mask) | value);
    nx_arch_dsb();
    return (NX_H743_READ(reg) & mask) == value;
}
static nx_result_t restore(const nx_stm32h743_gpio_lease_config_t* port) {
    nx_stm32h743_gpio_lease_t* receipt = port->receipt;
    if (!receipt->captured)
        return NX_SUCCESS;
    /* A powered-down bank cannot provide register readback evidence. Keep the
     * original snapshot and gate receipt until its owner restores access. */
    if ((NX_H743_READ(&RCC->AHB4ENR) & (1U << port->bank)) == 0U)
        return NX_ERROR_STATE;
    GPIO_TypeDef* gpio = port->registers;
    uint32_t pairs = fields(port->pins);
    /* Withdraw drive before touching the latch or mux; restore mode last. */
    if (!restore_field(&gpio->MODER, pairs, pairs))
        return NX_ERROR_IO;
    NX_H743_WRITE(&gpio->BSRR,
                  receipt->odr | ((port->pins & ~receipt->odr) << 16U));
    nx_arch_dsb();
    if ((NX_H743_READ(&gpio->ODR) & port->pins) != receipt->odr ||
        !restore_field(&gpio->AFR[0], alternate_fields(port->pins, 0U),
                       receipt->af0) ||
        !restore_field(&gpio->AFR[1], alternate_fields(port->pins, 1U),
                       receipt->af1) ||
        !restore_field(&gpio->OTYPER, port->pins, receipt->type) ||
        !restore_field(&gpio->OSPEEDR, pairs, receipt->speed) ||
        !restore_field(&gpio->PUPDR, pairs, receipt->pull) ||
        !restore_field(&gpio->MODER, pairs, receipt->mode))
        return NX_ERROR_IO;
    receipt->captured = false;
    return NX_SUCCESS;
}
nx_result_t
nx_stm32h743_platform_release(const nx_stm32h743_platform_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    for (size_t i = config->port_count; i != 0U; --i) {
        nx_result_t result = restore(&config->ports[i - 1U]);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t result = release_gate(&RCC->AHB1ENR, config->dma_added);
    if (result != NX_SUCCESS)
        return result;
    result = release_gate(&RCC->APB1LENR, config->uart_added);
    if (result != NX_SUCCESS)
        return result;
    result = release_gate(&RCC->AHB4ENR, config->gpio_added);
    if (result != NX_SUCCESS)
        return result;
    result = release_gate(&RCC->APB4ENR, config->apb4.added);
    if (result != NX_SUCCESS)
        return result;
    result = release_gate(&RCC->AHB1ENR, config->ahb1.added);
    if (result != NX_SUCCESS)
        return result;
    result = release_gate(&RCC->APB1LENR, config->apb1.added);
    return result == NX_SUCCESS
               ? release_gate(&RCC->APB2ENR, config->apb2.added)
               : result;
}
