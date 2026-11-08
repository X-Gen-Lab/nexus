/**
 * \file            crc.c
 * \brief           Fixed-polynomial CRC using official LL and explicit clocks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_crc.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_crc.h"

/** \brief Calculator use and gate controls share one serialized task owner. */
static bool context_ok(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Configuration grants this unique calculator; a clock is no owner ID.
 */
nx_result_t nx_stm32_crc_prepare(nx_stm32_crc_state_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->initialized || state->clock_added) {
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    state->clock_added =
        LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_CRC) == 0U;
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC);
    nx_arch_dsb();
    nx_arch_irq_restore(saved);
    if (LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_CRC) == 0U) {
        return NX_ERROR_IO;
    }
    state->initialized = true;
    return NX_SUCCESS;
}
/** \brief No asynchronous sources exist; only the added gate is withdrawn. */
nx_result_t nx_stm32_crc_close(nx_stm32_crc_state_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    state->initialized = false;
    if (state->clock_added) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_CRC);
        nx_arch_dsb();
        nx_arch_irq_restore(saved);
        if (LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_CRC) != 0U) {
            return NX_ERROR_BUSY;
        }
        state->clock_added = false;
    }
    return NX_SUCCESS;
}
/** \brief Check live clock and deadline at bounded sixteen-word intervals. */
static nx_result_t progress(nx_time_us_t deadline) {
    if (LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_CRC) == 0U) {
        return NX_ERROR_IO;
    }
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
/** \brief All extents have passed the public entry's nonwrapping checks. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return an != 0U && (uintptr_t)a < (uintptr_t)b + bn &&
           (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief The calculator cannot reset or publish into its own live controls. */
static bool independent(const nx_stm32_crc_state_t* state, const void* data,
                        size_t size) {
    return !overlap(data, size, state, sizeof(*state)) &&
           !overlap(data, size, CRC, sizeof(*CRC)) &&
           !overlap(data, size, (const void*)&RCC->AHB1ENR,
                    sizeof(RCC->AHB1ENR));
}
/** \brief Four HCLK cycles per vendor word write; no byte reinterpretation. */
static nx_result_t compute(void* context, const uint32_t* words, size_t count,
                           nx_time_us_t deadline, uint32_t* output) {
    nx_stm32_crc_state_t* state = context;
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized) {
        return NX_ERROR_STATE;
    }
    if (!independent(state, output, sizeof(*output)) ||
        !independent(state, words, count * sizeof(*words))) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = progress(deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    LL_CRC_ResetCRCCalculationUnit(CRC);
    nx_arch_dsb();
    bool reset = false;
    for (unsigned attempt = 0U; attempt < 8U; ++attempt) {
        if ((LL_CRC_ReadReg(CRC, CR) & CRC_CR_RESET) == 0U &&
            LL_CRC_ReadData32(CRC) == UINT32_MAX) {
            reset = true;
            break;
        }
        result = progress(deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    if (!reset) {
        return NX_ERROR_IO;
    }
    for (size_t offset = 0U; offset < count;) {
        result = progress(deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        size_t block = count - offset < 16U ? count - offset : 16U;
        for (size_t index = 0U; index < block; ++index) {
            LL_CRC_FeedData32(CRC, words[offset++]);
        }
    }
    uint32_t value = LL_CRC_ReadData32(CRC);
    result = progress(deadline);
    if (result == NX_SUCCESS) {
        *output = value;
    }
    return result;
}
const nx_crc_ops_t nx_stm32_crc_ops = {.compute = compute};
