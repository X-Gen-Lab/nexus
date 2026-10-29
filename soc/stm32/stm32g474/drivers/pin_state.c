/**
 * \file            pin_state.c
 * \brief           Bounded G474 GPIO-state transitions under a static lease
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define NX_PIN_STATE_IMPLEMENTATION
#include "stm32g474_pin_state.h"

static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Trusted construction supplies ownership, not matching register bits.
 */
static bool valid(const nx_stm32g474_pin_state_t* state) {
    if (!span(state, sizeof(*state), _Alignof(nx_stm32g474_pin_state_t)) ||
        !span(state->gpio, sizeof(*state->gpio), _Alignof(GPIO_TypeDef)) ||
        !span(state->rcc, sizeof(*state->rcc), _Alignof(RCC_TypeDef)) ||
        !span(state->plan, sizeof(*state->plan),
              _Alignof(nx_pin_state_plan_t)) ||
        state->bank_mask == 0U || (state->bank_mask & ~0x6fU) != 0U ||
        (state->bank_mask & (state->bank_mask - 1U)) != 0U)
        return false;
    const void* storage[] = {state, state->plan, state->gpio, state->rcc};
    size_t sizes[] = {sizeof(*state), sizeof(*state->plan),
                      sizeof(*state->gpio), sizeof(*state->rcc)};
    for (unsigned i = 0U; i < 4U; ++i)
        for (unsigned j = i + 1U; j < 4U; ++j)
            if (overlap(storage[i], sizes[i], storage[j], sizes[j]))
                return false;
#ifndef NX_PIN_STATE_MODEL
    if (state->rcc != RCC)
        return false;
    GPIO_TypeDef* expected = NULL;
    switch (state->bank_mask) {
        case RCC_AHB2ENR_GPIOAEN:
            expected = GPIOA;
            break;
        case RCC_AHB2ENR_GPIOBEN:
            expected = GPIOB;
            break;
        case RCC_AHB2ENR_GPIOCEN:
            expected = GPIOC;
            break;
        case RCC_AHB2ENR_GPIODEN:
            expected = GPIOD;
            break;
        case RCC_AHB2ENR_GPIOFEN:
            expected = GPIOF;
            break;
        case RCC_AHB2ENR_GPIOGEN:
            expected = GPIOG;
            break;
        default:
            return false;
    }
    if (state->gpio != expected)
        return false;
#endif
    return true;
}
static nx_pin_state_bank_t bank(nx_stm32g474_pin_state_t* state) {
    nx_pin_state_bank_t result = {&state->rcc->AHB2ENR,  &state->rcc->AHB2RSTR,
                                  &state->gpio->MODER,   &state->gpio->OTYPER,
                                  &state->gpio->OSPEEDR, &state->gpio->PUPDR,
                                  &state->gpio->ODR,     &state->gpio->BSRR,
                                  &state->gpio->LCKR,    state->bank_mask};
    return result;
}
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static nx_result_t select_state(void* context,
                                nx_pin_state_selection_t target) {
    if (!task())
        return NX_ERROR_CONTEXT;
    nx_stm32g474_pin_state_t* state = context;
    if (!valid(state) ||
        (target != NX_PIN_STATE_DEFAULT && target != NX_PIN_STATE_SLEEP))
        return NX_ERROR_INVALID;
    if (!state->control.initialized || !state->control.retained)
        return NX_ERROR_STATE;
    uint32_t pairs;
    nx_result_t result = nx_pin_plan_valid(state->plan, &pairs);
    if (result != NX_SUCCESS)
        return result;
    nx_pin_state_bank_t view = bank(state);
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    result = nx_pin_unlocked(&view, state->plan->pins);
    if (result == NX_SUCCESS && !state->control.dirty) {
        result = state->control.committed < 2U
                     ? nx_pin_image(&view, state->plan, pairs,
                                    state->control.committed)
                     : NX_ERROR_STATE;
        if (result != NX_SUCCESS)
            state->control.dirty = true;
    }
    if (result == NX_SUCCESS &&
        (state->control.dirty || state->control.committed != (uint8_t)target))
        result = nx_pin_apply(&view, state->plan, &state->control, pairs,
                              (unsigned)target);
    if (result == NX_SUCCESS && !nx_pin_bank_ready(&view)) {
        state->control.dirty = true;
        result = NX_ERROR_STATE;
    }
    nx_arch_irq_restore(previous);
    return result;
}
nx_result_t nx_stm32g474_pin_state_initialize(nx_stm32g474_pin_state_t* state) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(state))
        return NX_ERROR_INVALID;
    if (state->control.initialized || state->control.retained)
        return NX_ERROR_BUSY;
    uint32_t pairs;
    nx_result_t result = nx_pin_plan_valid(state->plan, &pairs);
    if (result != NX_SUCCESS)
        return result;
    nx_pin_state_bank_t view = bank(state);
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    result = nx_pin_unlocked(&view, state->plan->pins);
    uint32_t mode;
    if (result == NX_SUCCESS)
        result = nx_pin_read(&view, view.mode, &mode);
    if (result == NX_SUCCESS)
        for (unsigned pin = 0U; pin < 16U; ++pin)
            if ((state->plan->pins & (1U << pin)) != 0U) {
                uint32_t field = (mode >> (2U * pin)) & 3U;
                if (field == 1U || field == 2U)
                    result = NX_ERROR_BUSY;
            }
    if (result == NX_SUCCESS) {
        state->control.committed = UINT8_MAX;
        result = nx_pin_apply(&view, state->plan, &state->control, pairs, 0U);
        if (result == NX_SUCCESS)
            state->control.initialized = true;
    }
    nx_arch_irq_restore(previous);
    return result;
}
nx_result_t nx_stm32g474_pin_state_stop(nx_stm32g474_pin_state_t* state) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(state))
        return NX_ERROR_INVALID;
    if (!state->control.retained)
        return NX_SUCCESS;
    uint32_t pairs;
    nx_result_t result = nx_pin_plan_valid(state->plan, &pairs);
    if (result != NX_SUCCESS)
        return result;
    nx_pin_state_bank_t view = bank(state);
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    result = nx_pin_unlocked(&view, state->plan->pins);
    if (result == NX_SUCCESS) {
        state->control.dirty = true;
        result = nx_pin_replace(&view, view.mode, pairs, pairs);
    }
    if (result == NX_SUCCESS && !nx_pin_bank_ready(&view))
        result = NX_ERROR_STATE;
    if (result == NX_SUCCESS) {
        state->control.initialized = false;
        state->control.dirty = false;
        state->control.retained = false;
    }
    nx_arch_irq_restore(previous);
    return result;
}
const nx_pin_state_ops_t nx_stm32g474_pin_state_ops = {select_state};

/** \brief Never overwrite the immutable connection of a retained controller. */
nx_result_t
nx_stm32g474_pin_state_prepare(nx_stm32g474_pin_state_t* state,
                               const nx_stm32g474_pin_state_config_t* config) {
    if (state == NULL || config == NULL)
        return NX_ERROR_INVALID;
    if (state->control.retained || state->control.initialized)
        return NX_ERROR_BUSY;
    state->gpio = config->gpio;
    state->rcc = config->rcc;
    state->bank_mask = config->bank_mask;
    state->plan = config->plan;
    return nx_stm32g474_pin_state_initialize(state);
}
