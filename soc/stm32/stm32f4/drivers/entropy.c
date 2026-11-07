/**
 * \file            entropy.c
 * \brief           Official-LL F407 RNG ownership and continuous health checks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "stm32f407_entropy.h"
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

/** \brief Validate the existing oscillator, exact PLL48 and authored HCLK. */
static nx_result_t clock_profile(const nx_stm32f407_entropy_config_t* config) {
    if (!LL_RCC_PLL_IsReady())
        return NX_ERROR_STATE;
    bool hse = LL_RCC_PLL_GetMainSource() == LL_RCC_PLLSOURCE_HSE;
    if ((hse && !LL_RCC_HSE_IsReady()) || (!hse && !LL_RCC_HSI_IsReady()))
        return NX_ERROR_STATE;
    if (config->hse_hz != 8000000U || config->hclk_hz > 168000000U)
        return NX_ERROR_UNSUPPORTED;
    uint32_t input = hse ? config->hse_hz : 16000000U;
    uint32_t m = LL_RCC_PLL_GetDivider();
    uint32_t n = LL_RCC_PLL_GetN();
    uint32_t q = LL_RCC_PLL_GetQ() >> RCC_PLLCFGR_PLLQ_Pos;
    uint32_t p = ((LL_RCC_PLL_GetP() >> RCC_PLLCFGR_PLLP_Pos) + 1U) * 2U;
    if (m < 2U || m > 63U || n < 50U || n > 432U || q < 4U || q > 9U ||
        (uint64_t)input < (uint64_t)m * 1000000U ||
        (uint64_t)input > (uint64_t)m * 2000000U ||
        (uint64_t)input * n != (uint64_t)48000000U * m * q)
        return NX_ERROR_UNSUPPORTED;
    uint32_t hpre = LL_RCC_GetAHBPrescaler() >> RCC_CFGR_HPRE_Pos;
    uint32_t shift = hpre < 8U ? 0U : hpre < 12U ? hpre - 7U : hpre - 6U;
    uint64_t expected_sysclk = (uint64_t)config->hclk_hz << shift;
    uint32_t source = LL_RCC_GetSysClkSource();
    if (source == LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
        return expected_sysclk * m * p == (uint64_t)input * n
                   ? NX_SUCCESS
                   : NX_ERROR_UNSUPPORTED;
    if (source == LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
        return LL_RCC_HSI_IsReady() && expected_sysclk == 16000000U
                   ? NX_SUCCESS
                   : NX_ERROR_UNSUPPORTED;
    if (source == LL_RCC_SYS_CLKSOURCE_STATUS_HSE)
        return LL_RCC_HSE_IsReady() && expected_sysclk == config->hse_hz
                   ? NX_SUCCESS
                   : NX_ERROR_UNSUPPORTED;
    return NX_ERROR_UNSUPPORTED;
}

/** \brief Detect loss/change without modifying the shared clock profile. */
static bool clock_stable(const nx_stm32f407_entropy_state_t* state) {
    return LL_RCC_PLL_IsReady() &&
           LL_RCC_ReadReg(PLLCFGR) == state->pll_snapshot &&
           (LL_RCC_ReadReg(CFGR) & (uint32_t)(RCC_CFGR_SWS | RCC_CFGR_HPRE)) ==
               state->clock_snapshot &&
           ((state->pll_snapshot & RCC_PLLCFGR_PLLSRC) != 0U
                ? LL_RCC_HSE_IsReady()
                : LL_RCC_HSI_IsReady());
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

/** \brief Admit the exact profile before acquiring the RNG controller. */
nx_result_t
nx_stm32f407_entropy_start(nx_stm32f407_entropy_state_t* state,
                           const nx_stm32f407_entropy_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!span(state, sizeof(*state), _Alignof(nx_stm32f407_entropy_state_t)) ||
        !span(config, sizeof(*config),
              _Alignof(nx_stm32f407_entropy_config_t)) ||
        overlaps(state, sizeof(*state), config, sizeof(*config)) ||
        config->hclk_hz == 0U)
        return NX_ERROR_INVALID;
    if (state->running || state->gate_owned || state->rng_owned)
        return NX_ERROR_BUSY;
    nx_result_t result = clock_profile(config);
    if (result != NX_SUCCESS)
        return result;
    bool enabled = LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_RNG) != 0U;
    if (enabled && (LL_RNG_IsEnabled(RNG) || LL_RNG_IsEnabledIT(RNG)))
        return NX_ERROR_BUSY;
    memset(state, 0, sizeof(*state));
    state->pll_snapshot = LL_RCC_ReadReg(PLLCFGR);
    state->clock_snapshot =
        LL_RCC_ReadReg(CFGR) & (uint32_t)(RCC_CFGR_SWS | RCC_CFGR_HPRE);
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
