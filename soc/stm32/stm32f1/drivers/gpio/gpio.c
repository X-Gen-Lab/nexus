/**
 * \file            gpio.c
 * \brief           STM32F1 GPIO and reversible CRL/CRH pad ownership.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_gpio.h"
#include "system/platform.h"

#ifndef NX_STM32F1_GPIO_BSRR
#define NX_STM32F1_GPIO_BSRR(reg, value) ((reg)->BSRR = (value))
#endif

/** \brief Find the APB2 gate supplied by the selected F103 device header. */
static uint32_t gpio_gate(const GPIO_TypeDef* registers) {
    if (registers == GPIOA) {
        return RCC_APB2ENR_IOPAEN;
    }
    if (registers == GPIOB) {
        return RCC_APB2ENR_IOPBEN;
    }
    if (registers == GPIOC) {
        return RCC_APB2ENR_IOPCEN;
    }
    if (registers == GPIOD) {
        return RCC_APB2ENR_IOPDEN;
    }
#if defined(GPIOE) && defined(RCC_APB2ENR_IOPEEN)
    if (registers == GPIOE) {
        return RCC_APB2ENR_IOPEEN;
    }
#endif
    return 0U;
}

/** \brief Expand eight pin bits into the corresponding CRL/CRH fields. */
static uint32_t cr_mask(uint32_t pins) {
    uint32_t result = 0U;
    for (unsigned pin = 0U; pin < 8U; ++pin) {
        if ((pins & (1U << pin)) != 0U) {
            result |= 0xFU << (4U * pin);
        }
    }
    return result;
}

/** \brief Avoid a CRL/CRH write when it contains no owned field. */
static bool write_fields(volatile uint32_t* reg, uint32_t mask,
                         uint32_t value) {
    return mask == 0U || nx_stm32f1_write_masked(reg, mask, value);
}

/** \brief Acquire a pad group without writing its configuration. */
nx_result_t nx_stm32f1_pads_capture(nx_stm32f1_pads_t* pads,
                                    GPIO_TypeDef* registers, uint16_t mask) {
    uint32_t gate = gpio_gate(registers);
    if (gate == 0U || mask == 0U || pads == NULL) {
        return NX_ERROR_INVALID;
    }
    if (pads->registers != NULL) {
        return NX_ERROR_STATE;
    }
    pads->registers = registers;
    pads->mask = mask;
    pads->gate = gate;
    pads->gate_added = (RCC->APB2ENR & gate) == 0U;
    if (pads->gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    pads->low = registers->CRL;
    pads->high = registers->CRH;
    pads->latch = LL_GPIO_ReadOutputPort(registers);
    pads->captured = true;
    return NX_SUCCESS;
}

/** \brief Update only owned fields, preloading the latch before enabling I/O.
 */
nx_result_t nx_stm32f1_pads_apply(const nx_stm32f1_pads_t* pads, uint32_t low,
                                  uint32_t high, uint32_t latch) {
    GPIO_TypeDef* registers = pads->registers;
    const uint32_t owned = pads->mask;
    const uint32_t low_mask = cr_mask(owned & 0xFFU);
    const uint32_t high_mask = cr_mask(owned >> 8U);
    const uint32_t floating = UINT32_C(0x44444444);
    if (!write_fields(&registers->CRL, low_mask, floating) ||
        !write_fields(&registers->CRH, high_mask, floating)) {
        return NX_ERROR_IO;
    }
    NX_STM32F1_GPIO_BSRR(registers,
                         (latch & owned) | ((owned & ~latch) << 16U));
    nx_arch_dsb();
    if ((LL_GPIO_ReadOutputPort(registers) & owned) != (latch & owned) ||
        !write_fields(&registers->CRL, low_mask, low) ||
        !write_fields(&registers->CRH, high_mask, high)) {
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief Relinquish only this pad group, then its originally acquired gate.
 * \note Platform releases consumers in reverse acquisition order.
 */
nx_result_t nx_stm32f1_pads_restore(nx_stm32f1_pads_t* pads) {
    if (pads->registers == NULL) {
        return NX_SUCCESS;
    }
    if (pads->captured) {
        nx_result_t restored =
            nx_stm32f1_pads_apply(pads, pads->low, pads->high, pads->latch);
        if (restored != NX_SUCCESS) {
            return restored;
        }
        pads->captured = false;
    }
    if (pads->gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, pads->gate, 0U)) {
        return NX_ERROR_IO;
    }
    pads->registers = NULL;
    return NX_SUCCESS;
}

/** \brief Validate the pads and checked electrical/initial-output selection. */
nx_result_t nx_stm32f1_gpio_admit(const nx_stm32f1_gpio_t* state,
                                  const nx_stm32f1_gpio_config_t* config) {
    if (state == NULL || config == NULL || gpio_gate(config->registers) == 0U ||
        config->mask == 0U || config->mask > UINT16_MAX ||
        (config->initial & ~config->mask) != 0U || config->pull > 2U ||
        config->output_type > 1U || config->speed > 3U ||
        (config->output && config->pull != 0U) ||
        (!config->output &&
         (config->initial != 0U || config->output_type != 0U ||
          config->speed != 0U))) {
        return NX_ERROR_INVALID;
    }
    if (config->output && config->registers == GPIOC &&
        (config->mask & UINT32_C(0xE000)) != 0U && config->speed > 1U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    return state->config == NULL ? NX_SUCCESS : NX_ERROR_STATE;
}

/** \brief Choose the CR field from generated electrical properties. */
static uint32_t output_field(const nx_stm32f1_gpio_config_t* config,
                             uint32_t saved, unsigned pin) {
    static const uint8_t speed[] = {0U, LL_GPIO_SPEED_FREQ_LOW,
                                    LL_GPIO_SPEED_FREQ_MEDIUM,
                                    LL_GPIO_SPEED_FREQ_HIGH};
    if (config->speed != 0U) {
        saved = speed[config->speed];
    }
    if (saved == 0U || (config->registers == GPIOC && pin >= 13U)) {
        saved = LL_GPIO_SPEED_FREQ_LOW;
    }
    return saved | (config->output_type == 0U ? LL_GPIO_OUTPUT_PUSHPULL
                                              : LL_GPIO_OUTPUT_OPENDRAIN);
}

/** \brief Acquire the cold receipt and expose the face only after readback. */
nx_result_t nx_stm32f1_gpio_prepare(nx_stm32f1_gpio_t* state,
                                    const nx_stm32f1_gpio_config_t* config) {
    nx_result_t checked = nx_stm32f1_gpio_admit(state, config);
    if (checked != NX_SUCCESS) {
        return checked;
    }
    if (!nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    state->config = config;
    nx_result_t acquired = nx_stm32f1_pads_capture(
        &state->pads, config->registers, (uint16_t)config->mask);
    if (acquired != NX_SUCCESS) {
        return acquired;
    }
    uint32_t cr[2] = {0U, 0U};
    const uint32_t old[2] = {state->pads.low, state->pads.high};
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        const unsigned bank = pin / 8U;
        const unsigned shift = (pin % 8U) * 4U;
        uint32_t field =
            config->pull == 0U ? LL_GPIO_MODE_FLOATING : LL_GPIO_MODE_INPUT;
        if (config->output) {
            field = output_field(config, (old[bank] >> shift) & 3U, pin);
        }
        cr[bank] |= field << shift;
    }
    const uint32_t latch = config->output       ? config->initial
                           : config->pull == 1U ? config->mask
                           : config->pull == 2U ? 0U
                                                : state->pads.latch;
    nx_result_t prepared =
        nx_stm32f1_pads_apply(&state->pads, cr[0], cr[1], latch);
    state->live = prepared == NX_SUCCESS;
    return prepared;
}

/** \brief Close the face and retry the same receipt on restore failure. */
nx_result_t nx_stm32f1_gpio_close(nx_stm32f1_gpio_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    state->live = false;
    nx_result_t restored = nx_stm32f1_pads_restore(&state->pads);
    if (restored == NX_SUCCESS) {
        state->config = NULL;
    }
    return restored;
}

/** \brief Set and reset authorized outputs with one atomic BSRR store. */
static nx_result_t gpio_write(void* context, uint32_t set, uint32_t reset) {
    const nx_stm32f1_gpio_t* state = context;
    if ((set & reset) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (!state->live) {
        return NX_ERROR_STATE;
    }
    if (!state->config->output ||
        ((set | reset) & ~state->config->mask) != 0U) {
        return NX_ERROR_PERMISSION;
    }
    if ((set | reset) != 0U) {
        NX_STM32F1_GPIO_BSRR(state->config->registers, set | (reset << 16U));
    }
    return NX_SUCCESS;
}

/** \brief Sample only authorized pins with one IDR read. */
static nx_result_t gpio_read(const void* context, uint32_t* value) {
    const nx_stm32f1_gpio_t* state = context;
    if (value == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!state->live) {
        return NX_ERROR_STATE;
    }
    *value =
        LL_GPIO_ReadInputPort(state->config->registers) & state->config->mask;
    return NX_SUCCESS;
}

/** \brief Derive the mixed BSRR word under the single-writer contract. */
static nx_result_t gpio_toggle(void* context, uint32_t mask) {
    const nx_stm32f1_gpio_t* state = context;
    if (!state->live) {
        return NX_ERROR_STATE;
    }
    if (!state->config->output || (mask & ~state->config->mask) != 0U) {
        return NX_ERROR_PERMISSION;
    }
    uint32_t high = LL_GPIO_ReadOutputPort(state->config->registers) & mask;
    return gpio_write(context, mask & ~high, high);
}

const nx_gpio_ops_t nx_stm32f1_gpio_ops = {
    .write = gpio_write, .read = gpio_read, .toggle = gpio_toggle};
