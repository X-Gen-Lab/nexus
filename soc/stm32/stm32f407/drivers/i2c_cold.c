/**
 * \file            i2c_cold.c
 * \brief           Concrete I2C pin, clock and publisher ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_i2c_cold.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_PLATFORM_PRIORITY_WRITE
#define NX_PLATFORM_PRIORITY_WRITE(reg, value) ((reg) = (uint8_t)(value))
#endif

static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static uint32_t gate_mask(const nx_stm32_i2c_config_t* config) {
    return config->registers == I2C1 ? RCC_APB1ENR_I2C1EN : RCC_APB1ENR_I2C2EN;
}

static bool config_valid(const nx_stm32_i2c_config_t* config) {
    if (config == NULL || config->pins == NULL ||
        config->pins[0].snapshot == NULL || config->pins[1].snapshot == NULL ||
        config->pins[0].snapshot == config->pins[1].snapshot ||
        config->clock_hz != 42000000U || config->priority > 15U ||
        (config->registers != I2C1 && config->registers != I2C2) ||
        (!config->asynchronous &&
         (config->registers != I2C1 || config->dma != NULL)))
        return false;
    uint8_t first = config->pins[0].pin;
    uint8_t second = config->pins[1].pin;
    return config->registers == I2C1 ? ((first == 6U && second == 7U) ||
                                        (first == 8U && second == 9U))
                                     : (first == 10U && second == 11U);
}

static size_t vector_count(const nx_stm32_i2c_config_t* config) {
    return !config->asynchronous ? 0U : config->dma == NULL ? 2U : 4U;
}

static IRQn_Type vector(const nx_stm32_i2c_config_t* config, size_t index) {
    if (config->registers == I2C1) {
        const IRQn_Type values[] = {I2C1_EV_IRQn, I2C1_ER_IRQn,
                                    DMA1_Stream0_IRQn, DMA1_Stream6_IRQn};
        return values[index];
    }
    const IRQn_Type values[] = {I2C2_EV_IRQn, I2C2_ER_IRQn, DMA1_Stream2_IRQn,
                                DMA1_Stream7_IRQn};
    return values[index];
}

static bool vector_idle(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&
           NVIC_GetActive(irq) == 0U;
}

static bool withdraw(uint8_t pin) {
    uint32_t mask = 3U << (pin * 2U);
    uint32_t mode = GPIOB->MODER & mask;
    if (mode != 0U && mode != mask) {
        NX_PLATFORM_REGISTER_WRITE(GPIOB->MODER, GPIOB->MODER | mask);
        nx_arch_dsb();
        if ((GPIOB->MODER & mask) != mask)
            return false;
    }
    return true;
}

nx_result_t
nx_stm32_i2c_cold_verify(const nx_stm32_i2c_config_t* config,
                         const nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!config_valid(config) || receipt == NULL || !receipt->prepared)
        return NX_ERROR_INVALID;
    if ((RCC->APB1ENR & gate_mask(config)) == 0U ||
        (RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN) == 0U ||
        (config->registers->CR1 & I2C_CR1_PE) == 0U)
        return NX_ERROR_IO;
    for (size_t i = 0U; i < vector_count(config); ++i) {
        IRQn_Type irq = vector(config, i);
        if (NVIC_GetEnableIRQ(irq) == 0U ||
            NVIC->IPR[(uint32_t)irq] != (uint8_t)(config->priority << 4U))
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

nx_result_t nx_stm32_i2c_cold_release(const nx_stm32_i2c_config_t* config,
                                      nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (!receipt->owned)
        return NX_SUCCESS;
    if (receipt->vectors) {
        for (size_t i = 0U; i < vector_count(config); ++i) {
            IRQn_Type irq = vector(config, i);
            uint32_t index = (uint32_t)irq;
            uint32_t bit = 1U << (index & 31U);
            NX_PLATFORM_REGISTER_WRITE(NVIC->ICER[index >> 5U], bit);
            NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[index >> 5U], bit);
            nx_arch_dsb();
            if (!vector_idle(irq))
                return NX_ERROR_BUSY;
            NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[index],
                                       receipt->priorities[i]);
            nx_arch_dsb();
            if (NVIC->IPR[index] != receipt->priorities[i])
                return NX_ERROR_IO;
        }
        receipt->vectors = false;
    }
    while (receipt->captured != 0U) {
        const nx_stm32_i2c_pin_config_t* pin =
            &config->pins[receipt->captured - 1U];
        if (!withdraw(pin->pin))
            return NX_ERROR_IO;
        nx_stm32_pin_restore(GPIOB, pin->pin, *pin->snapshot);
        nx_arch_dsb();
        if (nx_stm32_pin_capture(GPIOB, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        --receipt->captured;
    }
    uint32_t gate = gate_mask(config);
    NX_PLATFORM_REGISTER_WRITE(RCC->APB1ENR, (RCC->APB1ENR & ~gate) |
                                                 receipt->peripheral_clock);
    nx_arch_dsb();
    if ((RCC->APB1ENR & gate) != receipt->peripheral_clock)
        return NX_ERROR_IO;
    NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR,
                               (RCC->AHB1ENR & ~RCC_AHB1ENR_GPIOBEN) |
                                   receipt->gpio_clock);
    nx_arch_dsb();
    if ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN) != receipt->gpio_clock)
        return NX_ERROR_IO;
    *receipt = (nx_stm32_i2c_cold_receipt_t){0};
    return NX_SUCCESS;
}

nx_result_t nx_stm32_i2c_cold_acquire(const nx_stm32_i2c_config_t* config,
                                      nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->owned)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < vector_count(config); ++i) {
        if (!vector_idle(vector(config, i)))
            return NX_ERROR_BUSY;
    }
    uint32_t gate = gate_mask(config);
    receipt->gpio_clock = RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN;
    receipt->peripheral_clock = RCC->APB1ENR & gate;
    receipt->owned = true;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB1ENR, RCC->APB1ENR | gate);
    nx_arch_dsb();
    nx_result_t result = NX_ERROR_IO;
    if ((RCC->APB1ENR & gate) == 0U)
        goto failed;
    result = NX_ERROR_BUSY;
    if ((config->registers->CR1 & I2C_CR1_PE) != 0U)
        goto failed;
    for (size_t i = 0U; i < vector_count(config); ++i)
        receipt->priorities[i] = NVIC->IPR[(uint32_t)vector(config, i)];
    receipt->vectors = config->asynchronous;
    result = nx_stm32_gpio_clock_enable(1U);
    if (result != NX_SUCCESS)
        goto failed;
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_stm32_i2c_pin_config_t* pin = &config->pins[i];
        *pin->snapshot = nx_stm32_pin_capture(GPIOB, pin->pin);
        receipt->captured = (uint8_t)(i + 1U);
        result = nx_stm32_pin_configure(GPIOB, pin->pin, 2U, 4U, 0U, true);
        if (result != NX_SUCCESS)
            goto failed;
    }
    return NX_SUCCESS;
failed:
    (void)nx_stm32_i2c_cold_release(config, receipt);
    return result;
}
