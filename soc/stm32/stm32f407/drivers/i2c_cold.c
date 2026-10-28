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
    return config->dma == NULL ? 2U : 4U;
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

_Static_assert(sizeof(nx_stm32_i2c_cold_receipt_t) <= 8U,
               "I2C cold ownership must not exceed eight bytes");
/* Encode the two reviewed APB1 bits directly. Full-width register accesses
 * remain masked to their selected gate; the compact receipt is not MMIO. */
_Static_assert(((RCC_APB1ENR_I2C1EN | RCC_APB1ENR_I2C2EN) >> 20U) ==
                   NX_STM32_I2C_COLD_PERIPHERAL_CLOCK,
               "I2C gate receipt encoding");
_Static_assert((RCC_AHB1ENR_GPIOBEN << 2U) == NX_STM32_I2C_COLD_GPIO_CLOCK,
               "GPIO gate receipt encoding");

static nx_result_t
verify_resources(const nx_stm32_i2c_config_t* config,
                 const nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!config_valid(config) || receipt == NULL ||
        (receipt->effects & NX_STM32_I2C_COLD_PREPARED) == 0U)
        return NX_ERROR_INVALID;
    if ((RCC->APB1ENR & gate_mask(config)) == 0U ||
        (RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN) == 0U ||
        (config->registers->CR1 & I2C_CR1_PE) == 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

/* The shared pad/gate phases never reference publishers. Polling therefore
 * retains no NVIC algorithm when unused asynchronous sections are collected. */
static nx_result_t release_resources(const nx_stm32_i2c_config_t* config,
                                     nx_stm32_i2c_cold_receipt_t* receipt) {
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
    uint32_t original =
        ((uint32_t)receipt->effects & NX_STM32_I2C_COLD_PERIPHERAL_CLOCK)
        << 20U;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB1ENR, (RCC->APB1ENR & ~gate) | original);
    nx_arch_dsb();
    if ((RCC->APB1ENR & gate) != original)
        return NX_ERROR_IO;
    original =
        ((uint32_t)receipt->effects & NX_STM32_I2C_COLD_GPIO_CLOCK) >> 2U;
    NX_PLATFORM_REGISTER_WRITE(
        RCC->AHB1ENR, (RCC->AHB1ENR & ~RCC_AHB1ENR_GPIOBEN) | original);
    nx_arch_dsb();
    if ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN) != original)
        return NX_ERROR_IO;
    *receipt = (nx_stm32_i2c_cold_receipt_t){0};
    return NX_SUCCESS;
}

static nx_result_t acquire_clocks(const nx_stm32_i2c_config_t* config,
                                  nx_stm32_i2c_cold_receipt_t* receipt) {
    uint32_t gate = gate_mask(config);
    uint32_t gpio = (RCC->AHB1ENR & (uint32_t)RCC_AHB1ENR_GPIOBEN) << 2U;
    uint32_t peripheral = (RCC->APB1ENR & gate) >> 20U;
    receipt->effects = (uint8_t)(NX_STM32_I2C_COLD_OWNED | gpio | peripheral);
    NX_PLATFORM_REGISTER_WRITE(RCC->APB1ENR, RCC->APB1ENR | gate);
    nx_arch_dsb();
    if ((RCC->APB1ENR & gate) == 0U)
        return NX_ERROR_IO;
    return (config->registers->CR1 & I2C_CR1_PE) == 0U ? NX_SUCCESS
                                                       : NX_ERROR_BUSY;
}

static nx_result_t acquire_pads(const nx_stm32_i2c_config_t* config,
                                nx_stm32_i2c_cold_receipt_t* receipt) {
    nx_result_t result = nx_stm32_gpio_clock_enable(1U);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < 2U; ++i) {
        const nx_stm32_i2c_pin_config_t* pin = &config->pins[i];
        *pin->snapshot = nx_stm32_pin_capture(GPIOB, pin->pin);
        receipt->captured = (uint8_t)(i + 1U);
        result = nx_stm32_pin_configure(GPIOB, pin->pin, 2U, 4U, 0U, true);
        if (result != NX_SUCCESS)
            return result;
    }
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_i2c_cold_verify(const nx_stm32_i2c_config_t* config,
                         const nx_stm32_i2c_cold_receipt_t* receipt) {
    if (config == NULL || config->asynchronous)
        return NX_ERROR_INVALID;
    return verify_resources(config, receipt);
}

nx_result_t nx_stm32_i2c_cold_release(const nx_stm32_i2c_config_t* config,
                                      nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || config->asynchronous || receipt == NULL)
        return NX_ERROR_INVALID;
    if ((receipt->effects & NX_STM32_I2C_COLD_OWNED) == 0U)
        return NX_SUCCESS;
    return release_resources(config, receipt);
}

nx_result_t nx_stm32_i2c_cold_acquire(const nx_stm32_i2c_config_t* config,
                                      nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || config->asynchronous || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects != 0U || receipt->captured != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = acquire_clocks(config, receipt);
    if (result == NX_SUCCESS)
        result = acquire_pads(config, receipt);
    if (result != NX_SUCCESS)
        (void)nx_stm32_i2c_cold_release(config, receipt);
    return result;
}

nx_result_t
nx_stm32_i2c_async_cold_verify(const nx_stm32_i2c_config_t* config,
                               const nx_stm32_i2c_cold_receipt_t* receipt) {
    if (config == NULL || !config->asynchronous)
        return NX_ERROR_INVALID;
    nx_result_t result = verify_resources(config, receipt);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < vector_count(config); ++i) {
        IRQn_Type irq = vector(config, i);
        if (NVIC_GetEnableIRQ(irq) == 0U ||
            NVIC->IPR[(uint32_t)irq] != (uint8_t)(config->priority << 4U))
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_i2c_async_cold_release(const nx_stm32_i2c_config_t* config,
                                nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || !config->asynchronous || receipt == NULL)
        return NX_ERROR_INVALID;
    if ((receipt->effects & NX_STM32_I2C_COLD_OWNED) == 0U)
        return NX_SUCCESS;
    if ((receipt->effects & NX_STM32_I2C_COLD_VECTORS) != 0U) {
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
        receipt->effects &= (uint8_t)~NX_STM32_I2C_COLD_VECTORS;
    }
    return release_resources(config, receipt);
}

nx_result_t
nx_stm32_i2c_async_cold_acquire(const nx_stm32_i2c_config_t* config,
                                nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config) || !config->asynchronous || receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->effects != 0U || receipt->captured != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < vector_count(config); ++i) {
        if (!vector_idle(vector(config, i)))
            return NX_ERROR_BUSY;
    }
    nx_result_t result = acquire_clocks(config, receipt);
    if (result == NX_SUCCESS) {
        for (size_t i = 0U; i < vector_count(config); ++i)
            receipt->priorities[i] = NVIC->IPR[(uint32_t)vector(config, i)];
        receipt->effects |= NX_STM32_I2C_COLD_VECTORS;
        result = acquire_pads(config, receipt);
    }
    if (result != NX_SUCCESS)
        (void)nx_stm32_i2c_async_cold_release(config, receipt);
    return result;
}
