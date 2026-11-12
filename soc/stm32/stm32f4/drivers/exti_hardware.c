/**
 * \file            exti_hardware.c
 * \brief           Queue-free STM32F407 EXTI hardware-event ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_exti_hardware.h"
#include "stm32f4xx_ll_gpio.h"

#ifndef NX_EXTI_HARDWARE_WRITE
#define NX_EXTI_HARDWARE_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_EXTI_HARDWARE_CLEAR
#define NX_EXTI_HARDWARE_CLEAR(reg, mask) (*(reg) = (uint32_t)(mask))
#endif

/** \brief Serialize cold transitions without changing incoming CPU context. */
static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Match exact pad facts before reading or writing its GPIO bank. */
static bool valid_config(const nx_stm32_exti_hardware_config_t* config) {
    return config != NULL && config->line <= 15U && config->gpio_index <= 8U &&
           config->pull <= 2U && config->edge >= NX_EXTI_RISING &&
           config->edge <= NX_EXTI_BOTH &&
           config->gpio ==
               (GPIO_TypeDef*)(uintptr_t)(GPIOA_BASE +
                                          (uintptr_t)config->gpio_index *
                                              UINT32_C(0x400));
}

/** \brief Establish one selected register field before publishing ownership. */
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_EXTI_HARDWARE_WRITE(reg, value);
    nx_arch_dsb();
    return (*reg & mask) == (value & mask);
}

/** \brief Refuse live line owners and the ES0182 PC13/LSE coupling. */
nx_result_t
nx_stm32_exti_hardware_admit(const nx_stm32_exti_hardware_state_t* state,
                             const nx_stm32_exti_hardware_config_t* config) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || !valid_config(config))
        return NX_ERROR_INVALID;
    if (state->owned || state->initialized || state->config != NULL)
        return NX_ERROR_BUSY;
    uint32_t bit = 1U << config->line;
    if (((EXTI->IMR | EXTI->EMR | EXTI->PR | EXTI->RTSR | EXTI->FTSR) & bit) !=
            0U ||
        (config->gpio == GPIOC && config->line == 13U &&
         (RCC->BDCR & RCC_BDCR_LSEBYP) == 0U &&
         (RCC->BDCR & (RCC_BDCR_LSEON | RCC_BDCR_LSERDY)) != 0U))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

/** \brief Configure the exact input/mux/triggers, enabling EMR last. */
nx_result_t
nx_stm32_exti_hardware_prepare(nx_stm32_exti_hardware_state_t* state,
                               const nx_stm32_exti_hardware_config_t* config) {
    nx_result_t result = nx_stm32_exti_hardware_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    if ((RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U ||
        (RCC->AHB1ENR & (1U << config->gpio_index)) == 0U)
        return NX_ERROR_STATE;
    uint32_t bit = 1U << config->line;
    uint32_t mode = LL_GPIO_GetPinMode(config->gpio, bit);
    if ((mode != LL_GPIO_MODE_INPUT && mode != LL_GPIO_MODE_ANALOG) ||
        (config->gpio->LCKR & bit) != 0U)
        return NX_ERROR_BUSY;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    volatile uint32_t* mux = &SYSCFG->EXTICR[config->line / 4U];
    uint32_t shift = (uint32_t)(config->line % 4U) * 4U;
    uint32_t mask = 15U << shift;
    state->saved_mux = (uint8_t)((*mux >> shift) & 15U);
    state->config = config;
    state->owned = true;
    LL_GPIO_SetPinPull(config->gpio, bit, config->pull);
    LL_GPIO_SetPinMode(config->gpio, bit, LL_GPIO_MODE_INPUT);
    nx_arch_dsb();
    bool valid = LL_GPIO_GetPinPull(config->gpio, bit) == config->pull &&
                 LL_GPIO_GetPinMode(config->gpio, bit) == LL_GPIO_MODE_INPUT;
    uint32_t rising = (config->edge & NX_EXTI_RISING) != 0 ? bit : 0U;
    uint32_t falling = (config->edge & NX_EXTI_FALLING) != 0 ? bit : 0U;
    if (valid)
        valid = write_checked(
            mux, (*mux & ~mask) | ((uint32_t)config->gpio_index << shift),
            mask);
    if (valid)
        valid = write_checked(&EXTI->RTSR, (EXTI->RTSR & ~bit) | rising, bit);
    if (valid)
        valid = write_checked(&EXTI->FTSR, (EXTI->FTSR & ~bit) | falling, bit);
    if (valid) {
        NX_EXTI_HARDWARE_CLEAR(&EXTI->PR, bit);
        nx_arch_dsb();
        valid = (EXTI->PR & bit) == 0U;
    }
    if (valid)
        valid = write_checked(&EXTI->EMR, EXTI->EMR | bit, bit);
    state->initialized = valid;
    nx_arch_irq_restore(saved);
    return valid ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Retain config and mux after any refused source withdrawal. */
nx_result_t
nx_stm32_exti_hardware_close(nx_stm32_exti_hardware_state_t* state) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!state->owned)
        return state->initialized ? NX_ERROR_STATE : NX_SUCCESS;
    const nx_stm32_exti_hardware_config_t* config = state->config;
    if (!valid_config(config))
        return NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint32_t bit = 1U << config->line;
    bool valid = write_checked(&EXTI->EMR, EXTI->EMR & ~bit, bit);
    if (valid)
        valid = write_checked(&EXTI->RTSR, EXTI->RTSR & ~bit, bit);
    if (valid)
        valid = write_checked(&EXTI->FTSR, EXTI->FTSR & ~bit, bit);
    if (valid) {
        NX_EXTI_HARDWARE_CLEAR(&EXTI->PR, bit);
        nx_arch_dsb();
        valid = ((EXTI->PR | EXTI->IMR) & bit) == 0U;
    }
    if (valid) {
        volatile uint32_t* mux = &SYSCFG->EXTICR[config->line / 4U];
        uint32_t shift = (uint32_t)(config->line % 4U) * 4U;
        uint32_t mask = 15U << shift;
        valid = write_checked(
            mux, (*mux & ~mask) | ((uint32_t)state->saved_mux << shift), mask);
    }
    if (valid)
        *state = (nx_stm32_exti_hardware_state_t){0};
    nx_arch_irq_restore(saved);
    return valid ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Match one output byte against a live owner, config or MMIO object. */
static bool aliases(const bool* output, const void* object, size_t size) {
    uintptr_t address = (uintptr_t)output;
    uintptr_t first = (uintptr_t)object;
    return address >= first && address - first < size;
}

/** \brief Observe the pad without consuming an event or changing its source. */
static nx_result_t level(const void* opaque, bool* high) {
    const nx_stm32_exti_hardware_state_t* state = opaque;
    if (state == NULL || high == NULL)
        return NX_ERROR_INVALID;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!state->owned || !state->initialized)
        return NX_ERROR_STATE;
    const nx_stm32_exti_hardware_config_t* config = state->config;
    if (!valid_config(config) || aliases(high, state, sizeof(*state)) ||
        aliases(high, config, sizeof(*config)) ||
        aliases(high, config->gpio, sizeof(*config->gpio)) ||
        aliases(high, EXTI, sizeof(*EXTI)) ||
        aliases(high, SYSCFG, sizeof(*SYSCFG)) ||
        aliases(high, RCC, sizeof(*RCC)))
        return NX_ERROR_INVALID;
    *high = (LL_GPIO_ReadInputPort(config->gpio) & (1U << config->line)) != 0U;
    return NX_SUCCESS;
}

/** \brief Keep the public optional stop method on the same concrete owner. */
static nx_result_t stop(void* opaque) {
    return nx_stm32_exti_hardware_close(opaque);
}

const nx_exti_ops_t nx_stm32_exti_hardware_ops = {
    .read = NULL,
    .stop = stop,
    .attach_wake = NULL,
    .level = level,
};
