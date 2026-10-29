/**
 * \file            platform_cold.c
 * \brief           Exact G474 Platform resource admission and retained cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_acquisition.h"
#include "stm32g474_platform_cold.h"

static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static GPIO_TypeDef* bank(uint8_t index) {
    switch (index) {
        case 0U:
            return GPIOA;
        case 1U:
            return GPIOB;
        case 2U:
            return GPIOC;
        case 3U:
            return GPIOD;
        case 5U:
            return GPIOF;
        case 6U:
            return GPIOG;
        default:
            return NULL;
    }
}
static bool valid(const nx_stm32g474_platform_config_t* config) {
    if (config == NULL || config->pin_count > 96U ||
        (config->pin_count != 0U && config->pins == NULL) ||
        (config->irq_masks[3] & ~UINT32_C(0x3f)) != 0U)
        return false;
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32g474_platform_pin_t* pin = &config->pins[i];
        if (pin->gpio == NULL || pin->gpio != bank(pin->port) ||
            pin->snapshot == NULL || pin->pin > 15U)
            return false;
    }
    return true;
}
static bool ready(const nx_stm32g474_platform_pin_t* pin) {
    uint32_t bit = UINT32_C(1) << pin->port;
    return (NX_G474_READ(&RCC->AHB2ENR) & bit) != 0U &&
           (NX_G474_READ(&RCC->AHB2RSTR) & bit) == 0U;
}
static nx_result_t read_pin(const nx_stm32g474_platform_pin_t* pin,
                            volatile uint32_t* reg, uint32_t* value) {
    if (!ready(pin))
        return NX_ERROR_STATE;
    *value = NX_G474_READ(reg);
    return ready(pin) ? NX_SUCCESS : NX_ERROR_STATE;
}
static nx_result_t write_pin(const nx_stm32g474_platform_pin_t* pin,
                             volatile uint32_t* reg, uint32_t value) {
    if (!ready(pin))
        return NX_ERROR_STATE;
    NX_G474_WRITE(reg, value);
    nx_arch_dsb();
    return ready(pin) ? NX_SUCCESS : NX_ERROR_STATE;
}
static nx_result_t modify_pin(const nx_stm32g474_platform_pin_t* pin,
                              volatile uint32_t* reg, uint32_t mask,
                              uint32_t value) {
    uint32_t current;
    nx_result_t status = read_pin(pin, reg, &current);
    if (status != NX_SUCCESS)
        return status;
    status = write_pin(pin, reg, (current & ~mask) | (value & mask));
    if (status != NX_SUCCESS)
        return status;
    status = read_pin(pin, reg, &current);
    if (status != NX_SUCCESS)
        return status;
    return (current & mask) == (value & mask) ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t capture(const nx_stm32g474_platform_pin_t* pin,
                           uint16_t* saved) {
    uint32_t value, state = 0U;
    unsigned shift = (unsigned)pin->pin * 2U;
    nx_result_t status;
#define CAPTURE(reg, right, mask, left)                                        \
    do {                                                                       \
        status = read_pin(pin, &pin->gpio->reg, &value);                       \
        if (status != NX_SUCCESS)                                              \
            return status;                                                     \
        state |= ((value >> (right)) & (mask)) << (left);                      \
    } while (0)
    CAPTURE(MODER, shift, 3U, 0U);
    CAPTURE(OTYPER, pin->pin, 1U, 2U);
    CAPTURE(OSPEEDR, shift, 3U, 3U);
    CAPTURE(PUPDR, shift, 3U, 5U);
    CAPTURE(AFR[pin->pin / 8U], (pin->pin % 8U) * 4U, 15U, 7U);
    CAPTURE(ODR, pin->pin, 1U, 11U);
#undef CAPTURE
    *saved = (uint16_t)state;
    return NX_SUCCESS;
}
static nx_result_t restore(const nx_stm32g474_platform_pin_t* pin,
                           uint16_t saved) {
    uint32_t value;
    uint32_t bit = UINT32_C(1) << pin->pin;
    unsigned shift = (unsigned)pin->pin * 2U;
    unsigned af = ((unsigned)pin->pin % 8U) * 4U;
    nx_result_t status = write_pin(
        pin, &pin->gpio->BSRR, (saved & (1U << 11U)) != 0U ? bit : bit << 16U);
    if (status != NX_SUCCESS)
        return status;
    status = read_pin(pin, &pin->gpio->ODR, &value);
    if (status != NX_SUCCESS)
        return status;
    if ((value & bit) != (((uint32_t)saved >> 11U & 1U) << pin->pin))
        return NX_ERROR_IO;
#define RESTORE(reg, mask, value)                                              \
    do {                                                                       \
        status = modify_pin(pin, &pin->gpio->reg, (mask), (value));            \
        if (status != NX_SUCCESS)                                              \
            return status;                                                     \
    } while (0)
    RESTORE(AFR[pin->pin / 8U], 15U << af, ((uint32_t)saved >> 7U & 15U) << af);
    RESTORE(OTYPER, bit, ((uint32_t)saved >> 2U & 1U) << pin->pin);
    RESTORE(OSPEEDR, 3U << shift, ((uint32_t)saved >> 3U & 3U) << shift);
    RESTORE(PUPDR, 3U << shift, ((uint32_t)saved >> 5U & 3U) << shift);
    RESTORE(MODER, 3U << shift, ((uint32_t)saved & 3U) << shift);
#undef RESTORE
    return NX_SUCCESS;
}
static nx_result_t admit_pins(const nx_stm32g474_platform_config_t* config,
                              bool powered_only) {
    for (size_t i = 0U; i < config->pin_count; ++i) {
        const nx_stm32g474_platform_pin_t* pin = &config->pins[i];
        uint32_t bit = UINT32_C(1) << pin->port;
        if ((NX_G474_READ(&RCC->AHB2RSTR) & bit) != 0U)
            return NX_ERROR_BUSY;
        if (powered_only && (NX_G474_READ(&RCC->AHB2ENR) & bit) == 0U)
            continue;
        uint32_t mode, lock;
        nx_result_t status = read_pin(pin, &pin->gpio->MODER, &mode);
        if (status != NX_SUCCESS)
            return status;
        mode = (mode >> ((unsigned)pin->pin * 2U)) & 3U;
        if (mode != 0U && mode != 3U)
            return NX_ERROR_BUSY;
        status = read_pin(pin, &pin->gpio->LCKR, &lock);
        if (status != NX_SUCCESS)
            return status;
        if ((lock & (UINT32_C(1) << pin->pin)) != 0U)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
/** \brief Even failed admission restores the exact incoming mask. */
nx_result_t
nx_stm32g474_platform_pins_admit(const nx_stm32g474_platform_config_t* config,
                                 bool powered_only) {
    if (!valid(config))
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    nx_result_t status = admit_pins(config, powered_only);
    nx_arch_irq_restore(previous);
    return status;
}
static nx_result_t release_clocks(nx_stm32g474_platform_pins_t* receipt) {
    if (receipt->captured)
        return NX_ERROR_BUSY;
    if (receipt->clocks_added != 0U) {
        NX_G474_WRITE(&RCC->AHB2ENR,
                      NX_G474_READ(&RCC->AHB2ENR) & ~receipt->clocks_added);
        nx_arch_dsb();
        if ((NX_G474_READ(&RCC->AHB2ENR) & receipt->clocks_added) != 0U)
            return NX_ERROR_IO;
        receipt->clocks_added = 0U;
    }
    return NX_SUCCESS;
}
nx_result_t nx_stm32g474_platform_gpio_clocks_release(
    nx_stm32g474_platform_pins_t* receipt) {
    if (receipt == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    nx_result_t status = release_clocks(receipt);
    nx_arch_irq_restore(previous);
    return status;
}
/** \brief Capture only after every pad passes admission with readable banks. */
nx_result_t
nx_stm32g474_platform_pins_acquire(const nx_stm32g474_platform_config_t* config,
                                   nx_stm32g474_platform_pins_t* receipt) {
    if (!valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (receipt->captured || receipt->clocks_added != 0U)
        return NX_ERROR_STATE;
    uint32_t mask = 0U;
    for (size_t i = 0U; i < config->pin_count; ++i)
        mask |= UINT32_C(1) << config->pins[i].port;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    receipt->clocks_added = ~NX_G474_READ(&RCC->AHB2ENR) & mask;
    if (receipt->clocks_added != 0U) {
        NX_G474_WRITE(&RCC->AHB2ENR, NX_G474_READ(&RCC->AHB2ENR) | mask);
        nx_arch_dsb();
    }
    nx_result_t status = NX_SUCCESS;
    if ((NX_G474_READ(&RCC->AHB2ENR) & mask) != mask)
        status = NX_ERROR_IO;
    if (status == NX_SUCCESS)
        status = admit_pins(config, false);
    for (size_t i = 0U; status == NX_SUCCESS && i < config->pin_count; ++i)
        status = capture(&config->pins[i], config->pins[i].snapshot);
    if (status == NX_SUCCESS)
        receipt->captured = true;
    nx_arch_irq_restore(previous);
    return status;
}
nx_result_t
nx_stm32g474_platform_pins_abort(nx_stm32g474_platform_pins_t* receipt) {
    if (receipt == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    receipt->captured = false;
    nx_result_t status = release_clocks(receipt);
    nx_arch_irq_restore(previous);
    return status;
}
static nx_result_t release_pins(const nx_stm32g474_platform_config_t* config,
                                nx_stm32g474_platform_pins_t* receipt) {
    if (receipt->captured) {
        /* All selected drive sources withdraw before any former latch/mux. */
        for (size_t i = config->pin_count; i != 0U; --i) {
            const nx_stm32g474_platform_pin_t* pin = &config->pins[i - 1U];
            uint32_t mode;
            nx_result_t status = read_pin(pin, &pin->gpio->MODER, &mode);
            if (status != NX_SUCCESS)
                return status;
            unsigned shift = (unsigned)pin->pin * 2U;
            uint32_t selected = (mode >> shift) & 3U;
            if (selected == 1U || selected == 2U) {
                status = modify_pin(pin, &pin->gpio->MODER, 3U << shift,
                                    3U << shift);
                if (status != NX_SUCCESS)
                    return status;
            }
        }
        for (size_t i = config->pin_count; i != 0U; --i) {
            const nx_stm32g474_platform_pin_t* pin = &config->pins[i - 1U];
            uint16_t current;
            nx_result_t status = capture(pin, &current);
            if (status != NX_SUCCESS)
                return status;
            if (current != *pin->snapshot) {
                status = restore(pin, *pin->snapshot);
                if (status != NX_SUCCESS)
                    return status;
                status = capture(pin, &current);
                if (status != NX_SUCCESS)
                    return status;
                if (current != *pin->snapshot)
                    return NX_ERROR_IO;
            }
        }
        receipt->captured = false;
    }
    return release_clocks(receipt);
}
nx_result_t
nx_stm32g474_platform_pins_release(const nx_stm32g474_platform_config_t* config,
                                   nx_stm32g474_platform_pins_t* receipt) {
    if (!valid(config) || receipt == NULL)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    nx_result_t status = release_pins(config, receipt);
    nx_arch_irq_restore(previous);
    return status;
}
/** \brief Every optional physical observation is gated by its exact clock. */
nx_result_t
nx_stm32g474_platform_preflight(const nx_stm32g474_platform_config_t* config) {
    if (!valid(config))
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if ((NX_G474_READ(&RCC->APB1ENR1) & config->admit_apb1) != 0U ||
        (NX_G474_READ(&RCC->APB1ENR2) & config->admit_apb1_2) != 0U ||
        (NX_G474_READ(&RCC->APB2ENR) & config->admit_apb2) != 0U ||
        (NX_G474_READ(&RCC->AHB1ENR) & config->admit_ahb1) != 0U ||
        (NX_G474_READ(&RCC->AHB2ENR) & config->admit_ahb2) != 0U ||
        (NX_G474_READ(&RCC->APB1RSTR1) & config->admit_reset_apb1) != 0U ||
        (NX_G474_READ(&RCC->CCIPR) & config->admit_mux) != 0U)
        return NX_ERROR_BUSY;
    if (config->control_group &&
        (((NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_SYSCFGEN) != 0U &&
          (NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != 0U) ||
         ((NX_G474_READ(&RCC->APB2ENR) & RCC_APB2ENR_TIM15EN) != 0U &&
          NX_G474_READ(&TIM15->DIER) != 0U)))
        return NX_ERROR_BUSY;
    if (config->flash && (NX_G474_READ(&FLASH->ECCR) & FLASH_ECCR_ECCIE) != 0U)
        return NX_ERROR_BUSY;
    if (config->exti_lines != 0U &&
        ((NX_G474_READ(&EXTI->IMR1) | NX_G474_READ(&EXTI->EMR1) |
          NX_G474_READ(&EXTI->RTSR1) | NX_G474_READ(&EXTI->FTSR1) |
          NX_G474_READ(&EXTI->PR1)) &
         config->exti_lines) != 0U)
        return NX_ERROR_BUSY;
    for (unsigned bank_index = 0U; bank_index < 4U; ++bank_index)
        for (unsigned bit = 0U; bit < 32U; ++bit)
            if ((config->irq_masks[bank_index] & (UINT32_C(1) << bit)) != 0U &&
                !NX_G474_IRQ_AVAILABLE((IRQn_Type)(bank_index * 32U + bit)))
                return NX_ERROR_BUSY;
    if (config->dma_channel1) {
        if ((NX_G474_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0U &&
            (NX_G474_READ(&DMA1_Channel1->CCR) != 0U ||
             (config->strict_dma_channel1 &&
              (NX_G474_READ(&DMA1_Channel1->CNDTR) != 0U ||
               NX_G474_READ(&DMA1_Channel1->CPAR) != 0U ||
               NX_G474_READ(&DMA1_Channel1->CMAR) != 0U))))
            return NX_ERROR_BUSY;
        if ((NX_G474_READ(&RCC->AHB1ENR) & RCC_AHB1ENR_DMAMUX1EN) != 0U &&
            NX_G474_READ(&DMAMUX1_Channel0->CCR) != 0U)
            return NX_ERROR_BUSY;
    }
    return nx_stm32g474_platform_pins_admit(config, true);
}
static nx_result_t acquire_gate(volatile uint32_t* reg, uint32_t mask,
                                uint32_t* added) {
    if (added == NULL)
        return mask == 0U ? NX_SUCCESS : NX_ERROR_INVALID;
    *added = ~NX_G474_READ(reg) & mask;
    NX_G474_WRITE(reg, NX_G474_READ(reg) | mask);
    nx_arch_dsb();
    return (NX_G474_READ(reg) & mask) == mask ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t release_gate(volatile uint32_t* reg, uint32_t* added) {
    if (added != NULL && *added != 0U) {
        NX_G474_WRITE(reg, NX_G474_READ(reg) & ~*added);
        nx_arch_dsb();
        if ((NX_G474_READ(reg) & *added) != 0U)
            return NX_ERROR_IO;
        *added = 0U;
    }
    return NX_SUCCESS;
}
static nx_result_t acquire_mux(uint32_t mask, uint32_t value, bool* owned) {
    if (owned == NULL)
        return NX_SUCCESS;
    *owned = true;
    NX_G474_WRITE(&RCC->CCIPR, NX_G474_READ(&RCC->CCIPR) | value);
    nx_arch_dsb();
    return (NX_G474_READ(&RCC->CCIPR) & mask) == value ? NX_SUCCESS
                                                       : NX_ERROR_IO;
}
static nx_result_t release_mux(uint32_t mask, bool* owned) {
    if (owned != NULL && *owned) {
        NX_G474_WRITE(&RCC->CCIPR, NX_G474_READ(&RCC->CCIPR) & ~mask);
        nx_arch_dsb();
        if ((NX_G474_READ(&RCC->CCIPR) & mask) != 0U)
            return NX_ERROR_IO;
        *owned = false;
    }
    return NX_SUCCESS;
}
/** \brief Fixed C ordering; the config contains selections, never operations.
 */
nx_result_t nx_stm32g474_platform_resources_start(
    const nx_stm32g474_platform_config_t* config) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(config))
        return NX_ERROR_INVALID;
    /* An unfinished close keeps the original gate and mux observations.
     * Re-entry cannot turn an added gate into an apparently preexisting one. */
    if ((config->added_apb1 != NULL && *config->added_apb1 != 0U) ||
        (config->added_apb1_2 != NULL && *config->added_apb1_2 != 0U) ||
        (config->added_apb2 != NULL && *config->added_apb2 != 0U) ||
        (config->added_ahb1 != NULL && *config->added_ahb1 != 0U) ||
        (config->added_ahb2 != NULL && *config->added_ahb2 != 0U) ||
        (config->added_syscfg != NULL && *config->added_syscfg != 0U) ||
        (config->owned_i2c_mux != NULL && *config->owned_i2c_mux) ||
        (config->owned_adc_mux != NULL && *config->owned_adc_mux))
        return NX_ERROR_BUSY;
    nx_result_t status = acquire_mux(RCC_CCIPR_I2C1SEL, RCC_CCIPR_I2C1SEL_1,
                                     config->owned_i2c_mux);
    if (status != NX_SUCCESS)
        return status;
    status = acquire_mux(RCC_CCIPR_ADC12SEL, RCC_CCIPR_ADC12SEL_1,
                         config->owned_adc_mux);
    if (status != NX_SUCCESS)
        return status;
#define ACQUIRE(reg, field)                                                    \
    do {                                                                       \
        status = acquire_gate(&RCC->reg, config->clocks_##field,               \
                              config->added_##field);                          \
        if (status != NX_SUCCESS)                                              \
            return status;                                                     \
    } while (0)
    ACQUIRE(APB1ENR1, apb1);
    ACQUIRE(APB1ENR2, apb1_2);
    ACQUIRE(APB2ENR, apb2);
    ACQUIRE(AHB2ENR, ahb2);
    ACQUIRE(AHB1ENR, ahb1);
#undef ACQUIRE
    status = acquire_gate(
        &RCC->APB2ENR, config->added_syscfg != NULL ? RCC_APB2ENR_SYSCFGEN : 0U,
        config->added_syscfg);
    if (status != NX_SUCCESS)
        return status;
    if (config->flash &&
        ((NX_G474_FLASH_REMAP() & SYSCFG_MEMRMP_FB_MODE) != 0U ||
         (NX_G474_READ(&FLASH->OPTR) & FLASH_OPTR_BFB2) != 0U))
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}
nx_result_t nx_stm32g474_platform_resources_stop(
    const nx_stm32g474_platform_config_t* config) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(config))
        return NX_ERROR_INVALID;
    nx_result_t status = release_gate(&RCC->APB2ENR, config->added_syscfg);
    if (status != NX_SUCCESS)
        return status;
#define RELEASE(reg, field)                                                    \
    do {                                                                       \
        status = release_gate(&RCC->reg, config->added_##field);               \
        if (status != NX_SUCCESS)                                              \
            return status;                                                     \
    } while (0)
    RELEASE(AHB1ENR, ahb1);
    RELEASE(AHB2ENR, ahb2);
    RELEASE(APB2ENR, apb2);
    RELEASE(APB1ENR2, apb1_2);
    RELEASE(APB1ENR1, apb1);
#undef RELEASE
    status = release_mux(RCC_CCIPR_I2C1SEL, config->owned_i2c_mux);
    if (status != NX_SUCCESS)
        return status;
    return release_mux(RCC_CCIPR_ADC12SEL, config->owned_adc_mux);
}
