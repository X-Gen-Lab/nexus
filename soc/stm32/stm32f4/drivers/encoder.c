/**
 * \file            encoder.c
 * \brief           F407 raw quadrature counter with explicit cold ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f4_encoder.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_tim.h"

enum { NX_ENCODER_CLOCK = 1U, NX_ENCODER_CONTROLLER = 2U };

/** \brief All observations and controls belong to one serialized task. */
static bool context_ok(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Exact reviewed TI1/TI2 bank; Platform owns both pad leases. */
static GPIO_TypeDef* gpio(const nx_stm32_encoder_config_t* config) {
    return config->registers == TIM4   ? GPIOB
           : config->registers == TIM8 ? GPIOC
                                       : GPIOA;
}
/** \brief Physical phase order is fixed before optional inversion. */
static uint8_t first_pin(const nx_stm32_encoder_config_t* config) {
    return config->registers == TIM5 ? 0U : config->registers == TIM1 ? 8U : 6U;
}
/** \brief Bus helpers share the same selected timer identity as pin routing. */
static bool clock_enabled(const nx_stm32_encoder_config_t* config) {
    uint32_t gate = nx_stm32_timer_gate(config->registers);
    return nx_stm32_timer_apb2(config->registers)
               ? LL_APB2_GRP1_IsEnabledClock(gate) != 0U
               : LL_APB1_GRP1_IsEnabledClock(gate) != 0U;
}
/** \brief Only this receipt's APB gate may be changed, preserving its peers. */
static void clock_change(const nx_stm32_encoder_config_t* config, bool on) {
    uint32_t gate = nx_stm32_timer_gate(config->registers);
    if (nx_stm32_timer_apb2(config->registers)) {
        if (on)
            LL_APB2_GRP1_EnableClock(gate);
        else
            LL_APB2_GRP1_DisableClock(gate);
    } else if (on) {
        LL_APB1_GRP1_EnableClock(gate);
    } else {
        LL_APB1_GRP1_DisableClock(gate);
    }
}
/** \brief This mode publishes no IRQ and cannot adopt a foreign vector. */
static bool vector_idle(const nx_stm32_encoder_config_t* config) {
    const IRQn_Type irq = config->registers == TIM1   ? TIM1_CC_IRQn
                          : config->registers == TIM8 ? TIM8_CC_IRQn
                          : config->registers == TIM3 ? TIM3_IRQn
                          : config->registers == TIM4 ? TIM4_IRQn
                                                      : TIM5_IRQn;
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&
           NVIC_GetActive(irq) == 0U;
}
/** \brief Advanced timers also expose complementary channel enable sources. */
static uint32_t channel_enables(const nx_stm32_encoder_config_t* config) {
    return nx_stm32_timer_apb2(config->registers) ? UINT32_C(0x1555)
                                                  : UINT32_C(0x1111);
}
/** \brief Stop every owned source without resetting the modulo position. */
static nx_result_t withdraw(nx_stm32_encoder_state_t* state) {
    TIM_TypeDef* regs = state->config->registers;
    LL_TIM_DisableCounter(regs);
    LL_TIM_CC_DisableChannel(regs, channel_enables(state->config));
    LL_TIM_WriteReg(regs, DIER, 0U);
    nx_arch_dsb();
    return (regs->CR1 & TIM_CR1_CEN) == 0U &&
                   (regs->CCER & channel_enables(state->config)) == 0U &&
                   regs->DIER == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

nx_result_t nx_stm32_encoder_prepare(nx_stm32_encoder_state_t* state,
                                     const nx_stm32_encoder_config_t* config) {
    if (state == NULL || config == NULL ||
        (config->registers != TIM1 && config->registers != TIM8 &&
         config->registers != TIM3 && config->registers != TIM4 &&
         config->registers != TIM5) ||
        config->mode < NX_ENCODER_MODE_X2_TI1 ||
        config->mode > NX_ENCODER_MODE_X4_TI12 || config->filter_a > 15U ||
        config->filter_b > 15U)
        return NX_ERROR_INVALID;
    if (!context_ok())
        return NX_ERROR_CONTEXT;
    if (state->initialized || state->cold_effects != 0U ||
        clock_enabled(config) || !vector_idle(config))
        return NX_ERROR_BUSY;
    GPIO_TypeDef* bank = gpio(config);
    const uint32_t gpio_gate = bank == GPIOA   ? RCC_AHB1ENR_GPIOAEN
                               : bank == GPIOB ? RCC_AHB1ENR_GPIOBEN
                                               : RCC_AHB1ENR_GPIOCEN;
    if ((RCC->AHB1ENR & gpio_gate) == 0U)
        return NX_ERROR_STATE;
    const uint8_t pin = first_pin(config);
    if ((bank->LCKR & (3U << pin)) != 0U)
        return NX_ERROR_BUSY;
    /* Record responsibility before the first fallible hardware write. The
     * immutable descriptor remains borrowed until close proves release. */
    state->config = config;
    state->cold_effects = NX_ENCODER_CLOCK;
    clock_change(config, true);
    nx_arch_dsb();
    if (!clock_enabled(config))
        return NX_ERROR_IO;
    TIM_TypeDef* regs = config->registers;
    if ((regs->CR1 & TIM_CR1_CEN) != 0U || regs->DIER != 0U ||
        (regs->CCER & channel_enables(config)) != 0U ||
        ((regs == TIM1 || regs == TIM8) && regs->BDTR != 0U))
        return NX_ERROR_BUSY;
    state->cold_effects |= NX_ENCODER_CONTROLLER;
    for (uint8_t i = pin; i < pin + 2U; ++i) {
        nx_result_t result = nx_stm32_pin_configure(
            bank, i, 2U, (uint8_t)nx_stm32_timer_af(regs), 0U, false);
        if (result != NX_SUCCESS)
            return result;
    }
    LL_TIM_WriteReg(regs, CR1, 0U);
    LL_TIM_WriteReg(regs, CR2, 0U);
    LL_TIM_WriteReg(regs, SMCR, 0U);
    LL_TIM_WriteReg(regs, DIER, 0U);
    LL_TIM_WriteReg(regs, CCER, 0U);
    LL_TIM_WriteReg(regs, CCMR1, 0U);
    LL_TIM_WriteReg(regs, CCMR2, 0U);
    LL_TIM_SetPrescaler(regs, 0U);
    LL_TIM_SetAutoReload(regs, regs == TIM5 ? UINT32_MAX : UINT16_MAX);
    LL_TIM_SetCounter(regs, 0U);
    LL_TIM_IC_SetActiveInput(regs, LL_TIM_CHANNEL_CH1,
                             LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetActiveInput(regs, LL_TIM_CHANNEL_CH2,
                             LL_TIM_ACTIVEINPUT_DIRECTTI);
    /* LL packs IC1F in the upper halfword; TOML retains raw silicon codes. */
    LL_TIM_IC_SetFilter(regs, LL_TIM_CHANNEL_CH1,
                        (uint32_t)config->filter_a
                            << (TIM_CCMR1_IC1F_Pos + 16U));
    LL_TIM_IC_SetFilter(regs, LL_TIM_CHANNEL_CH2,
                        (uint32_t)config->filter_b
                            << (TIM_CCMR1_IC1F_Pos + 16U));
    LL_TIM_IC_SetPolarity(regs, LL_TIM_CHANNEL_CH1,
                          config->invert_a ? LL_TIM_IC_POLARITY_FALLING
                                           : LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetPolarity(regs, LL_TIM_CHANNEL_CH2,
                          config->invert_b ? LL_TIM_IC_POLARITY_FALLING
                                           : LL_TIM_IC_POLARITY_RISING);
    LL_TIM_SetEncoderMode(regs, (uint32_t)config->mode);
    LL_TIM_GenerateEvent_UPDATE(regs);
    LL_TIM_WriteReg(regs, SR, 0U);
    nx_arch_dsb();
    const uint32_t inputs = UINT32_C(0x101) |
                            ((uint32_t)config->filter_a << 4U) |
                            ((uint32_t)config->filter_b << 12U);
    const uint32_t polarity = (config->invert_a ? TIM_CCER_CC1P : 0U) |
                              (config->invert_b ? TIM_CCER_CC2P : 0U);
    if (regs->CR1 != 0U || regs->CR2 != 0U || regs->DIER != 0U ||
        regs->SMCR != (uint32_t)config->mode || regs->CCMR1 != inputs ||
        regs->CCMR2 != 0U || regs->CCER != polarity || regs->PSC != 0U ||
        regs->ARR != (regs == TIM5 ? UINT32_MAX : UINT16_MAX))
        return NX_ERROR_IO;
    for (uint8_t i = pin; i < pin + 2U; ++i) {
        if (((bank->MODER >> ((uint32_t)i * 2U)) & 3U) != 2U ||
            ((bank->AFR[i / 8U] >> ((uint32_t)(i % 8U) * 4U)) & 15U) !=
                nx_stm32_timer_af(regs) ||
            ((bank->PUPDR >> ((uint32_t)i * 2U)) & 3U) != 0U ||
            (bank->OTYPER & (1U << i)) != 0U)
            return NX_ERROR_IO;
    }
    state->initialized = true;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_encoder_close(nx_stm32_encoder_state_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!context_ok())
        return NX_ERROR_CONTEXT;
    if (state->cold_effects == 0U)
        return state->initialized ? NX_ERROR_BUSY : NX_SUCCESS;
    if (state->config == NULL)
        return NX_ERROR_STATE;
    if ((state->cold_effects & NX_ENCODER_CONTROLLER) != 0U) {
        if (!clock_enabled(state->config))
            return NX_ERROR_IO;
        nx_result_t result = withdraw(state);
        if (result != NX_SUCCESS)
            return result;
        state->initialized = false;
        state->cold_effects &= (uint8_t)~NX_ENCODER_CONTROLLER;
    }
    clock_change(state->config, false);
    nx_arch_dsb();
    if (clock_enabled(state->config))
        return NX_ERROR_IO;
    state->cold_effects = 0U;
    state->config = NULL;
    return NX_SUCCESS;
}

/** \brief Admit only initialized, clocked sessions in the owning task. */
static nx_result_t ready(const nx_stm32_encoder_state_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!context_ok())
        return NX_ERROR_CONTEXT;
    if (!state->initialized || state->config == NULL)
        return NX_ERROR_STATE;
    return clock_enabled(state->config) ? NX_SUCCESS : NX_ERROR_IO;
}
/** \brief Enable both phases without restarting the raw position. */
static nx_result_t start(void* context) {
    nx_stm32_encoder_state_t* state = context;
    nx_result_t result = ready(state);
    if (result != NX_SUCCESS)
        return result;
    TIM_TypeDef* regs = state->config->registers;
    LL_TIM_CC_EnableChannel(regs, LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2);
    LL_TIM_EnableCounter(regs);
    nx_arch_dsb();
    return (regs->CR1 & TIM_CR1_CEN) != 0U &&
                   (regs->CCER & (TIM_CCER_CC1E | TIM_CCER_CC2E)) ==
                       (TIM_CCER_CC1E | TIM_CCER_CC2E)
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
/** \brief Stop retains counter, descriptor and cold clock leases. */
static nx_result_t stop(void* context) {
    nx_stm32_encoder_state_t* state = context;
    nx_result_t result = ready(state);
    return result == NX_SUCCESS ? withdraw(state) : result;
}
/** \brief Extents have already passed nonwrapping arithmetic checks. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Outputs cannot overwrite immutable config or live owner controls. */
static bool independent(const nx_stm32_encoder_state_t* state,
                        const void* pointer, size_t bytes, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)pointer &&
           !overlap(pointer, bytes, state, sizeof(*state)) &&
           !overlap(pointer, bytes, state->config, sizeof(*state->config)) &&
           !overlap(pointer, bytes, state->config->registers,
                    sizeof(TIM_TypeDef)) &&
           !overlap(pointer, bytes, gpio(state->config),
                    sizeof(GPIO_TypeDef)) &&
           !overlap(pointer, bytes, RCC, sizeof(*RCC));
}
/** \brief One raw modulo observation, without extrapolating across wraps. */
static nx_result_t read(void* context, nx_encoder_sample_t* sample) {
    const nx_stm32_encoder_state_t* state = context;
    nx_result_t result = ready(state);
    if (result != NX_SUCCESS)
        return result;
    if (!independent(state, sample, sizeof(*sample),
                     _Alignof(nx_encoder_sample_t)))
        return NX_ERROR_INVALID;
    const uint32_t count = LL_TIM_GetCounter(state->config->registers);
    const nx_time_us_t observed = nx_time_now_us();
    *sample = (nx_encoder_sample_t){
        .count = state->config->registers == TIM5 ? count : count & UINT16_MAX,
        .observed_at = observed};
    return NX_SUCCESS;
}
/** \brief Selected filter codes remain distinct from actual enable state. */
static nx_result_t describe(const void* context, nx_encoder_state_t* result) {
    const nx_stm32_encoder_state_t* state = context;
    nx_result_t status = ready(state);
    if (status != NX_SUCCESS)
        return status;
    if (!independent(state, result, sizeof(*result),
                     _Alignof(nx_encoder_state_t)))
        return NX_ERROR_INVALID;
    const nx_stm32_encoder_config_t* config = state->config;
    *result = (nx_encoder_state_t) {
#if NEXUS_APB1_TIMER_HZ == NEXUS_APB2_TIMER_HZ
        .filter_clock_hz = NEXUS_APB1_TIMER_HZ,
#else
        .filter_clock_hz = nx_stm32_timer_apb2(config->registers)
                               ? NEXUS_APB2_TIMER_HZ
                               : NEXUS_APB1_TIMER_HZ,
#endif
        .mode = config->mode,
        .counter_bits = config->registers == TIM5 ? 32U : 16U,
        .filter_a = config->filter_a, .filter_b = config->filter_b,
        .running = LL_TIM_IsEnabledCounter(config->registers) != 0U
    };
    return NX_SUCCESS;
}
/** \brief One immutable method table, with no ring, request or worker. */
const nx_encoder_ops_t nx_stm32_encoder_ops = {
    .start = start, .stop = stop, .read = read, .state = describe};
