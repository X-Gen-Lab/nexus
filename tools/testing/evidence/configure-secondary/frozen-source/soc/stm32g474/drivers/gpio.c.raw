/**
 * \file            gpio.c
 * \brief           G474 authorized GPIO latch and source-reviewed AF12 pins
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32g474_provider.h"
static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    nx_stm32g474_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || !p->output || (set & reset) != 0U)
        return NX_ERROR_INVALID;
    if (((set | reset) & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    NX_G474_WRITE(&p->registers->BSRR, set | (reset << 16U));
    return NX_SUCCESS;
}
static nx_result_t read(const void* context, uint32_t* value) {
    const nx_stm32g474_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || value == NULL ||
        ((uintptr_t)value < (uintptr_t)p + sizeof(*p) &&
         (uintptr_t)p < (uintptr_t)value + sizeof(*value)) ||
        ((uintptr_t)value < (uintptr_t)p->registers + sizeof(*p->registers) &&
         (uintptr_t)p->registers < (uintptr_t)value + sizeof(*value)))
        return NX_ERROR_INVALID;
    *value = NX_G474_READ(&p->registers->IDR) & p->mask;
    return NX_SUCCESS;
}
static nx_result_t toggle(void* context, uint32_t mask) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    nx_stm32g474_gpio_state_t* p = context;
    if (p == NULL || !p->initialized || !p->output)
        return NX_ERROR_INVALID;
    if ((mask & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    uint32_t high = NX_G474_READ(&p->registers->ODR) & mask;
    return write(p, mask & ~high, high);
}
nx_result_t nx_stm32g474_gpio_initialize(nx_stm32g474_gpio_state_t* p,
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
    NX_G474_WRITE(&p->registers->BSRR, initial | ((p->mask & ~initial) << 16U));
    nx_arch_dsb();
    if ((NX_G474_READ(&p->registers->ODR) & p->mask) != initial)
        return NX_ERROR_IO;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((p->mask & (1U << pin)) != 0U) {
            uint32_t shift = pin * 2U;
            NX_G474_WRITE(&p->registers->OTYPER,
                          NX_G474_READ(&p->registers->OTYPER) & ~(1U << pin));
            NX_G474_WRITE(&p->registers->PUPDR,
                          NX_G474_READ(&p->registers->PUPDR) & ~(3U << shift));
            nx_arch_dsb();
            if ((NX_G474_READ(&p->registers->OTYPER) & (1U << pin)) != 0U ||
                (NX_G474_READ(&p->registers->PUPDR) & (3U << shift)) != 0U)
                return NX_ERROR_IO;
            NX_G474_WRITE(
                &p->registers->MODER,
                (NX_G474_READ(&p->registers->MODER) & ~(3U << shift)) |
                    ((p->output ? 1U : 0U) << shift));
            nx_arch_dsb();
            if (((NX_G474_READ(&p->registers->MODER) >> shift) & 3U) !=
                (p->output ? 1U : 0U))
                return NX_ERROR_IO;
        }
    p->initialized = true;
    return NX_SUCCESS;
}
nx_result_t nx_stm32g474_gpio_stop(nx_stm32g474_gpio_state_t* p,
                                   uint32_t inactive) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (p == NULL || (inactive & ~p->mask) != 0U)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->output) {
        NX_G474_WRITE(&p->registers->BSRR,
                      inactive | ((p->mask & ~inactive) << 16U));
        nx_arch_dsb();
        if ((NX_G474_READ(&p->registers->ODR) & p->mask) != inactive)
            return NX_ERROR_IO;
    }
    p->initialized = false;
    p->retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32g474_gpio_clock_enable(unsigned index) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (index != 0U || g_nx_stm32g474_system.rcc == NULL)
        return NX_ERROR_UNSUPPORTED;
    volatile uint32_t* gate = &g_nx_stm32g474_system.rcc->AHB2ENR;
    NX_G474_WRITE(gate, NX_G474_READ(gate) | RCC_AHB2ENR_GPIOAEN);
    nx_arch_dsb();
    return (NX_G474_READ(gate) & RCC_AHB2ENR_GPIOAEN) != 0U ? NX_SUCCESS
                                                            : NX_ERROR_IO;
}
nx_result_t nx_stm32g474_lpuart1_pins_prepare(void) {
    nx_result_t result = nx_stm32g474_gpio_clock_enable(0U);
    if (result != NX_SUCCESS)
        return result;
    volatile uint32_t* gate = &g_nx_stm32g474_system.rcc->APB1ENR2;
    NX_G474_WRITE(gate, NX_G474_READ(gate) | RCC_APB1ENR2_LPUART1EN);
    nx_arch_dsb();
    if ((NX_G474_READ(gate) & RCC_APB1ENR2_LPUART1EN) == 0U)
        return NX_ERROR_IO;
#ifndef NX_G474_GPIOA
#define NX_G474_GPIOA GPIOA
#endif
    GPIO_TypeDef* gpio = NX_G474_GPIOA;
    uint32_t mask = (3U << 4U) | (3U << 6U);
    NX_G474_WRITE(&gpio->BSRR, 1U << 2U);
    nx_arch_dsb();
    if ((NX_G474_READ(&gpio->ODR) & (1U << 2U)) == 0U)
        return NX_ERROR_IO;
    NX_G474_WRITE(&gpio->OTYPER,
                  NX_G474_READ(&gpio->OTYPER) & ~((1U << 2U) | (1U << 3U)));
    NX_G474_WRITE(&gpio->OSPEEDR, (NX_G474_READ(&gpio->OSPEEDR) & ~mask) |
                                      (1U << 4U) | (1U << 6U));
    NX_G474_WRITE(&gpio->PUPDR,
                  (NX_G474_READ(&gpio->PUPDR) & ~mask) | (1U << 6U));
    NX_G474_WRITE(&gpio->AFR[0], (NX_G474_READ(&gpio->AFR[0]) &
                                  ~((15U << 8U) | (15U << 12U))) |
                                     (12U << 8U) | (12U << 12U));
    nx_arch_dsb();
    if ((NX_G474_READ(&gpio->OTYPER) & ((1U << 2U) | (1U << 3U))) != 0U ||
        (NX_G474_READ(&gpio->OSPEEDR) & mask) != ((1U << 4U) | (1U << 6U)) ||
        (NX_G474_READ(&gpio->PUPDR) & mask) != (1U << 6U) ||
        (NX_G474_READ(&gpio->AFR[0]) & ((15U << 8U) | (15U << 12U))) !=
            ((12U << 8U) | (12U << 12U)))
        return NX_ERROR_IO;
    NX_G474_WRITE(&gpio->MODER, (NX_G474_READ(&gpio->MODER) & ~mask) |
                                    (2U << 4U) | (2U << 6U));
    nx_arch_dsb();
    return (NX_G474_READ(&gpio->MODER) & mask) == ((2U << 4U) | (2U << 6U)) &&
                   (NX_G474_READ(&gpio->AFR[0]) &
                    ((15U << 8U) | (15U << 12U))) ==
                       ((12U << 8U) | (12U << 12U))
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
const nx_gpio_ops_t nx_stm32g474_gpio_ops = {write, read, toggle};
