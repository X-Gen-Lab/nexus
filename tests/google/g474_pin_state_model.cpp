/**
 * \file            g474_pin_state_model.cpp
 * \brief           Real register-layout leaves, never provider progress state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pin_state_model.h"
#include <cstring>
nx_pin_state_model_t g_pin_state_model;
static GPIO_TypeDef& PinGpio(void) {
#ifdef NX_PIN_STATE_FACTORY_MODEL
    return g_g474_model.gpio;
#else
    return g_pin_state_model.gpio;
#endif
}
static RCC_TypeDef& PinRcc(void) {
#ifdef NX_PIN_STATE_FACTORY_MODEL
    return g_g474_model.rcc;
#else
    return g_pin_state_model.rcc;
#endif
}
static bool IsGpio(const volatile uint32_t* reg) {
    uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    uintptr_t first = reinterpret_cast<uintptr_t>(&PinGpio());
    return address >= first && address < first + sizeof(PinGpio());
}
static uint32_t Pairs(uint32_t pins) {
    uint32_t bits = 0U;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((pins & (1U << pin)) != 0U)
            bits |= 3U << (pin * 2U);
    return bits;
}
static bool Powered(void) {
    return (PinRcc().AHB2ENR & RCC_AHB2ENR_GPIOAEN) != 0U &&
           (PinRcc().AHB2RSTR & RCC_AHB2RSTR_GPIOARST) == 0U;
}
void nx_pin_state_model_counters(void) {
    auto& m = g_pin_state_model;
    m.reads = m.writes = m.gpio_reads = m.gpio_writes = 0U;
    m.forbidden = m.unmasked_writes = m.rcc_writes = 0U;
    m.unsafe_electrical = m.unsafe_drive = m.unselected_change = false;
}
void nx_pin_state_model_reset(void) {
    std::memset(&g_pin_state_model, 0, sizeof(g_pin_state_model));
    auto& m = g_pin_state_model;
    m.privileged = true;
#ifdef NX_PIN_STATE_FACTORY_MODEL
    nx_g474_model_reset();
    m.pins = 1U << 5U;
    m.expected_output_mask = m.pins;
#else
    m.pins = (1U << 5U) | (1U << 6U);
    PinRcc().AHB2ENR = RCC_AHB2ENR_GPIOAEN;
    PinGpio().MODER = 1U;
    PinGpio().ODR = 1U;
#endif
}
uint32_t nx_pin_state_model_read(volatile uint32_t* reg) {
    auto& m = g_pin_state_model;
    ++m.reads;
    if (IsGpio(reg)) {
        ++m.gpio_reads;
        if (!Powered()) {
            ++m.forbidden;
#ifdef NX_PIN_STATE_FACTORY_MODEL
            return 0U;
#endif
        }
    }
#ifdef NX_PIN_STATE_FACTORY_MODEL
    uint32_t actual = IsGpio(reg) ? *reg : nx_g474_model_read(reg);
#else
    uint32_t actual = *reg;
#endif
    if (m.after_read != nullptr)
        m.after_read(reg);
    return actual;
}
void nx_pin_state_model_write(volatile uint32_t* reg, uint32_t value) {
    auto& m = g_pin_state_model;
    ++m.writes;
    if (!nx_arch_irq_is_masked())
        ++m.unmasked_writes;
    if (!IsGpio(reg)) {
        ++m.rcc_writes;
#ifdef NX_PIN_STATE_FACTORY_MODEL
        if (m.refuse_write != reg)
            nx_g474_model_write(reg, value);
        if (m.after_write != nullptr)
            m.after_write(reg, value);
#else
        *reg = value;
#endif
        return;
    }
    ++m.gpio_writes;
    if (!Powered()) {
        ++m.forbidden;
        return;
    }
    uint32_t pairs = Pairs(m.pins);
    bool changing_fields = reg != &PinGpio().MODER;
    if (changing_fields && (PinGpio().MODER & pairs) != pairs)
        m.unsafe_electrical = true;
    uint32_t allowed = reg == &PinGpio().OTYPER ? m.pins : pairs;
#ifdef NX_PIN_STATE_FACTORY_MODEL
    if (reg == &PinGpio().AFR[0] || reg == &PinGpio().AFR[1]) {
        allowed = 0U;
        for (unsigned pin = 0U; pin < 16U; ++pin)
            if ((m.pins & (1U << pin)) != 0U && reg == &PinGpio().AFR[pin / 8U])
                allowed |= 15U << ((pin % 8U) * 4U);
    }
#endif
    if (reg == &PinGpio().BSRR) {
        allowed = m.pins | (m.pins << 16U);
        if ((value & ~allowed) != 0U)
            m.unselected_change = true;
    } else if (((*reg ^ value) & ~allowed) != 0U) {
        m.unselected_change = true;
    }
    if (m.refuse_write == reg)
        return;
    *reg = value;
    if (reg == &PinGpio().BSRR)
        PinGpio().ODR = (PinGpio().ODR | (value & 0xffffU)) & ~(value >> 16U);
    if (reg == &PinGpio().MODER) {
        bool drives = false;
        for (unsigned pin = 0U; pin < 16U; ++pin)
            if ((m.pins & (1U << pin)) != 0U &&
                ((value >> (pin * 2U)) & 3U) == 1U)
                drives = true;
        if (drives &&
            (PinGpio().ODR & m.expected_output_mask) != m.expected_levels)
            m.unsafe_drive = true;
    }
    if (m.after_write != nullptr)
        m.after_write(reg, value);
}
bool nx_pin_state_model_privileged(void) {
    return g_pin_state_model.privileged;
}
bool nx_pin_state_model_isr(void) {
    return g_pin_state_model.isr;
}
