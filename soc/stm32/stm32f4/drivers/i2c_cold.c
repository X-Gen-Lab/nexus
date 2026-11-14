/**
 * \file            i2c_cold.c
 * \brief           Concrete I2C pin, clock and publisher ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
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
    return config->registers == I2C1   ? RCC_APB1ENR_I2C1EN
           : config->registers == I2C2 ? RCC_APB1ENR_I2C2EN
                                       : RCC_APB1ENR_I2C3EN;
}

static uint32_t gpio_mask(const nx_stm32_i2c_config_t* config) {
    return (1U << config->pins[0].port_index) |
           (1U << config->pins[1].port_index);
}

static GPIO_TypeDef* pin_bank(const nx_stm32_i2c_config_t* config,
                              size_t index) {
    return config->pins[index].registers;
}

static bool config_valid(const nx_stm32_i2c_config_t* config) {
    if (config == NULL || config->pins == NULL ||
        !nx_stm32_platform_pin_valid(&config->pins[0]) ||
        !nx_stm32_platform_pin_valid(&config->pins[1]) ||
        config->pins[0].snapshot == config->pins[1].snapshot ||
        !nx_i2c_v1_lines_valid(config->lines) ||
        config->clock_hz != NEXUS_APB1_HZ || config->clock_hz < 2000000U ||
        config->clock_hz > 50000000U || config->clock_hz % 1000000U != 0U ||
        config->priority > 15U || config->max_hz < 5129U ||
        config->max_hz > 400000U ||
        (config->registers != I2C1 && config->registers != I2C2 &&
         config->registers != I2C3) ||
        (!config->asynchronous && config->dma != NULL))
        return false;
    for (size_t i = 0U; i < 2U; ++i) {
        if (config->lines->inputs[i] != &config->pins[i].registers->IDR ||
            config->lines->masks[i] != (1U << config->pins[i].pin))
            return false;
    }
    return true;
}

static bool async_config_valid(const nx_stm32_i2c_config_t* config) {
    return config_valid(config) && config->asynchronous &&
           (config->dma == NULL ||
            nx_stm32_i2c_dma_valid(config->registers, config->dma));
}

static size_t vector_count(const nx_stm32_i2c_config_t* config) {
    return config->dma == NULL ? 2U : 4U;
}

static IRQn_Type vector(const nx_stm32_i2c_config_t* config, size_t index) {
    if (index >= 2U)
        return (IRQn_Type)(index == 2U ? config->dma->rx.publisher
                                       : config->dma->tx.publisher);
    if (config->registers == I2C1)
        return index == 0U ? I2C1_EV_IRQn : I2C1_ER_IRQn;
    if (config->registers == I2C2)
        return index == 0U ? I2C2_EV_IRQn : I2C2_ER_IRQn;
    return index == 0U ? I2C3_EV_IRQn : I2C3_ER_IRQn;
}

static bool vector_idle(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&
           NVIC_GetActive(irq) == 0U;
}

static bool withdraw(GPIO_TypeDef* bank, uint8_t pin) {
    uint32_t mask = 3U << (pin * 2U);
    uint32_t mode = bank->MODER & mask;
    if (mode != 0U && mode != mask) {
        NX_PLATFORM_REGISTER_WRITE(bank->MODER, bank->MODER | mask);
        nx_arch_dsb();
        if ((bank->MODER & mask) != mask)
            return false;
    }
    return true;
}

_Static_assert(sizeof(nx_stm32_i2c_cold_receipt_t) == 6U,
               "I2C cold ownership remains six bytes");
/* Two selected bank observations use two receipt bits, including a shared
 * bank. The unchanged pin tuple maps those bits back to exact gate identities.
 * Route expansion therefore adds no mutable receipt storage. */

static nx_result_t
verify_resources(const nx_stm32_i2c_config_t* config,
                 const nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!config_valid(config) || receipt == NULL ||
        (receipt->effects & NX_STM32_I2C_COLD_PREPARED) == 0U)
        return NX_ERROR_INVALID;
    if ((RCC->APB1ENR & gate_mask(config)) == 0U ||
        (RCC->AHB1ENR & gpio_mask(config)) != gpio_mask(config) ||
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
        GPIO_TypeDef* bank = pin_bank(config, receipt->captured - 1U);
        if (!withdraw(bank, pin->pin))
            return NX_ERROR_IO;
        nx_stm32_pin_restore(bank, pin->pin, *pin->snapshot);
        nx_arch_dsb();
        if (nx_stm32_pin_capture(bank, pin->pin) != *pin->snapshot)
            return NX_ERROR_IO;
        --receipt->captured;
    }
    uint32_t gate = gate_mask(config);
    uint32_t original =
        (receipt->effects & NX_STM32_I2C_COLD_PERIPHERAL_CLOCK) != 0U ? gate
                                                                      : 0U;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB1ENR, (RCC->APB1ENR & ~gate) | original);
    nx_arch_dsb();
    if ((RCC->APB1ENR & gate) != original)
        return NX_ERROR_IO;
    original = 0U;
    for (size_t i = 0U; i < 2U; ++i) {
        if ((receipt->effects & (1U << (4U + i))) != 0U)
            original |= 1U << config->pins[i].port_index;
    }
    NX_PLATFORM_REGISTER_WRITE(RCC->AHB1ENR,
                               (RCC->AHB1ENR & ~gpio_mask(config)) | original);
    nx_arch_dsb();
    if ((RCC->AHB1ENR & gpio_mask(config)) != original)
        return NX_ERROR_IO;
    *receipt = (nx_stm32_i2c_cold_receipt_t){0};
    return NX_SUCCESS;
}

static nx_result_t acquire_clocks(const nx_stm32_i2c_config_t* config,
                                  nx_stm32_i2c_cold_receipt_t* receipt) {
    uint32_t gate = gate_mask(config);
    uint32_t gpio = 0U;
    for (size_t i = 0U; i < 2U; ++i) {
        if ((RCC->AHB1ENR & (1U << config->pins[i].port_index)) != 0U)
            gpio |= 1U << (4U + i);
    }
    uint32_t peripheral =
        (RCC->APB1ENR & gate) != 0U ? NX_STM32_I2C_COLD_PERIPHERAL_CLOCK : 0U;
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
    for (size_t i = 0U; i < 2U; ++i) {
        GPIO_TypeDef* bank = pin_bank(config, i);
        uint8_t number = config->pins[i].port_index;
        nx_result_t result = nx_stm32_gpio_clock_enable(number);
        if (result != NX_SUCCESS)
            return result;
        const nx_stm32_i2c_pin_config_t* pin = &config->pins[i];
        *pin->snapshot = nx_stm32_pin_capture(bank, pin->pin);
        receipt->captured = (uint8_t)(i + 1U);
        result = nx_stm32_pin_configure(bank, pin->pin, 2U, 4U, 0U, true);
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
    if (!async_config_valid(config))
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
    if (!async_config_valid(config) || receipt == NULL)
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
    if (!async_config_valid(config) || receipt == NULL)
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
