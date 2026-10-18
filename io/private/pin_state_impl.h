/**
 * \file            pin_state_impl.h
 * \brief           Immutable one-port GPIO plans and private transition state
 * \author          Nexus Team
 */
#ifndef NX_PIN_STATE_IMPL_H
#define NX_PIN_STATE_IMPL_H
#include "nexus/io/pin_state.h"
/** \brief Normalized owned-bit fields; AF and reserved pull encodings excluded.
 */
typedef struct {
    uint32_t mode;
    uint32_t output_type;
    uint32_t speed;
    uint32_t pull;
    uint32_t levels;
} nx_pin_state_value_t;
/** \brief Static, immutable two-state plan for one exact authorized pad mask.
 */
typedef struct {
    uint32_t pins;
    nx_pin_state_value_t states[2];
} nx_pin_state_plan_t;
/** \brief Mode-only state, never added to an ordinary GPIO provider. */
typedef struct {
    bool initialized;
    bool retained;
    bool dirty;
    uint8_t committed;
} nx_pin_state_control_t;

#ifdef NX_PIN_STATE_IMPLEMENTATION
#include "nexus/arch/arch.h"
#ifndef NX_PIN_READ
#define NX_PIN_READ(reg) (*(reg))
#endif
#ifndef NX_PIN_WRITE
#define NX_PIN_WRITE(reg, value) (*(reg) = (value))
#endif
/** \brief Stack-only typed-bank view; no register operations table or owner. */
typedef struct {
    volatile uint32_t* gate;
    volatile uint32_t* reset;
    volatile uint32_t* mode;
    volatile uint32_t* type;
    volatile uint32_t* speed;
    volatile uint32_t* pull;
    volatile uint32_t* levels;
    volatile uint32_t* latch;
    volatile uint32_t* lock;
    uint32_t bank_mask;
} nx_pin_state_bank_t;

/** \brief All software pad/RCC writers must obey the caller's lifetime lease.
 */
static bool nx_pin_bank_ready(const nx_pin_state_bank_t* bank) {
    return (NX_PIN_READ(bank->gate) & bank->bank_mask) != 0U &&
           (NX_PIN_READ(bank->reset) & bank->bank_mask) == 0U;
}
/** \brief A missing gate/reset condition forbids even a GPIO read. */
static nx_result_t nx_pin_read(const nx_pin_state_bank_t* bank,
                               volatile uint32_t* reg, uint32_t* value) {
    if (!nx_pin_bank_ready(bank))
        return NX_ERROR_STATE;
    *value = NX_PIN_READ(reg);
    return NX_SUCCESS;
}
/** \brief A successful attempt is insufficient; actual readback is required. */
static nx_result_t nx_pin_write(const nx_pin_state_bank_t* bank,
                                volatile uint32_t* reg, uint32_t value,
                                volatile uint32_t* observed, uint32_t mask,
                                uint32_t expected) {
    if (!nx_pin_bank_ready(bank))
        return NX_ERROR_STATE;
    NX_PIN_WRITE(reg, value);
    nx_arch_dsb();
    uint32_t actual;
    nx_result_t result = nx_pin_read(bank, observed, &actual);
    return result != NX_SUCCESS
               ? result
               : ((actual & mask) == expected ? NX_SUCCESS : NX_ERROR_IO);
}
/** \brief Preserve every unselected field in a full-word register. */
static nx_result_t nx_pin_replace(const nx_pin_state_bank_t* bank,
                                  volatile uint32_t* reg, uint32_t mask,
                                  uint32_t value) {
    uint32_t current;
    nx_result_t result = nx_pin_read(bank, reg, &current);
    return result != NX_SUCCESS
               ? result
               : nx_pin_write(bank, reg, (current & ~mask) | value, reg, mask,
                              value);
}
/** \brief Constant-size admission validates both targets before any MMIO. */
static nx_result_t nx_pin_plan_valid(const nx_pin_state_plan_t* plan,
                                     uint32_t* pairs) {
    if (plan->pins == 0U || (plan->pins & ~0xffffU) != 0U)
        return NX_ERROR_INVALID;
    *pairs = 0U;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((plan->pins & (1U << pin)) != 0U)
            *pairs |= 3U << (2U * pin);
    for (unsigned state = 0U; state < 2U; ++state) {
        const nx_pin_state_value_t* value = &plan->states[state];
        if (((value->mode | value->speed | value->pull) & ~*pairs) != 0U ||
            ((value->output_type | value->levels) & ~plan->pins) != 0U)
            return NX_ERROR_INVALID;
        uint32_t outputs = 0U;
        for (unsigned pin = 0U; pin < 16U; ++pin) {
            if ((plan->pins & (1U << pin)) == 0U)
                continue;
            uint32_t mode = (value->mode >> (2U * pin)) & 3U;
            if (mode == 2U)
                return NX_ERROR_UNSUPPORTED;
            if (((value->pull >> (2U * pin)) & 3U) == 3U)
                return NX_ERROR_INVALID;
            if (mode == 1U)
                outputs |= 1U << pin;
        }
        if ((value->levels & ~outputs) != 0U)
            return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
/** \brief Clean plans must still match the committed owned image. */
static nx_result_t nx_pin_image(const nx_pin_state_bank_t* bank,
                                const nx_pin_state_plan_t* plan, uint32_t pairs,
                                unsigned index) {
    const nx_pin_state_value_t* value = &plan->states[index];
    volatile uint32_t* regs[] = {bank->mode, bank->type, bank->speed,
                                 bank->pull};
    uint32_t masks[] = {pairs, plan->pins, pairs, pairs};
    uint32_t expected[] = {value->mode, value->output_type, value->speed,
                           value->pull};
    for (unsigned i = 0U; i < 4U; ++i) {
        uint32_t actual;
        nx_result_t result = nx_pin_read(bank, regs[i], &actual);
        if (result != NX_SUCCESS)
            return result;
        if ((actual & masks[i]) != expected[i])
            return NX_ERROR_STATE;
    }
    uint32_t outputs = 0U;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((plan->pins & (1U << pin)) != 0U &&
            ((value->mode >> (2U * pin)) & 3U) == 1U)
            outputs |= 1U << pin;
    uint32_t actual;
    nx_result_t result = nx_pin_read(bank, bank->levels, &actual);
    return result != NX_SUCCESS
               ? result
               : ((actual & outputs) == value->levels ? NX_SUCCESS
                                                      : NX_ERROR_STATE);
}
/** \brief This conservative lock test never takes over a selected lock bit. */
static nx_result_t nx_pin_unlocked(const nx_pin_state_bank_t* bank,
                                   uint32_t pins) {
    uint32_t locked;
    nx_result_t result = nx_pin_read(bank, bank->lock, &locked);
    return result != NX_SUCCESS
               ? result
               : ((locked & pins) == 0U ? NX_SUCCESS : NX_ERROR_BUSY);
}
/** \brief Withdraw the entire group before any electrical field or latch. */
static nx_result_t nx_pin_apply(const nx_pin_state_bank_t* bank,
                                const nx_pin_state_plan_t* plan,
                                nx_pin_state_control_t* control, uint32_t pairs,
                                unsigned target) {
    control->retained = true;
    control->dirty = true;
    nx_result_t result = nx_pin_replace(bank, bank->mode, pairs, pairs);
    if (result != NX_SUCCESS)
        return result;
    const nx_pin_state_value_t* value = &plan->states[target];
    uint32_t outputs = 0U;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((plan->pins & (1U << pin)) != 0U &&
            ((value->mode >> (2U * pin)) & 3U) == 1U)
            outputs |= 1U << pin;
    if (outputs != 0U) {
        result =
            nx_pin_write(bank, bank->latch,
                         value->levels | ((outputs & ~value->levels) << 16U),
                         bank->levels, outputs, value->levels);
        if (result != NX_SUCCESS)
            return result;
    }
    result = nx_pin_replace(bank, bank->type, plan->pins, value->output_type);
    if (result == NX_SUCCESS)
        result = nx_pin_replace(bank, bank->speed, pairs, value->speed);
    if (result == NX_SUCCESS)
        result = nx_pin_replace(bank, bank->pull, pairs, value->pull);
    if (result == NX_SUCCESS)
        result = nx_pin_replace(bank, bank->mode, pairs, value->mode);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_pin_bank_ready(bank))
        return NX_ERROR_STATE;
    control->committed = (uint8_t)target;
    control->dirty = false;
    return NX_SUCCESS;
}
#endif
#endif
