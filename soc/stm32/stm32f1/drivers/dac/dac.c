/**
 * \file            dac.c
 * \brief           STM32F1 PA4 direct DAC output.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "drivers/dac/instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_dac.h"
#include "system/platform.h"

#if !defined(STM32F100xB) && !defined(STM32F103xE) && !defined(STM32F103xG)
#error "DAC PA4 direct mode requires a reviewed STM32F1 device"
#endif

#define PA4_MASK   (UINT16_C(1) << 4U)
#define POLL_LIMIT 4096U

enum { GATE = 1U, READY = 2U, ENABLED = 4U, RUNNING = 8U, FAULT = 16U };

static bool clock_on(void) {
    return (RCC->APB1ENR & RCC_APB1ENR_DACEN) != 0U;
}

static uint32_t direct_control(const nx_stm32f1_dac_t* state) {
    return state->buffered ? 0U : DAC_CR_BOFF1;
}

static uint16_t maximum(nx_dac_format_t format) {
    return format == NX_DAC_FORMAT_U8 ? UINT16_C(255) : UINT16_C(4095);
}

static bool write_code(const nx_stm32f1_dac_t* state, uint16_t code) {
    volatile uint32_t* target = &DAC->DHR12R1;
    uint32_t encoded = code;
    if (state->format == NX_DAC_FORMAT_U8) {
        target = &DAC->DHR8R1;
    } else if (state->format == NX_DAC_FORMAT_U12_LEFT) {
        target = &DAC->DHR12L1;
        encoded <<= 4U;
    }
    NX_STM32F1_WRITE(target, encoded);
    nx_arch_dsb();
    return *target == encoded;
}

static uint16_t read_code(const nx_stm32f1_dac_t* state) {
    const uint32_t code = DAC->DOR1 & DAC_DOR1_DACC1DOR;
    return (uint16_t)(state->format == NX_DAC_FORMAT_U8 ? code >> 4U : code);
}

nx_result_t nx_stm32f1_dac_prepare(nx_stm32f1_dac_t* state,
                                   nx_dac_format_t format, uint16_t idle,
                                   bool buffered) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL ||
        (format != NX_DAC_FORMAT_U8 && format != NX_DAC_FORMAT_U12_RIGHT &&
         format != NX_DAC_FORMAT_U12_LEFT) ||
        idle > maximum(format))
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_system_live() ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1)) !=
            RCC_CFGR_SWS_HSI)
        return NX_ERROR_STATE;
    if (state->effects != 0U || state->pads.registers != NULL || clock_on() ||
        (RCC->APB1RSTR & RCC_APB1RSTR_DACRST) != 0U)
        return NX_ERROR_BUSY;
    state->effects = GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_DACEN,
                                 RCC_APB1ENR_DACEN))
        return NX_ERROR_IO;
    if (DAC->CR != 0U)
        return NX_ERROR_BUSY;
#if defined(DAC_SR_DMAUDR1)
    if (DAC->SR != 0U)
        return NX_ERROR_BUSY;
#endif
    nx_result_t result = nx_stm32f1_pads_capture(&state->pads, GPIOA, PA4_MASK);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_pads_apply(&state->pads, 0U, 0U, state->pads.latch);
    if (result != NX_SUCCESS)
        return result;
    state->idle = idle;
    state->format = format;
    state->buffered = buffered;
    state->effects |= READY;
    return NX_SUCCESS;
}

static bool control_matches(const nx_stm32f1_dac_t* state) {
    if (!clock_on())
        return false;
    const uint32_t current = DAC->CR;
    const uint32_t mode = direct_control(state);
    return current == 0U ||
           ((state->effects & ENABLED) != 0U &&
            (current == mode || current == (mode | DAC_CR_EN1)));
}

nx_result_t nx_stm32f1_dac_close(nx_stm32f1_dac_t* state) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if ((state->effects & ENABLED) != 0U) {
        if (!control_matches(state) ||
            !nx_stm32f1_write_masked(&DAC->CR, DAC_CR_BOFF1 | DAC_CR_EN1, 0U))
            return NX_ERROR_IO;
        state->effects &= (uint8_t) ~(ENABLED | RUNNING);
    }
    state->effects &= (uint8_t)~READY;
    nx_result_t result = nx_stm32f1_pads_restore(&state->pads);
    if (result != NX_SUCCESS)
        return result;
    if ((state->effects & GATE) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_DACEN, 0U))
        return NX_ERROR_IO;
    *state = (nx_stm32f1_dac_t){0};
    return NX_SUCCESS;
}

static bool independent(const nx_stm32f1_dac_t* state, const void* output,
                        size_t bytes, size_t alignment) {
    return nx_io_dispatch_span(output, bytes, alignment) &&
           !nx_io_dispatch_overlap(output, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(output, bytes, DAC, sizeof(*DAC)) &&
           !nx_io_dispatch_overlap(output, bytes, RCC, sizeof(*RCC)) &&
           !nx_io_dispatch_overlap(output, bytes, GPIOA, sizeof(*GPIOA));
}

static nx_result_t info(const void* context, nx_dac_info_t* output) {
    const nx_stm32f1_dac_t* state = context;
    if ((state->effects & READY) == 0U)
        return NX_ERROR_STATE;
    if (!independent(state, output, sizeof(*output), _Alignof(nx_dac_info_t)))
        return NX_ERROR_INVALID;
    *output = (nx_dac_info_t){.format = state->format,
                              .maximum_code = maximum(state->format),
                              .channel_mask = NX_DAC_CHANNEL_1,
                              .output_buffer = state->buffered,
                              .code_bits =
                                  state->format == NX_DAC_FORMAT_U8 ? 8U : 12U};
    return NX_SUCCESS;
}

static nx_result_t start(void* context, uint32_t hz, nx_time_us_t deadline) {
    nx_stm32f1_dac_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if ((state->effects & (READY | FAULT)) != READY)
        return NX_ERROR_STATE;
    if ((state->effects & RUNNING) != 0U)
        return NX_ERROR_BUSY;
    if (hz != 0U)
        return NX_ERROR_INVALID;
    nx_time_us_t settle;
    /* The reviewed DAC characteristics allow at most 10 us at the stated load.
     */
    if (nx_deadline_finite_after(nx_time_now_us(), 12U, &settle) != NX_SUCCESS)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(deadline, settle))
        return NX_ERROR_TIMEOUT;
    if (!control_matches(state) || !write_code(state, state->idle))
        return NX_ERROR_IO;
    state->effects |= ENABLED;
    if (!nx_stm32f1_write_masked(&DAC->CR, DAC_CR_BOFF1 | DAC_CR_EN1,
                                 direct_control(state) | DAC_CR_EN1) ||
        nx_deadline_finite_after(nx_time_now_us(), 12U, &settle) !=
            NX_SUCCESS) {
        state->effects |= FAULT;
        return NX_ERROR_IO;
    }
    for (unsigned attempt = 0U; attempt < POLL_LIMIT; ++attempt) {
        const nx_time_us_t now = nx_time_now_us();
        if (nx_deadline_expired(deadline, now))
            break;
        if (now >= settle && read_code(state) == state->idle) {
            state->effects |= RUNNING;
            return NX_SUCCESS;
        }
    }
    state->effects |= FAULT;
    return NX_ERROR_TIMEOUT;
}

static nx_result_t write(void* context, uint8_t channel, uint16_t code) {
    nx_stm32f1_dac_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if ((state->effects & (RUNNING | FAULT)) != RUNNING)
        return NX_ERROR_STATE;
    if (channel != NX_DAC_CHANNEL_1)
        return NX_ERROR_PERMISSION;
    if (code > maximum(state->format))
        return NX_ERROR_INVALID;
    if (!control_matches(state) ||
        LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_1) == 0U ||
        !write_code(state, code)) {
        state->effects |= FAULT;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

static nx_result_t stop(void* context, nx_time_us_t deadline) {
    nx_stm32f1_dac_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if ((state->effects & READY) == 0U)
        return NX_ERROR_STATE;
    if ((state->effects & ENABLED) == 0U)
        return NX_SUCCESS;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (!control_matches(state) ||
        LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_1) == 0U ||
        !write_code(state, state->idle))
        return NX_ERROR_IO;
    for (unsigned attempt = 0U; attempt < POLL_LIMIT; ++attempt) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            break;
        if (read_code(state) == state->idle) {
            state->effects &= (uint8_t) ~(RUNNING | FAULT);
            return NX_SUCCESS;
        }
    }
    state->effects |= FAULT;
    return NX_ERROR_TIMEOUT;
}

static nx_result_t state(const void* context, nx_dac_state_t* output) {
    const nx_stm32f1_dac_t* instance = context;
    if ((instance->effects & READY) == 0U)
        return NX_ERROR_STATE;
    if (!independent(instance, output, sizeof(*output),
                     _Alignof(nx_dac_state_t)))
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!control_matches(instance))
        return NX_ERROR_IO;
    const bool enabled = LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_1) != 0U;
    *output = (nx_dac_state_t){
        .output_codes = {enabled ? read_code(instance) : 0U, 0U},
        .enabled_channels = enabled ? NX_DAC_CHANNEL_1 : 0U,
        .running = (instance->effects & RUNNING) != 0U,
        .faulted = (instance->effects & FAULT) != 0U,
        .code_bits = instance->format == NX_DAC_FORMAT_U8 ? 8U : 12U};
    return NX_SUCCESS;
}

const nx_dac_ops_t nx_stm32f1_dac_ops = {
    .info = info,
    .write = write,
    .start = start,
    .stop = stop,
    .state = state,
};
