/**
 * \file            encoder.c
 * \brief           F103 TIM3 hardware quadrature on the default PA6/PA7 route.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_tim.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "The reviewed TIM3 encoder route requires the STM32F103xB target"
#endif

enum { AFIO_GATE = 1U, TIMER_GATE = 2U, LIVE = 4U };
enum { PHASE_PINS = (1U << 6U) | (1U << 7U) };
#define PHASE_ENABLES (TIM_CCER_CC1E | TIM_CCER_CC2E)

/** \brief Return only a temporary AFIO clock taken by this instance. */
static bool release_afio(nx_stm32f1_encoder_t* state) {
    if ((state->effects & AFIO_GATE) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
            return false;
        }
        state->effects &= (uint8_t)~AFIO_GATE;
    }
    return true;
}

/** \brief Reset only the cold-exclusive TIM3 peripheral. */
static bool reset_timer(void) {
    return nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_TIM3RST,
                                   RCC_APB1RSTR_TIM3RST) &&
           nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_TIM3RST, 0U);
}

/** \brief Check both withdrawals without resetting the observed position. */
static bool withdraw(void) {
    LL_TIM_WriteReg(TIM3, DIER, 0U);
    LL_TIM_DisableCounter(TIM3);
    LL_TIM_CC_DisableChannel(TIM3, PHASE_ENABLES);
    nx_arch_dsb();
    return (TIM3->CR1 & TIM_CR1_CEN) == 0U &&
           (TIM3->CCER & PHASE_ENABLES) == 0U && TIM3->DIER == 0U;
}

/** \brief Program direct TI1/TI2, independent qualification and x4 counting. */
static bool configure_timer(const nx_stm32f1_encoder_config_t* config) {
    const uint32_t inputs = TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0 |
                            ((uint32_t)config->filter_a << TIM_CCMR1_IC1F_Pos) |
                            ((uint32_t)config->filter_b << TIM_CCMR1_IC2F_Pos);
    const uint32_t polarity = (config->invert_a ? TIM_CCER_CC1P : 0U) |
                              (config->invert_b ? TIM_CCER_CC2P : 0U);
    NX_STM32F1_WRITE(&TIM3->CCMR1, inputs);
    NX_STM32F1_WRITE(&TIM3->CCER, polarity);
    LL_TIM_SetEncoderMode(TIM3, LL_TIM_ENCODERMODE_X4_TI12);
    nx_arch_dsb();
    return TIM3->CCMR1 == inputs && TIM3->CCER == polarity &&
           TIM3->SMCR == LL_TIM_ENCODERMODE_X4_TI12 && TIM3->CR1 == 0U &&
           TIM3->CR2 == 0U && TIM3->DIER == 0U && TIM3->CCMR2 == 0U &&
           TIM3->CNT == 0U && TIM3->PSC == 0U && TIM3->ARR == UINT16_MAX;
}

nx_result_t
nx_stm32f1_encoder_prepare(nx_stm32f1_encoder_t* state,
                           const nx_stm32f1_encoder_config_t* config) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || config == NULL || config->filter_a > 15U ||
        config->filter_b > 15U) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_system_live() ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1)) !=
            RCC_CFGR_SWS_HSI) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (state->config != NULL || (RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_TIM3RST) != 0U ||
        NVIC_GetEnableIRQ(TIM3_IRQn) != 0U ||
        NVIC_GetPendingIRQ(TIM3_IRQn) != 0U ||
        NVIC_GetActive(TIM3_IRQn) != 0U) {
        return NX_ERROR_BUSY;
    }
    state->config = config;
    if ((RCC->APB2ENR & RCC_APB2ENR_AFIOEN) == 0U) {
        state->effects |= AFIO_GATE;
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN,
                                     RCC_APB2ENR_AFIOEN)) {
            return NX_ERROR_IO;
        }
    }
    const bool remapped = (AFIO->MAPR & AFIO_MAPR_TIM3_REMAP) != 0U;
    if (!release_afio(state)) {
        return NX_ERROR_IO;
    }
    if (remapped) {
        state->config = NULL;
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result =
        nx_stm32f1_pads_capture(&state->pads, GPIOA, PHASE_PINS);
    if (result == NX_SUCCESS) {
        result = nx_stm32f1_pads_apply(&state->pads, UINT32_C(0x44000000), 0U,
                                       state->pads.latch);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    state->effects |= TIMER_GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM3EN,
                                 RCC_APB1ENR_TIM3EN) ||
        !reset_timer() || !configure_timer(config)) {
        return NX_ERROR_IO;
    }
    state->effects |= LIVE;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_encoder_close(nx_stm32f1_encoder_t* state) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & TIMER_GATE) != 0U) {
        if (((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U && !withdraw()) ||
            !reset_timer() ||
            !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM3EN, 0U)) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~TIMER_GATE;
    }
    if (!release_afio(state)) {
        return NX_ERROR_IO;
    }
    const nx_result_t result = nx_stm32f1_pads_restore(&state->pads);
    if (result == NX_SUCCESS) {
        state->config = NULL;
    }
    return result;
}

static nx_result_t ready(const nx_stm32f1_encoder_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if ((state->effects & LIVE) == 0U) {
        return NX_ERROR_STATE;
    }
    return (RCC->APB1ENR & RCC_APB1ENR_TIM3EN) != 0U ? NX_SUCCESS : NX_ERROR_IO;
}

static nx_result_t start(void* context) {
    nx_result_t result = ready(context);
    if (result == NX_SUCCESS) {
        LL_TIM_CC_EnableChannel(TIM3, PHASE_ENABLES);
        LL_TIM_EnableCounter(TIM3);
        nx_arch_dsb();
        if ((TIM3->CCER & PHASE_ENABLES) != PHASE_ENABLES ||
            (TIM3->CR1 & TIM_CR1_CEN) == 0U) {
            result = NX_ERROR_IO;
        }
    }
    return result;
}

static nx_result_t stop(void* context) {
    const nx_result_t result = ready(context);
    return result == NX_SUCCESS && !withdraw() ? NX_ERROR_IO : result;
}

static bool overlaps(uintptr_t begin, uintptr_t end, const void* area,
                     size_t size) {
    return begin < (uintptr_t)area + size && (uintptr_t)area < end;
}

/** \brief An observation must leave live ownership and register windows intact.
 */
static bool separate(const nx_stm32f1_encoder_t* state, const void* output,
                     size_t bytes, size_t align) {
    const uintptr_t begin = (uintptr_t)output;
    if (output == NULL || begin % align != 0U || UINTPTR_MAX - begin < bytes) {
        return false;
    }
    const uintptr_t end = begin + bytes;
    return !overlaps(begin, end, state, sizeof(*state)) &&
           !overlaps(begin, end, state->config, sizeof(*state->config)) &&
           !overlaps(begin, end, TIM3, sizeof(*TIM3)) &&
           !overlaps(begin, end, GPIOA, sizeof(*GPIOA)) &&
           !overlaps(begin, end, RCC, sizeof(*RCC));
}

static nx_result_t read(void* context, nx_encoder_sample_t* output) {
    const nx_stm32f1_encoder_t* state = context;
    const nx_result_t result = ready(state);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!separate(state, output, sizeof(*output),
                  _Alignof(nx_encoder_sample_t))) {
        return NX_ERROR_INVALID;
    }
    /* Capture errata concern CCR reads. The encoder observes CNT once. */
    const uint32_t count = LL_TIM_GetCounter(TIM3) & UINT16_MAX;
    *output =
        (nx_encoder_sample_t){.observed_at = nx_time_now_us(), .count = count};
    return NX_SUCCESS;
}

/** \brief RM0008 ICxF interpretation with CKD=0. */
static nx_result_t describe(const void* context, nx_encoder_state_t* output) {
    static const nx_encoder_filter_t filters[] = {
        {0U, 0U},  {1U, 2U},  {1U, 4U},  {1U, 8U}, {2U, 6U},  {2U, 8U},
        {4U, 6U},  {4U, 8U},  {8U, 6U},  {8U, 8U}, {16U, 5U}, {16U, 6U},
        {16U, 8U}, {32U, 5U}, {32U, 6U}, {32U, 8U}};
    const nx_stm32f1_encoder_t* state = context;
    const nx_result_t result = ready(state);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!separate(state, output, sizeof(*output),
                  _Alignof(nx_encoder_state_t))) {
        return NX_ERROR_INVALID;
    }
    *output = (nx_encoder_state_t){
        .filter_clock_hz = 8000000U,
        .mode = NX_ENCODER_MODE_X4,
        .counter_bits = 16U,
        .filter_a = filters[state->config->filter_a],
        .filter_b = filters[state->config->filter_b],
        .running = (TIM3->CR1 & TIM_CR1_CEN) != 0U,
    };
    return NX_SUCCESS;
}

const nx_encoder_ops_t nx_stm32f1_encoder_ops = {
    .start = start, .stop = stop, .read = read, .state = describe};
