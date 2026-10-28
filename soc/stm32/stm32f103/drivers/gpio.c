/**
 * \file            gpio.c
 * \brief           F103 CRL pin modes, safe preload and fixed mask access
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f103_provider.h"
#include "stm32f1xx_ll_gpio.h"
static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_F103_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_F103_READ(reg) & mask) == (value & mask);
}
/** \brief Retained cold state rejects before replacing the selected facts. */
nx_result_t nx_stm32f103_gpio_prepare(nx_stm32f103_gpio_state_t* p,
                                      uint32_t mask, bool output,
                                      uint32_t initial) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || (mask != 16U && mask != 32U) || (initial & ~mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (p->retained || p->initialized) {
        return NX_ERROR_BUSY;
    }
    p->registers = NX_F103_GPIOA;
    p->mask = mask;
    p->output = output;
    return nx_stm32f103_gpio_initialize(p, initial);
}
nx_result_t nx_stm32f103_gpio_initialize(nx_stm32f103_gpio_state_t* p,
                                         uint32_t initial) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || p->registers != NX_F103_GPIOA ||
        (p->mask != 16U && p->mask != 32U) || (initial & ~p->mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (p->initialized || p->retained) {
        return NX_ERROR_BUSY;
    }
    if (!g_nx_stm32f103_system.started ||
        (NX_F103_READ(&g_nx_stm32f103_system.rcc->APB2ENR) &
         RCC_APB2ENR_IOPAEN) == 0U) {
        return NX_ERROR_STATE;
    }
    uint32_t shift = p->mask == 16U ? 16U : 20U;
    uint32_t current = (NX_F103_READ(&p->registers->CRL) >> shift) & 15U;
    if ((NX_F103_READ(&p->registers->LCKR) & p->mask) != 0U ||
        (current != 0U && current != 4U)) {
        return NX_ERROR_BUSY;
    }
    p->retained = true;
    NX_F103_WRITE(&p->registers->BSRR, initial | ((p->mask & ~initial) << 16U));
    nx_arch_dsb();
    if ((NX_F103_READ(&p->registers->ODR) & p->mask) != initial) {
        return NX_ERROR_IO;
    }
    /* F1 has one CRL nibble,not a modern MODER/AFR pair. 2MHz push-pull
     * output is0x2; floating input is0x4. Preserve every unleased nibble. */
    uint32_t mode = p->output ? 2U : 4U;
    if (!checked(&p->registers->CRL,
                 (NX_F103_READ(&p->registers->CRL) & ~(15U << shift)) |
                     (mode << shift),
                 15U << shift)) {
        return NX_ERROR_IO;
    }
    p->initialized = true;
    return NX_SUCCESS;
}
static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    nx_stm32f103_gpio_state_t* p = context;
    if (p == NULL || !p->initialized || !p->output || (set & reset) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (((set | reset) & ~p->mask) != 0U) {
        return NX_ERROR_PERMISSION;
    }
    NX_F103_WRITE(&p->registers->BSRR, set | (reset << 16U));
    return NX_SUCCESS;
}
static nx_result_t read(const void* context, uint32_t* value) {
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    const nx_stm32f103_gpio_state_t* p = context;
    if (p == NULL || !p->initialized || value == NULL) {
        return NX_ERROR_INVALID;
    }
    uintptr_t output = (uintptr_t)value;
    if (output > UINTPTR_MAX - sizeof(*value) ||
        (output < (uintptr_t)p + sizeof(*p) &&
         (uintptr_t)p < output + sizeof(*value)) ||
        (output < (uintptr_t)p->registers + sizeof(*p->registers) &&
         (uintptr_t)p->registers < output + sizeof(*value))) {
        return NX_ERROR_INVALID;
    }
    *value = LL_GPIO_ReadInputPort(p->registers) & p->mask;
    return NX_SUCCESS;
}
static nx_result_t toggle(void* context, uint32_t mask) {
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    nx_stm32f103_gpio_state_t* p = context;
    if (p == NULL || !p->initialized || !p->output) {
        return NX_ERROR_INVALID;
    }
    if ((mask & ~p->mask) != 0U) {
        return NX_ERROR_PERMISSION;
    }
    uint32_t high = LL_GPIO_ReadOutputPort(p->registers) & mask;
    return write(p, mask & ~high, high);
}
nx_result_t nx_stm32f103_gpio_stop(nx_stm32f103_gpio_state_t* p,
                                   uint32_t inactive) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || (inactive & ~p->mask) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (!p->retained) {
        return NX_SUCCESS;
    }
    if (p->output) {
        NX_F103_WRITE(&p->registers->BSRR,
                      inactive | ((p->mask & ~inactive) << 16U));
        nx_arch_dsb();
        if ((NX_F103_READ(&p->registers->ODR) & p->mask) != inactive) {
            return NX_ERROR_IO;
        }
    }
    p->initialized = false;
    p->retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32f103_usart2_pins_prepare(void) {
    RCC_TypeDef* rcc = g_nx_stm32f103_system.rcc;
    GPIO_TypeDef* gpio = NX_F103_GPIOA;
    if ((NX_F103_READ(&rcc->APB2ENR) &
         (RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN)) !=
            (RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN) ||
        (NX_F103_READ(&NX_F103_AFIO->MAPR) & AFIO_MAPR_USART2_REMAP) != 0U) {
        return NX_ERROR_STATE;
    }
    /* Default PA2 alternate push-pull2MHz and PA3 floating input. AFIO SWJ
     * fields have undefined readback; do not perform an unrelated MAPR RMW. */
    NX_F103_WRITE(&gpio->BSRR, 1U << 2U);
    nx_arch_dsb();
    if ((NX_F103_READ(&gpio->ODR) & (1U << 2U)) == 0U ||
        !checked(&gpio->CRL,
                 (NX_F103_READ(&gpio->CRL) & ~((15U << 8U) | (15U << 12U))) |
                     (10U << 8U) | (4U << 12U),
                 (15U << 8U) | (15U << 12U))) {
        return NX_ERROR_IO;
    }
    return checked(&rcc->APB1ENR,
                   NX_F103_READ(&rcc->APB1ENR) | RCC_APB1ENR_USART2EN,
                   RCC_APB1ENR_USART2EN)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
const nx_gpio_ops_t nx_stm32f103_gpio_ops = {write, read, toggle};
