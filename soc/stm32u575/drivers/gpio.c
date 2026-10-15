/**
 * \file            gpio.c
 * \brief           U575 authorized GPIO latch and source-reviewed AF7 pins
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_provider.h"
static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    nx_stm32u575_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || !p->output || (set & reset) != 0U)
        return NX_ERROR_INVALID;
    if (((set | reset) & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    NX_U575_WRITE(&p->registers->BSRR, set | (reset << 16U));
    return NX_SUCCESS;
}
static nx_result_t read(const void* context, uint32_t* value) {
    const nx_stm32u575_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || p->registers == NULL || value == NULL ||
        (uintptr_t)value % _Alignof(uint32_t) != 0U ||
        (uintptr_t)value > UINTPTR_MAX - sizeof(*value) ||
        ((uintptr_t)value < (uintptr_t)p + sizeof(*p) &&
         (uintptr_t)p < (uintptr_t)value + sizeof(*value)) ||
        ((uintptr_t)value < (uintptr_t)p->registers + sizeof(*p->registers) &&
         (uintptr_t)p->registers < (uintptr_t)value + sizeof(*value)))
        return NX_ERROR_INVALID;
    *value = NX_U575_READ(&p->registers->IDR) & p->mask;
    return NX_SUCCESS;
}
static nx_result_t toggle(void* context, uint32_t mask) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    nx_stm32u575_gpio_state_t* p = context;
    if (p == NULL || !p->initialized || !p->output)
        return NX_ERROR_INVALID;
    if ((mask & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    uint32_t high = NX_U575_READ(&p->registers->ODR) & mask;
    return write(p, mask & ~high, high);
}
nx_result_t nx_stm32u575_gpio_initialize(nx_stm32u575_gpio_state_t* p,
                                         uint32_t initial) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL || p->mask == 0U ||
        (p->mask & ~0xffffU) != 0U)
        return NX_ERROR_INVALID;
    if ((initial & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    if (p->initialized || p->retained)
        return NX_ERROR_BUSY;
    p->retained = true;
    NX_U575_WRITE(&p->registers->BSRR, initial | ((p->mask & ~initial) << 16U));
    nx_arch_dsb();
    if ((NX_U575_READ(&p->registers->ODR) & p->mask) != initial)
        return NX_ERROR_IO;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((p->mask & (1U << pin)) != 0U) {
            uint32_t shift = pin * 2U;
            NX_U575_WRITE(&p->registers->OTYPER,
                          NX_U575_READ(&p->registers->OTYPER) & ~(1U << pin));
            NX_U575_WRITE(&p->registers->PUPDR,
                          NX_U575_READ(&p->registers->PUPDR) & ~(3U << shift));
            nx_arch_dsb();
            if ((NX_U575_READ(&p->registers->OTYPER) & (1U << pin)) != 0U ||
                (NX_U575_READ(&p->registers->PUPDR) & (3U << shift)) != 0U)
                return NX_ERROR_IO;
            NX_U575_WRITE(
                &p->registers->MODER,
                (NX_U575_READ(&p->registers->MODER) & ~(3U << shift)) |
                    ((p->output ? 1U : 0U) << shift));
            nx_arch_dsb();
            if (((NX_U575_READ(&p->registers->MODER) >> shift) & 3U) !=
                (p->output ? 1U : 0U))
                return NX_ERROR_IO;
        }
    p->initialized = true;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_gpio_stop(nx_stm32u575_gpio_state_t* p,
                                   uint32_t inactive) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL || (inactive & ~p->mask) != 0U)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->output) {
        NX_U575_WRITE(&p->registers->BSRR,
                      inactive | ((p->mask & ~inactive) << 16U));
        nx_arch_dsb();
        if ((NX_U575_READ(&p->registers->ODR) & p->mask) != inactive)
            return NX_ERROR_IO;
    }
    p->initialized = false;
    p->retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_gpio_clock_enable(unsigned index) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if ((index != 0U && index != 2U) || g_nx_stm32u575_system.rcc == NULL)
        return NX_ERROR_UNSUPPORTED;
    uint32_t bit = 1U << index;
    volatile uint32_t* gate = &g_nx_stm32u575_system.rcc->AHB2ENR1;
    NX_U575_WRITE(gate, NX_U575_READ(gate) | bit);
    nx_arch_dsb();
    return (NX_U575_READ(gate) & bit) != 0U ? NX_SUCCESS : NX_ERROR_IO;
}
nx_result_t nx_stm32u575_usart1_pins_prepare(void) {
    nx_result_t result = nx_stm32u575_gpio_clock_enable(0U);
    if (result != NX_SUCCESS)
        return result;
    volatile uint32_t* gate = &g_nx_stm32u575_system.rcc->APB2ENR;
    NX_U575_WRITE(gate, NX_U575_READ(gate) | RCC_APB2ENR_USART1EN);
    nx_arch_dsb();
    if ((NX_U575_READ(gate) & RCC_APB2ENR_USART1EN) == 0U)
        return NX_ERROR_IO;
#ifndef NX_U575_GPIOA
#define NX_U575_GPIOA GPIOA
#endif
    GPIO_TypeDef* gpio = NX_U575_GPIOA;
    const uint32_t bits = (1U << 9U) | (1U << 10U);
    const uint32_t fields = (3U << 18U) | (3U << 20U);
    const uint32_t af = (15U << 4U) | (15U << 8U);
    NX_U575_WRITE(&gpio->BSRR, 1U << 9U);
    nx_arch_dsb();
    if ((NX_U575_READ(&gpio->ODR) & (1U << 9U)) == 0U)
        return NX_ERROR_IO;
    NX_U575_WRITE(&gpio->OTYPER, NX_U575_READ(&gpio->OTYPER) & ~bits);
    NX_U575_WRITE(&gpio->OSPEEDR, (NX_U575_READ(&gpio->OSPEEDR) & ~fields) |
                                      (1U << 18U) | (1U << 20U));
    NX_U575_WRITE(&gpio->PUPDR,
                  (NX_U575_READ(&gpio->PUPDR) & ~fields) | (1U << 20U));
    NX_U575_WRITE(&gpio->AFR[1], (NX_U575_READ(&gpio->AFR[1]) & ~af) |
                                     (7U << 4U) | (7U << 8U));
    nx_arch_dsb();
    if ((NX_U575_READ(&gpio->OTYPER) & bits) != 0U ||
        (NX_U575_READ(&gpio->OSPEEDR) & fields) !=
            ((1U << 18U) | (1U << 20U)) ||
        (NX_U575_READ(&gpio->PUPDR) & fields) != (1U << 20U) ||
        (NX_U575_READ(&gpio->AFR[1]) & af) != ((7U << 4U) | (7U << 8U)))
        return NX_ERROR_IO;
    NX_U575_WRITE(&gpio->MODER, (NX_U575_READ(&gpio->MODER) & ~fields) |
                                    (2U << 18U) | (2U << 20U));
    nx_arch_dsb();
    return (NX_U575_READ(&gpio->MODER) & fields) ==
                       ((2U << 18U) | (2U << 20U)) &&
                   (NX_U575_READ(&gpio->AFR[1]) & af) ==
                       ((7U << 4U) | (7U << 8U))
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
const nx_gpio_ops_t nx_stm32u575_gpio_ops = {write, read, toggle};
