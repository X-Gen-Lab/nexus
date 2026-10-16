/**
 * \file            gpio.c
 * \brief           GD32F303 exact GPIOA authorization and cold pin lease
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static volatile uint32_t* control(unsigned pin) {
    return pin < 8U ? &GPIO_CTL0(GPIOA) : &GPIO_CTL1(GPIOA);
}
static bool configure_pin(unsigned pin, uint32_t mode) {
    volatile uint32_t* reg = control(pin);
    unsigned shift = (pin % 8U) * 4U;
    NX_GD303_WRITE(reg,
                   (NX_GD303_READ(reg) & ~(15U << shift)) | (mode << shift));
    nx_arch_dsb();
    return ((NX_GD303_READ(reg) >> shift) & 15U) == mode;
}
static bool latch(uint32_t mask, uint32_t value) {
    NX_GD303_WRITE(&GPIO_BOP(GPIOA), value | ((mask & ~value) << 16U));
    nx_arch_dsb();
    return (NX_GD303_READ(&GPIO_OCTL(GPIOA)) & mask) == value;
}
static bool valid_mask(uint32_t mask) {
    return mask != 0U && (mask & ~0x6f3U) == 0U;
}
static bool pins_available(uint32_t mask) {
    bool available = (NX_GD303_READ(&GPIO_LOCK(GPIOA)) & mask) == 0U;
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if ((mask & (1U << pin)) == 0U)
            continue;
        uint32_t mode =
            (NX_GD303_READ(control(pin)) >> ((pin % 8U) * 4U)) & 15U;
        if (mode != 0U && mode != 4U && mode != 8U)
            available = false;
    }
    return available;
}
nx_result_t nx_gd32f303_pins_preflight(uint32_t mask) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_mask(mask))
        return NX_ERROR_INVALID;
    if ((NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_PAEN) == 0U)
        return NX_SUCCESS;
    return pins_available(mask) ? NX_SUCCESS : NX_ERROR_BUSY;
}
nx_result_t nx_gd32f303_pins_acquire(nx_gd32f303_pin_lease_t* p,
                                     uint32_t mask) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !valid_mask(mask))
        return NX_ERROR_INVALID;
    if (p->retained)
        return NX_ERROR_BUSY;
    nx_result_t preflight = nx_gd32f303_pins_preflight(mask);
    if (preflight != NX_SUCCESS)
        return preflight;
    p->mask = mask;
    p->captured = false;
    p->gate_added = (NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_PAEN) == 0U;
    p->retained = true;
    NX_GD303_WRITE(&RCU_APB2EN, NX_GD303_READ(&RCU_APB2EN) | RCU_APB2EN_PAEN);
    nx_arch_dsb();
    if ((NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_PAEN) == 0U)
        return NX_ERROR_IO;
    if (!pins_available(mask)) {
        /* No pin snapshot/effect was accepted; only the added gate is ours. */
        (void)nx_gd32f303_pins_release(p);
        return NX_ERROR_BUSY;
    }
    p->saved_ctl[0] = NX_GD303_READ(&GPIO_CTL0(GPIOA));
    p->saved_ctl[1] = NX_GD303_READ(&GPIO_CTL1(GPIOA));
    p->saved_latch = NX_GD303_READ(&GPIO_OCTL(GPIOA)) & mask;
    p->captured = true;
    return NX_SUCCESS;
}
nx_result_t nx_gd32f303_pins_release(nx_gd32f303_pin_lease_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->captured) {
        /* First remove every selected driver, then restore the original latch
         * and nibble. A refused write keeps the full bank/gate lease alive. */
        for (unsigned pin = 0U; pin < 16U; ++pin)
            if ((p->mask & (1U << pin)) != 0U && !configure_pin(pin, 4U))
                return NX_ERROR_IO;
        if (!latch(p->mask, p->saved_latch))
            return NX_ERROR_IO;
        for (unsigned pin = 0U; pin < 16U; ++pin)
            if ((p->mask & (1U << pin)) != 0U &&
                !configure_pin(
                    pin, (p->saved_ctl[pin / 8U] >> ((pin % 8U) * 4U)) & 15U))
                return NX_ERROR_IO;
        p->captured = false;
    }
    if (p->gate_added) {
        NX_GD303_WRITE(&RCU_APB2EN,
                       NX_GD303_READ(&RCU_APB2EN) & ~(uint32_t)RCU_APB2EN_PAEN);
        nx_arch_dsb();
        if ((NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_PAEN) != 0U)
            return NX_ERROR_IO;
        p->gate_added = false;
    }
    p->retained = false;
    return NX_SUCCESS;
}
static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    nx_gd32f303_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || !p->output || (set & reset) != 0U)
        return NX_ERROR_INVALID;
    if (((set | reset) & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    NX_GD303_WRITE(&GPIO_BOP(GPIOA), set | (reset << 16U));
    return NX_SUCCESS;
}
static nx_result_t read(const void* context, uint32_t* value) {
    const nx_gd32f303_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || value == NULL ||
        (uintptr_t)value > UINTPTR_MAX - sizeof(*value) ||
        ((uintptr_t)value < (uintptr_t)p + sizeof(*p) &&
         (uintptr_t)p < (uintptr_t)value + sizeof(*value)) ||
        ((uintptr_t)value < (uintptr_t)GPIOA + 0x40U &&
         (uintptr_t)GPIOA < (uintptr_t)value + sizeof(*value)))
        return NX_ERROR_INVALID;
    *value = NX_GD303_READ(&GPIO_ISTAT(GPIOA)) & p->mask;
    return NX_SUCCESS;
}
static nx_result_t toggle(void* context, uint32_t mask) {
    nx_gd32f303_gpio_state_t* p = context;
    if (p == NULL || !p->initialized || !p->output)
        return NX_ERROR_INVALID;
    if ((mask & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    uint32_t high = NX_GD303_READ(&GPIO_OCTL(GPIOA)) & mask;
    return write(p, mask & ~high, high);
}
nx_result_t nx_gd32f303_gpio_initialize(nx_gd32f303_gpio_state_t* p,
                                        uint32_t initial) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || (p->mask != 0x20U && p->mask != 0x10U) ||
        (initial & ~p->mask) != 0U)
        return NX_ERROR_INVALID;
    if (p->retained || p->initialized)
        return NX_ERROR_BUSY;
    if ((NX_GD303_READ(&RCU_APB2EN) & RCU_APB2EN_PAEN) == 0U)
        return NX_ERROR_STATE;
    unsigned pin = p->mask == 0x10U ? 4U : 5U;
    uint32_t mode = (NX_GD303_READ(&GPIO_CTL0(GPIOA)) >> (pin * 4U)) & 15U;
    if ((mode != 0U && mode != 4U && mode != 8U) ||
        (NX_GD303_READ(&GPIO_LOCK(GPIOA)) & p->mask) != 0U)
        return NX_ERROR_BUSY;
    p->retained = true;
    if ((p->output && !latch(p->mask, initial)) ||
        !configure_pin(pin, p->output ? 2U : 4U))
        return NX_ERROR_IO;
    p->initialized = true;
    return NX_SUCCESS;
}
nx_result_t nx_gd32f303_gpio_stop(nx_gd32f303_gpio_state_t* p,
                                  uint32_t inactive) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (p == NULL || (inactive & ~p->mask) != 0U)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    unsigned pin = p->mask == 0x10U ? 4U : 5U;
    uint32_t mode = (NX_GD303_READ(&GPIO_CTL0(GPIOA)) >> (pin * 4U)) & 3U;
    /* A failed preload never published a strong output. The enclosing pin
     * lease restores its captured latch; do not invent an inactive drive. */
    if (p->output && (p->initialized || mode != 0U) &&
        !latch(p->mask, inactive))
        return NX_ERROR_IO;
    p->initialized = false;
    p->retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_gd32f303_usart0_pins_prepare(void) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    const uint32_t gates =
        RCU_APB2EN_PAEN | RCU_APB2EN_AFEN | RCU_APB2EN_USART0EN;
    if ((NX_GD303_READ(&RCU_APB2EN) & gates) != gates)
        return NX_ERROR_STATE;
    if ((NX_GD303_READ(&AFIO_PCF0) & AFIO_PCF0_USART0_REMAP) != 0U)
        return NX_ERROR_UNSUPPORTED;
    uint32_t ctl = NX_GD303_READ(&GPIO_CTL1(GPIOA));
    uint32_t tx = (ctl >> 4U) & 15U;
    uint32_t rx = (ctl >> 8U) & 15U;
    if ((tx != 0U && tx != 4U && tx != 8U) ||
        (rx != 0U && rx != 4U && rx != 8U) ||
        (NX_GD303_READ(&GPIO_LOCK(GPIOA)) & 0x600U) != 0U)
        return NX_ERROR_BUSY;
    if (!latch(0x200U, 0x200U) || !configure_pin(10U, 4U) ||
        !configure_pin(9U, 10U))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
const nx_gpio_ops_t nx_gd32f303_gpio_ops = {write, read, toggle};

nx_result_t nx_gd32f303_spi0_pins_prepare(void) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    const uint32_t gates =
        RCU_APB2EN_PAEN | RCU_APB2EN_AFEN | RCU_APB2EN_SPI0EN;
    if ((NX_GD303_READ(&RCU_APB2EN) & gates) != gates)
        return NX_ERROR_STATE;
    if ((NX_GD303_READ(&AFIO_PCF0) & AFIO_PCF0_SPI0_REMAP) != 0U)
        return NX_ERROR_UNSUPPORTED;
    if (!pins_available(0xe0U))
        return NX_ERROR_BUSY;
    if (!configure_pin(6U, 4U) || !configure_pin(7U, 11U) ||
        !configure_pin(5U, 11U))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
