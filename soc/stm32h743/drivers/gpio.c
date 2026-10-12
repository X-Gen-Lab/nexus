/**
 * \file            gpio.c
 * \brief           H743 GPIO latch and reviewed PD8/PD9 AF7 preparation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_provider.h"
static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    nx_stm32h743_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || !p->output || (set & reset) != 0U)
        return NX_ERROR_INVALID;
    if (((set | reset) & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    NX_H743_WRITE(&p->registers->BSRR, set | (reset << 16U));
    return NX_SUCCESS;
}
static nx_result_t read(const void* context, uint32_t* value) {
    const nx_stm32h743_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || p->registers == NULL || value == NULL ||
        (uintptr_t)value > UINTPTR_MAX - sizeof(*value) ||
        ((uintptr_t)value % _Alignof(uint32_t)) != 0U ||
        ((uintptr_t)value < (uintptr_t)p + sizeof(*p) &&
         (uintptr_t)p < (uintptr_t)value + sizeof(*value)) ||
        ((uintptr_t)value < (uintptr_t)p->registers + sizeof(*p->registers) &&
         (uintptr_t)p->registers < (uintptr_t)value + sizeof(*value)))
        return NX_ERROR_INVALID;
    *value = NX_H743_READ(&p->registers->IDR) & p->mask;
    return NX_SUCCESS;
}
static nx_result_t toggle(void* context, uint32_t mask) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    nx_stm32h743_gpio_state_t* p = context;
    if (p == NULL || !p->initialized || !p->output)
        return NX_ERROR_INVALID;
    if ((mask & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    uint32_t high = NX_H743_READ(&p->registers->ODR) & mask;
    return write(p, mask & ~high, high);
}
nx_result_t nx_stm32h743_gpio_initialize(nx_stm32h743_gpio_state_t* p,
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
    NX_H743_WRITE(&p->registers->BSRR, initial | ((p->mask & ~initial) << 16U));
    nx_arch_dsb();
    if ((NX_H743_READ(&p->registers->ODR) & p->mask) != initial)
        return NX_ERROR_IO;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((p->mask & (1U << pin)) != 0U) {
            uint32_t shift = pin * 2U;
            NX_H743_WRITE(&p->registers->OTYPER,
                          NX_H743_READ(&p->registers->OTYPER) & ~(1U << pin));
            NX_H743_WRITE(&p->registers->PUPDR,
                          NX_H743_READ(&p->registers->PUPDR) & ~(3U << shift));
            nx_arch_dsb();
            if ((NX_H743_READ(&p->registers->OTYPER) & (1U << pin)) != 0U ||
                (NX_H743_READ(&p->registers->PUPDR) & (3U << shift)) != 0U)
                return NX_ERROR_IO;
            NX_H743_WRITE(
                &p->registers->MODER,
                (NX_H743_READ(&p->registers->MODER) & ~(3U << shift)) |
                    ((p->output ? 1U : 0U) << shift));
            nx_arch_dsb();
            if (((NX_H743_READ(&p->registers->MODER) >> shift) & 3U) !=
                (p->output ? 1U : 0U))
                return NX_ERROR_IO;
        }
    p->initialized = true;
    return NX_SUCCESS;
}
nx_result_t nx_stm32h743_gpio_stop(nx_stm32h743_gpio_state_t* p,
                                   uint32_t inactive) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL || (inactive & ~p->mask) != 0U)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->output) {
        NX_H743_WRITE(&p->registers->BSRR,
                      inactive | ((p->mask & ~inactive) << 16U));
        nx_arch_dsb();
        if ((NX_H743_READ(&p->registers->ODR) & p->mask) != inactive)
            return NX_ERROR_IO;
    }
    p->initialized = false;
    p->retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32h743_gpio_clock_enable(unsigned index) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if ((index != 1U && index != 3U) || g_nx_stm32h743_system.rcc == NULL)
        return NX_ERROR_UNSUPPORTED;
    uint32_t bit = index == 1U ? RCC_AHB4ENR_GPIOBEN : RCC_AHB4ENR_GPIODEN;
    volatile uint32_t* gate = &g_nx_stm32h743_system.rcc->AHB4ENR;
    NX_H743_WRITE(gate, NX_H743_READ(gate) | bit);
    nx_arch_dsb();
    return (NX_H743_READ(gate) & bit) != 0U ? NX_SUCCESS : NX_ERROR_IO;
}
nx_result_t nx_stm32h743_usart3_pins_prepare(void) {
    nx_result_t result = nx_stm32h743_gpio_clock_enable(3U);
    if (result != NX_SUCCESS)
        return result;
    volatile uint32_t* gate = &g_nx_stm32h743_system.rcc->APB1LENR;
    NX_H743_WRITE(gate, NX_H743_READ(gate) | RCC_APB1LENR_USART3EN);
    nx_arch_dsb();
    if ((NX_H743_READ(gate) & RCC_APB1LENR_USART3EN) == 0U)
        return NX_ERROR_IO;
#ifndef NX_H743_GPIOD
#define NX_H743_GPIOD GPIOD
#endif
    GPIO_TypeDef* gpio = NX_H743_GPIOD;
    const uint32_t bits = (1U << 8U) | (1U << 9U);
    const uint32_t mask = (3U << 16U) | (3U << 18U);
    const uint32_t af_mask = (15U << 0U) | (15U << 4U);
    NX_H743_WRITE(&gpio->BSRR, 1U << 8U);
    nx_arch_dsb();
    if ((NX_H743_READ(&gpio->ODR) & (1U << 8U)) == 0U)
        return NX_ERROR_IO;
    NX_H743_WRITE(&gpio->OTYPER, NX_H743_READ(&gpio->OTYPER) & ~bits);
    NX_H743_WRITE(&gpio->OSPEEDR, (NX_H743_READ(&gpio->OSPEEDR) & ~mask) |
                                      (1U << 16U) | (1U << 18U));
    NX_H743_WRITE(&gpio->PUPDR,
                  (NX_H743_READ(&gpio->PUPDR) & ~mask) | (1U << 18U));
    NX_H743_WRITE(&gpio->AFR[1], (NX_H743_READ(&gpio->AFR[1]) & ~af_mask) |
                                     (7U << 0U) | (7U << 4U));
    nx_arch_dsb();
    if ((NX_H743_READ(&gpio->OTYPER) & bits) != 0U ||
        (NX_H743_READ(&gpio->OSPEEDR) & mask) != ((1U << 16U) | (1U << 18U)) ||
        (NX_H743_READ(&gpio->PUPDR) & mask) != (1U << 18U) ||
        (NX_H743_READ(&gpio->AFR[1]) & af_mask) != ((7U << 0U) | (7U << 4U)))
        return NX_ERROR_IO;
    NX_H743_WRITE(&gpio->MODER, (NX_H743_READ(&gpio->MODER) & ~mask) |
                                    (2U << 16U) | (2U << 18U));
    nx_arch_dsb();
    return (NX_H743_READ(&gpio->MODER) & mask) == ((2U << 16U) | (2U << 18U)) &&
                   (NX_H743_READ(&gpio->AFR[1]) & af_mask) ==
                       ((7U << 0U) | (7U << 4U))
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
const nx_gpio_ops_t nx_stm32h743_gpio_ops = {write, read, toggle};
