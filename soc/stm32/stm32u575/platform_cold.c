/**
 * \file            platform_cold.c
 * \brief           U575 selected peripheral admission and exact GPIO rollback
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_platform_cold.h"

static bool context_valid(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool selected(const nx_stm32u575_platform_config_t* config,
                     uint32_t selection) {
    return (config->selected & selection) != 0U;
}
static uint32_t apb1_mask(const nx_stm32u575_platform_config_t* config) {
    return (selected(config, NX_U575_SELECT_USART2) ? RCC_APB1ENR1_USART2EN
                                                    : 0U) |
           (selected(config, NX_U575_SELECT_TIM3) ? RCC_APB1ENR1_TIM3EN : 0U) |
           (selected(config, NX_U575_SELECT_I2C1) ? RCC_APB1ENR1_I2C1EN : 0U);
}
static uint32_t apb2_mask(const nx_stm32u575_platform_config_t* config) {
    return (selected(config, NX_U575_SELECT_USART1) ? RCC_APB2ENR_USART1EN
                                                    : 0U) |
           (selected(config, NX_U575_SELECT_SPI1) ? RCC_APB2ENR_SPI1EN : 0U);
}
static uint32_t ahb3_mask(const nx_stm32u575_platform_config_t* config) {
    return selected(config, NX_U575_SELECT_ADC4) ? RCC_AHB3ENR_ADC4EN : 0U;
}
static bool config_valid(const nx_stm32u575_platform_config_t* config) {
    if (config == NULL || (config->selected & ~UINT32_C(0x7FF)) != 0U ||
        (config->lease_count != 0U &&
         (config->leases == NULL || config->gpio_added == NULL)) ||
        (apb1_mask(config) != 0U && config->apb1_added == NULL) ||
        (apb2_mask(config) != 0U && config->apb2_added == NULL) ||
        (ahb3_mask(config) != 0U && config->ahb3_added == NULL))
        return false;
    uint32_t seen = 0U;
    for (size_t i = 0U; i < config->lease_count; ++i) {
        const nx_stm32u575_gpio_lease_t* lease = &config->leases[i];
        if (lease->registers == NULL || lease->receipt == NULL ||
            lease->pins == 0U || lease->bank > 7U ||
            (seen & (1U << lease->bank)) != 0U)
            return false;
        for (size_t j = 0U; j < i; ++j)
            if (lease->registers == config->leases[j].registers ||
                lease->receipt == config->leases[j].receipt)
                return false;
        seen |= 1U << lease->bank;
    }
    return true;
}
static uint32_t mode_mask(uint16_t pins) {
    uint32_t mask = 0U;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if (((uint32_t)pins & (1U << pin)) != 0U)
            mask |= 3U << (pin * 2U);
    return mask;
}
static uint32_t alternate_mask(uint16_t pins, unsigned bank) {
    uint32_t mask = 0U;
    for (unsigned pin = 0U; pin < 8U; ++pin)
        if (((uint32_t)pins & (1U << (pin + bank * 8U))) != 0U)
            mask |= 15U << (pin * 4U);
    return mask;
}
static bool pins_available(const nx_stm32u575_gpio_lease_t* lease) {
    GPIO_TypeDef* gpio = lease->registers;
    if ((NX_U575_READ(&gpio->LCKR) & lease->pins) != 0U)
        return false;
    uint32_t mode = NX_U575_READ(&gpio->MODER);
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if (((uint32_t)lease->pins & (1U << pin)) != 0U) {
            uint32_t value = (mode >> (pin * 2U)) & 3U;
            if (value != 0U && value != 3U)
                return false;
        }
    }
    return true;
}
static bool lptim_cold(void) {
    if ((NX_U575_READ(&RCC->APB1RSTR2) & RCC_APB1RSTR2_LPTIM2RST) != 0U ||
        (NX_U575_READ(&RCC->CCIPR1) & RCC_CCIPR1_LPTIM2SEL) != 0U ||
        !NX_U575_IRQ_AVAILABLE(LPTIM2_IRQn))
        return false;
    if ((NX_U575_READ(&RCC->APB1ENR2) & RCC_APB1ENR2_LPTIM2EN) == 0U)
        return true;
    return NX_U575_READ(&LPTIM2->CR) == 0U &&
           NX_U575_READ(&LPTIM2->CFGR) == 0U &&
           NX_U575_READ(&LPTIM2->CFGR2) == 0U &&
           NX_U575_READ(&LPTIM2->DIER) == 0U &&
           NX_U575_READ(&LPTIM2->ISR) == 0U &&
           NX_U575_READ(&LPTIM2->CCMR1) == 0U &&
           NX_U575_READ(&LPTIM2->RCR) == 0U &&
           NX_U575_READ(&LPTIM2->CNT) == 0U &&
           NX_U575_READ(&LPTIM2->CCR1) == 0U &&
           NX_U575_READ(&LPTIM2->CCR2) == 0U &&
           NX_U575_READ(&LPTIM2->ARR) == 1U;
}
nx_result_t nx_stm32u575_uart_preflight(uint8_t controller, bool dma) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    if (controller != 1U && controller != 2U)
        return NX_ERROR_INVALID;
    uint32_t gate = controller == 1U ? NX_U575_READ(&RCC->APB2ENR)
                                     : NX_U575_READ(&RCC->APB1ENR1);
    uint32_t mask =
        controller == 1U ? RCC_APB2ENR_USART1EN : RCC_APB1ENR1_USART2EN;
    uint32_t mux =
        controller == 1U ? RCC_CCIPR1_USART1SEL : RCC_CCIPR1_USART2SEL;
    if ((gate & mask) != 0U || (NX_U575_READ(&RCC->CCIPR1) & mux) != 0U ||
        !NX_U575_IRQ_AVAILABLE(controller == 1U ? USART1_IRQn : USART2_IRQn) ||
        (dma && !NX_U575_IRQ_AVAILABLE(GPDMA1_Channel0_IRQn)))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_spi_preflight(void) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    return (NX_U575_READ(&RCC->APB2ENR) & RCC_APB2ENR_SPI1EN) != 0U ||
                   (NX_U575_READ(&RCC->CCIPR1) & RCC_CCIPR1_SPI1SEL) != 0U
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}
nx_result_t nx_stm32u575_pwm_preflight(void) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    return (NX_U575_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_TIM3EN) != 0U
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}
nx_result_t nx_stm32u575_i2c_preflight(void) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    if ((NX_U575_READ(&RCC->APB1ENR1) & RCC_APB1ENR1_I2C1EN) != 0U ||
        (NX_U575_READ(&RCC->CCIPR1) & RCC_CCIPR1_I2C1SEL) != 0U ||
        (NX_U575_READ(&RCC->CR) &
         (RCC_CR_HSION | RCC_CR_HSIKERON | RCC_CR_HSIRDY)) != 0U ||
        (NX_U575_READ(&RCC->ICSCR3) & RCC_ICSCR3_HSITRIM) !=
            RCC_ICSCR3_HSITRIM_4)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_adc_preflight(void) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    if ((NX_U575_READ(&RCC->AHB3ENR) & RCC_AHB3ENR_ADC4EN) != 0U ||
        (NX_U575_READ(&RCC->CCIPR3) & RCC_CCIPR3_ADCDACSEL) != 0U ||
        (NX_U575_READ(&RCC->AHB2ENR1) & RCC_AHB2ENR1_ADC12EN) != 0U ||
        (NX_U575_READ(&RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0U ||
        (NX_U575_READ(&NX_U575_PWR->SVMCR) & PWR_SVMCR_ASV) == 0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_flash_preflight(void) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    return NX_U575_IRQ_AVAILABLE(FLASH_IRQn) ? NX_SUCCESS : NX_ERROR_BUSY;
}
nx_result_t nx_stm32u575_exti_preflight(uint8_t line) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    if (line > 1U)
        return NX_ERROR_INVALID;
    if (!NX_U575_IRQ_AVAILABLE(line == 0U ? EXTI0_IRQn : EXTI1_IRQn) ||
        ((NX_U575_READ(&EXTI->IMR1) | NX_U575_READ(&EXTI->EMR1) |
          NX_U575_READ(&EXTI->RTSR1) | NX_U575_READ(&EXTI->FTSR1) |
          NX_U575_READ(&EXTI->RPR1) | NX_U575_READ(&EXTI->FPR1)) &
         (1U << line)) != 0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_lptim_preflight(void) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    return lptim_cold() ? NX_SUCCESS : NX_ERROR_BUSY;
}
nx_result_t
nx_stm32u575_platform_preflight(const nx_stm32u575_platform_config_t* config) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < config->lease_count; ++i) {
        const nx_stm32u575_gpio_lease_t* lease = &config->leases[i];
        if (lease->bank == 6U && (lease->pins & ~3U) != 0U &&
            ((NX_U575_READ(&RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0U ||
             (NX_U575_READ(&NX_U575_PWR->SVMCR) & PWR_SVMCR_IO2SV) == 0U))
            return NX_ERROR_BUSY;
        if ((NX_U575_READ(&RCC->AHB2ENR1) & (1U << lease->bank)) != 0U &&
            !pins_available(lease))
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
static void capture(const nx_stm32u575_gpio_lease_t* lease) {
    GPIO_TypeDef* gpio = lease->registers;
    nx_stm32u575_gpio_receipt_t* state = lease->receipt;
    uint32_t fields = mode_mask(lease->pins);
    state->mode = NX_U575_READ(&gpio->MODER) & fields;
    state->type = NX_U575_READ(&gpio->OTYPER) & lease->pins;
    state->speed = NX_U575_READ(&gpio->OSPEEDR) & fields;
    state->pull = NX_U575_READ(&gpio->PUPDR) & fields;
    state->af0 = NX_U575_READ(&gpio->AFR[0]) & alternate_mask(lease->pins, 0U);
    state->af1 = NX_U575_READ(&gpio->AFR[1]) & alternate_mask(lease->pins, 1U);
    state->odr = NX_U575_READ(&gpio->ODR) & lease->pins;
    state->captured = true;
}
static nx_result_t acquire_gate(volatile uint32_t* gate, uint32_t mask) {
    if (mask == 0U)
        return NX_SUCCESS;
    NX_U575_WRITE(gate, NX_U575_READ(gate) | mask);
    nx_arch_dsb();
    return (NX_U575_READ(gate) & mask) == mask ? NX_SUCCESS : NX_ERROR_IO;
}
nx_result_t
nx_stm32u575_platform_acquire(const nx_stm32u575_platform_config_t* config) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    /* A failed gate release remains owned even after all pins are restored. */
    if ((config->gpio_added != NULL && *config->gpio_added != 0U) ||
        (config->apb1_added != NULL && *config->apb1_added != 0U) ||
        (config->apb2_added != NULL && *config->apb2_added != 0U) ||
        (config->ahb3_added != NULL && *config->ahb3_added != 0U))
        return NX_ERROR_BUSY;
    if (config->lease_count == 0U)
        return NX_SUCCESS;
    uint32_t gpio_mask = 0U;
    for (size_t i = 0U; i < config->lease_count; ++i) {
        if (config->leases[i].receipt->captured)
            return NX_ERROR_BUSY;
        gpio_mask |= 1U << config->leases[i].bank;
    }
    *config->gpio_added = ~NX_U575_READ(&RCC->AHB2ENR1) & gpio_mask;
    if (config->apb1_added != NULL)
        *config->apb1_added = ~NX_U575_READ(&RCC->APB1ENR1) & apb1_mask(config);
    if (config->apb2_added != NULL)
        *config->apb2_added = ~NX_U575_READ(&RCC->APB2ENR) & apb2_mask(config);
    if (config->ahb3_added != NULL)
        *config->ahb3_added = ~NX_U575_READ(&RCC->AHB3ENR) & ahb3_mask(config);
    nx_result_t result = acquire_gate(&RCC->AHB2ENR1, gpio_mask);
    if (result != NX_SUCCESS)
        return result;
    /* Admit every bank before taking the first reversible snapshot. */
    for (size_t i = 0U; i < config->lease_count; ++i)
        if (!pins_available(&config->leases[i]))
            return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->lease_count; ++i)
        capture(&config->leases[i]);
    result = acquire_gate(&RCC->AHB3ENR, ahb3_mask(config));
    if (result == NX_SUCCESS)
        result = acquire_gate(&RCC->APB1ENR1, apb1_mask(config));
    if (result == NX_SUCCESS)
        result = acquire_gate(&RCC->APB2ENR, apb2_mask(config));
    return result;
}
static bool restore_field(volatile uint32_t* field, uint32_t mask,
                          uint32_t value) {
    if (mask == 0U)
        return true;
    NX_U575_WRITE(field, (NX_U575_READ(field) & ~mask) | value);
    nx_arch_dsb();
    return (NX_U575_READ(field) & mask) == value;
}
static nx_result_t restore(const nx_stm32u575_gpio_lease_t* lease) {
    nx_stm32u575_gpio_receipt_t* state = lease->receipt;
    if (!state->captured)
        return NX_SUCCESS;
    /* No pad readback is evidence while the bank is inaccessible. Retain
     * the snapshot and gate responsibility for an explicit powered retry. */
    if ((NX_U575_READ(&RCC->AHB2ENR1) & (1U << lease->bank)) == 0U)
        return NX_ERROR_STATE;
    GPIO_TypeDef* gpio = lease->registers;
    uint32_t fields = mode_mask(lease->pins);
    if (!restore_field(&gpio->MODER, fields, fields))
        return NX_ERROR_IO;
    NX_U575_WRITE(&gpio->BSRR,
                  state->odr | (((uint32_t)lease->pins & ~state->odr) << 16U));
    nx_arch_dsb();
    if ((NX_U575_READ(&gpio->ODR) & lease->pins) != state->odr ||
        !restore_field(&gpio->AFR[0], alternate_mask(lease->pins, 0U),
                       state->af0) ||
        !restore_field(&gpio->AFR[1], alternate_mask(lease->pins, 1U),
                       state->af1) ||
        !restore_field(&gpio->OTYPER, lease->pins, state->type) ||
        !restore_field(&gpio->OSPEEDR, fields, state->speed) ||
        !restore_field(&gpio->PUPDR, fields, state->pull) ||
        !restore_field(&gpio->MODER, fields, state->mode))
        return NX_ERROR_IO;
    state->captured = false;
    return NX_SUCCESS;
}
static nx_result_t release_gate(volatile uint32_t* gate, uint32_t* added) {
    if (added == NULL || *added == 0U)
        return NX_SUCCESS;
    NX_U575_WRITE(gate, NX_U575_READ(gate) & ~*added);
    nx_arch_dsb();
    if ((NX_U575_READ(gate) & *added) != 0U)
        return NX_ERROR_IO;
    *added = 0U;
    return NX_SUCCESS;
}
nx_result_t
nx_stm32u575_platform_release(const nx_stm32u575_platform_config_t* config) {
    if (!context_valid())
        return NX_ERROR_CONTEXT;
    if (!config_valid(config))
        return NX_ERROR_INVALID;
    for (size_t i = config->lease_count; i > 0U; --i) {
        nx_result_t result = restore(&config->leases[i - 1U]);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t result = release_gate(&RCC->AHB3ENR, config->ahb3_added);
    if (result == NX_SUCCESS)
        result = release_gate(&RCC->APB1ENR1, config->apb1_added);
    if (result == NX_SUCCESS)
        result = release_gate(&RCC->APB2ENR, config->apb2_added);
    if (result == NX_SUCCESS)
        result = release_gate(&RCC->AHB2ENR1, config->gpio_added);
    return result;
}
