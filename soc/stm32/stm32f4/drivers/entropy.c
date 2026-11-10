/**
 * \file            entropy.c
 * \brief           Official-LL F407 RNG ownership and continuous health checks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "stm32f407_clock.h"
#include "stm32f407_entropy.h"
#include "stm32f407_system.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_rng.h"
#include <string.h>

/* RM0090 Rev21 sections24.3.1/24.3.2: discard the initial word, compare each
 * subsequent whole word, and never use data after seed/clock error. Official
 * HAL polling has an internal millisecond tick wait and no polling-path health
 * admission. Official LL preserves the explicit bounded word/deadline contract.
 * These checks do not establish entropy quality, FIPS approval or hardware HIL.
 */

/** \brief Validate a complete aligned nonwrapping object before access. */
static bool span(const void* pointer, size_t size, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer && address % alignment == 0U &&
           size <= UINTPTR_MAX - address;
}

/** \brief Compare complete private state and fixed register spans. */
static bool overlaps(const void* left, size_t left_size, const void* right,
                     size_t right_size) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    return left_size != 0U && right_size != 0U &&
           (a <= b ? b - a < left_size : a - b < right_size);
}

/** \brief The first provider has no ISR, NMI or masked task execution mode. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

extern nx_stm32_system_t g_nx_stm32_system;

/** \brief Only the selected live System can lend its actual nominal PLL48. */
static nx_result_t clock_profile(void) {
    const nx_stm32_system_t* system = &g_nx_stm32_system;
    if (NEXUS_PLL48_HZ != 48000000U)
        return NX_ERROR_UNSUPPORTED;
    if (!system->started || system->time_epoch == 0U || system->rcc != RCC ||
        system->flash == NULL || system->core_hz != NEXUS_CORE_HZ ||
        system->timer_error != NX_SUCCESS)
        return NX_ERROR_STATE;
    const uint32_t source = NEXUS_CLOCK_SOURCE_HSE
                                ? RCC_CR_HSEON | RCC_CR_HSERDY
                                : RCC_CR_HSION | RCC_CR_HSIRDY;
    const uint32_t ready = source | RCC_CR_PLLON | RCC_CR_PLLRDY;
    if ((RCC->CR & ready) != ready)
        return NX_ERROR_STATE;
    return nx_stm32_clock_profile_matches(RCC, system->flash)
               ? NX_SUCCESS
               : NX_ERROR_UNSUPPORTED;
}

/** \brief A retained RNG lease never crosses a System time-domain change. */
static bool clock_stable(const nx_stm32f407_entropy_state_t* state) {
    return clock_profile() == NX_SUCCESS &&
           state->time_epoch == g_nx_stm32_system.time_epoch;
}

/** \brief Guard the RCC shared-register update and retain ownership on error.
 */
static void gate_enable(void) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_RNG);
    nx_arch_dsb();
    nx_arch_irq_restore(saved);
}

/** \brief Release only the gate acquired by this exact owner. */
static void gate_disable(void) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_RNG);
    nx_arch_dsb();
    nx_arch_irq_restore(saved);
}

/** \brief Reject foreign live RNG before System can restart its clock. */
nx_result_t
nx_stm32f407_entropy_admit(const nx_stm32f407_entropy_state_t* state) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!span(state, sizeof(*state), _Alignof(nx_stm32f407_entropy_state_t)))
        return NX_ERROR_INVALID;
    if (state->running || state->gate_owned || state->rng_owned)
        return NX_ERROR_BUSY;
    if (LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_RNG) &&
        (LL_RNG_IsEnabled(RNG) || LL_RNG_IsEnabledIT(RNG)))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

/** \brief Admit the exact profile before acquiring the RNG controller. */
nx_result_t nx_stm32f407_entropy_start(nx_stm32f407_entropy_state_t* state) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!span(state, sizeof(*state), _Alignof(nx_stm32f407_entropy_state_t)))
        return NX_ERROR_INVALID;
    if (state->running || state->gate_owned || state->rng_owned)
        return NX_ERROR_BUSY;
    nx_result_t result = clock_profile();
    if (result != NX_SUCCESS)
        return result;
    bool enabled = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_RNG) != 0U;
    if (enabled && (LL_RNG_IsEnabled(RNG) || LL_RNG_IsEnabledIT(RNG)))
        return NX_ERROR_BUSY;
    memset(state, 0, sizeof(*state));
    state->time_epoch = g_nx_stm32_system.time_epoch;
    if (!enabled) {
        state->gate_owned = true;
        gate_enable();
        if (!LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_RNG))
            return NX_ERROR_IO;
    }
    if (LL_RNG_IsEnabled(RNG) || LL_RNG_IsEnabledIT(RNG)) {
        result = nx_stm32f407_entropy_stop(state);
        return result == NX_SUCCESS ? NX_ERROR_BUSY : result;
    }
    LL_RNG_ClearFlag_CEIS(RNG);
    LL_RNG_ClearFlag_SEIS(RNG);
    state->rng_owned = true;
    LL_RNG_Enable(RNG);
    nx_arch_dsb();
    if (!LL_RNG_IsEnabled(RNG) || LL_RNG_IsEnabledIT(RNG) ||
        !clock_stable(state)) {
        state->faulted = true;
        return NX_ERROR_IO;
    }
    state->running = true;
    return NX_SUCCESS;
}

/** \brief Refuse unjoined reclamation when hardware withdrawal fails. */
nx_result_t nx_stm32f407_entropy_stop(nx_stm32f407_entropy_state_t* state) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!span(state, sizeof(*state), _Alignof(nx_stm32f407_entropy_state_t)))
        return NX_ERROR_INVALID;
    state->running = false;
    if (state->rng_owned) {
        if (!LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_RNG))
            return NX_ERROR_IO;
        LL_RNG_Disable(RNG);
        nx_arch_dsb();
        if (LL_RNG_IsEnabled(RNG))
            return NX_ERROR_IO;
        state->rng_owned = false;
    }
    if (state->gate_owned) {
        gate_disable();
        if (LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_RNG))
            return NX_ERROR_IO;
        state->gate_owned = false;
    }
    memset(state, 0, sizeof(*state));
    return NX_SUCCESS;
}

/** \brief Read one full hardware word only between successful health checks. */
static nx_result_t try_read(void* context, uint32_t* output) {
    nx_stm32f407_entropy_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!span(state, sizeof(*state), _Alignof(nx_stm32f407_entropy_state_t)) ||
        !span(output, sizeof(*output), _Alignof(uint32_t)) ||
        overlaps(output, sizeof(*output), state, sizeof(*state)) ||
        overlaps(output, sizeof(*output), RNG, sizeof(*RNG)) ||
        overlaps(output, sizeof(*output), RCC, sizeof(*RCC)))
        return NX_ERROR_INVALID;
    if (!state->running)
        return NX_ERROR_STATE;
    if (state->faulted)
        return NX_ERROR_IO;
    if (!clock_stable(state) ||
        !LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_RNG)) {
        state->faulted = true;
        return NX_ERROR_IO;
    }
    const uint32_t errors =
        RNG_SR_CECS | RNG_SR_CEIS | RNG_SR_SECS | RNG_SR_SEIS;
    uint32_t status = LL_RNG_ReadReg(RNG, SR);
    if (!LL_RNG_IsEnabled(RNG) || LL_RNG_IsEnabledIT(RNG) ||
        (status & errors) != 0U) {
        state->faulted = true;
        return NX_ERROR_IO;
    }
    if ((status & RNG_SR_DRDY) == 0U)
        return NX_ERROR_BUSY;
    uint32_t word = LL_RNG_ReadRandData32(RNG);
    if ((LL_RNG_ReadReg(RNG, SR) & errors) != 0U || !clock_stable(state)) {
        state->faulted = true;
        return NX_ERROR_IO;
    }
    if (!state->primed) {
        state->previous_word = word;
        state->primed = true;
        return NX_ERROR_BUSY;
    }
    if (word == state->previous_word) {
        state->faulted = true;
        return NX_ERROR_IO;
    }
    state->previous_word = word;
    *output = word;
    return NX_SUCCESS;
}

const nx_entropy_ops_t nx_stm32f407_entropy_ops = {try_read};
