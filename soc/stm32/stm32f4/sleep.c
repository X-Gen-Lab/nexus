/**
 * \file            sleep.c
 * \brief           Guarded closed-domain sleep without clock reconfiguration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/sleep.h"
#include "clock_domain_impl.h"
#include "nexus/arch/arch.h"
#include "stm32f407_clock.h"
#include "stm32f407_sleep.h"

/** \brief Actual source, PLL, bus mux/divisors and established Flash latency.
 */
static bool fixed_run(void) {
    return nx_stm32_clock_profile_matches(RCC, FLASH) &&
           (RCC->APB1LPENR & RCC_APB1LPENR_TIM2LPEN) != 0U;
}

nx_result_t nx_stm32f407_sleep(nx_clock_barrier_t barrier,
                               nx_clock_power_mode_t mode,
                               const uint32_t* wake_sequence, uint32_t expected,
                               bool* slept) {
    if (wake_sequence == NULL || slept == NULL ||
        (uintptr_t)wake_sequence % _Alignof(uint32_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask != 0U || masks.basepri != 0U || masks.faultmask != 0U) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_INVALID;
    if (nx_clock_domain_span_valid(barrier.domain, wake_sequence,
                                   sizeof(*wake_sequence)) &&
        nx_clock_domain_span_valid(barrier.domain, slept, sizeof(*slept)) &&
        !((uintptr_t)slept >= (uintptr_t)wake_sequence &&
          (uintptr_t)slept <
              (uintptr_t)wake_sequence + sizeof(*wake_sequence))) {
        *slept = false;
        if (mode == NX_CLOCK_DEEP_STOP || mode == NX_CLOCK_DYNAMIC_RUN) {
            result = NX_ERROR_UNSUPPORTED;
        } else if (mode == NX_CLOCK_SHALLOW_SLEEP) {
            result = nx_clock_domain_quiescent(barrier);
            if (result == NX_SUCCESS) {
                if (!fixed_run()) {
                    result = NX_ERROR_UNSUPPORTED;
                } else if (__atomic_load_n(wake_sequence, __ATOMIC_ACQUIRE) ==
                           expected) {
                    /* Retained identity, physical profile and wake publication
                     * are observed under the same mask as production WFI.
                     * No Tick/clock domain is stopped or rebuilt here. */
                    nx_arch_result_t wait = nx_arch_wait_for_interrupt();
                    result = wait == NX_ARCH_OK        ? NX_SUCCESS
                             : wait == NX_ARCH_CONTEXT ? NX_ERROR_CONTEXT
                             : wait == NX_ARCH_UNSUPPORTED
                                 ? NX_ERROR_UNSUPPORTED
                                 : NX_ERROR_INVALID;
                    *slept = result == NX_SUCCESS;
                }
            }
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}
