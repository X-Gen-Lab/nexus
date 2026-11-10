/**
 * \file            window_watchdog.c
 * \brief           WWDG explicit windows and retained activation responsibility
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-11-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/soc/stm32f407_wwdg_ewi.h"
#include "stm32f407_clock.h"
#include "stm32f407_window_watchdog.h"
#include "stm32f4xx_ll_wwdg.h"

#ifndef NX_STM32_WWDG_WRITE
#define NX_STM32_WWDG_WRITE(address, value) (*(address) = (uint32_t)(value))
#endif

#define PREPARED      1U
#define OWN_CLOCK     2U
#define OWN_SLEEP     4U
#define SAVED_SLEEP   8U
#define OWN_DEBUG     16U
#define SAVED_DEBUG   32U
#define OWN_CONFIG    64U
#define ACTIVATED     128U
#define CONFIG_FIELDS (WWDG_CFR_W | WWDG_CFR_WDGTB | WWDG_CFR_EWI)
#define OWN_EWI       UINT16_C(0x8000)

_Static_assert(WWDG_IRQn == 0, "Exact F407 WWDG publisher identity");

/** \brief Private owner and output storage cannot alias peripheral windows. */
static bool storage(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)pointer;
    if (pointer == NULL || begin % alignment != 0U ||
        bytes > UINTPTR_MAX - begin)
        return false;
    uintptr_t end = begin + bytes;
    return !(begin < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(begin < UINT32_C(0xF0000000) && end > UINT32_C(0xE0000000));
}

/** \brief Cold changes require the serialized privileged Thread owner. */
static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Fixed nominal PCLK1 is derived from the actual maintained RUN tuple.
 */
static bool clock_valid(void) {
    return nx_stm32_clock_profile_matches(RCC, NULL);
}

/** \brief No EWI publisher is adopted, enabled or acknowledged by this mode. */
static bool vector_idle(void) {
    return NVIC_GetEnableIRQ(WWDG_IRQn) == 0U &&
           NVIC_GetPendingIRQ(WWDG_IRQn) == 0U &&
           NVIC_GetActive(WWDG_IRQn) == 0U;
}

/** \brief Failed activation remains conservatively irreversible. */
static nx_watchdog_state_t
observed(const nx_stm32_window_watchdog_state_t* port) {
    bool enabled =
        (port->effects & ACTIVATED) != 0U ||
        ((port->effects & OWN_CLOCK) != 0U && LL_WWDG_IsEnabled(WWDG));
    return (nx_watchdog_state_t){
        .minimum_timeout_us = 0U,
        .maximum_timeout_us = 0U,
        .enabled = enabled,
        .irreversible = enabled,
        .debug_freeze = (DBGMCU->APB1FZ & DBGMCU_APB1_FZ_DBG_WWDG_STOP) != 0U};
}

/** \brief A result object cannot overwrite the retained cleanup receipt. */
static bool output_valid(const nx_stm32_window_watchdog_state_t* port,
                         const nx_watchdog_state_t* state) {
    uintptr_t output = (uintptr_t)state;
    uintptr_t owner = (uintptr_t)port;
    return storage(state, sizeof(*state), _Alignof(nx_watchdog_state_t)) &&
           !(output < owner + sizeof(*port) && owner < output + sizeof(*state));
}

nx_result_t
nx_stm32_window_watchdog_admit(const nx_stm32_window_watchdog_state_t* port,
                               uint8_t window_ticks) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port),
                 _Alignof(nx_stm32_window_watchdog_state_t)) ||
        window_ticks == 0U || window_ticks > 62U)
        return NX_ERROR_INVALID;
    if (port->effects != 0U || port->window_ticks != 0U || port->reload != 0U ||
        port->divider != 0U || port->saved_cfr != 0U)
        return NX_ERROR_BUSY;
    if ((RCC->APB1ENR & RCC_APB1ENR_WWDGEN) != 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_WWDGRST) != 0U || !vector_idle())
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_window_watchdog_prepare(nx_stm32_window_watchdog_state_t* port,
                                 uint8_t window_ticks) {
    nx_result_t result = nx_stm32_window_watchdog_admit(port, window_ticks);
    if (result != NX_SUCCESS)
        return result;
    /* The shared Core plan admits every controller before system_start.
     * Actual RUN-clock proof belongs here, before our first hardware write. */
    if (!clock_valid())
        return NX_ERROR_UNSUPPORTED;
    port->window_ticks = window_ticks;
    port->effects = OWN_CLOCK;
    if ((RCC->APB1LPENR & RCC_APB1LPENR_WWDGLPEN) != 0U)
        port->effects |= SAVED_SLEEP;
    NX_STM32_WWDG_WRITE(&RCC->APB1ENR, RCC->APB1ENR | RCC_APB1ENR_WWDGEN);
    nx_arch_dsb();
    if ((RCC->APB1ENR & RCC_APB1ENR_WWDGEN) == 0U)
        return NX_ERROR_IO;
    if (LL_WWDG_IsEnabled(WWDG)) {
        port->effects |= ACTIVATED;
        return NX_ERROR_BUSY;
    }
    port->saved_cfr = (uint16_t)(WWDG->CFR & CONFIG_FIELDS);
    if (LL_WWDG_IsEnabledIT_EWKUP(WWDG))
        return NX_ERROR_BUSY;
    port->effects |= OWN_SLEEP;
    NX_STM32_WWDG_WRITE(&RCC->APB1LPENR,
                        RCC->APB1LPENR | RCC_APB1LPENR_WWDGLPEN);
    nx_arch_dsb();
    if ((RCC->APB1LPENR & RCC_APB1LPENR_WWDGLPEN) == 0U)
        return NX_ERROR_IO;
    port->effects |= PREPARED;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32_window_watchdog_stop(nx_stm32_window_watchdog_state_t* port) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port),
                 _Alignof(nx_stm32_window_watchdog_state_t)))
        return NX_ERROR_INVALID;
    if ((port->effects & ACTIVATED) != 0U ||
        ((port->effects & OWN_CLOCK) != 0U && LL_WWDG_IsEnabled(WWDG))) {
        port->effects |= ACTIVATED;
        return NX_ERROR_UNSUPPORTED;
    }
    /* Closing revokes activation admission even when hardware restoration
     * fails. Only successful close followed by prepare may admit it again. */
    port->effects &= (uint8_t)~PREPARED;
    if ((port->effects & OWN_CLOCK) != 0U) {
        if (!vector_idle() || LL_WWDG_IsEnabledIT_EWKUP(WWDG) ||
            (RCC->APB1RSTR & RCC_APB1RSTR_WWDGRST) != 0U)
            return NX_ERROR_BUSY;
        if ((port->effects & OWN_CONFIG) != 0U) {
            NX_STM32_WWDG_WRITE(&WWDG->CFR,
                                (WWDG->CFR & ~(uint32_t)CONFIG_FIELDS) |
                                    port->saved_cfr);
            nx_arch_dsb();
            if ((WWDG->CFR & CONFIG_FIELDS) != port->saved_cfr)
                return NX_ERROR_IO;
            port->effects &= (uint8_t)~OWN_CONFIG;
        }
    }
    if ((port->effects & OWN_DEBUG) != 0U) {
        uint32_t saved = (port->effects & SAVED_DEBUG) != 0U
                             ? DBGMCU_APB1_FZ_DBG_WWDG_STOP
                             : 0U;
        NX_STM32_WWDG_WRITE(
            &DBGMCU->APB1FZ,
            (DBGMCU->APB1FZ & ~(uint32_t)DBGMCU_APB1_FZ_DBG_WWDG_STOP) | saved);
        nx_arch_dsb();
        if ((DBGMCU->APB1FZ & DBGMCU_APB1_FZ_DBG_WWDG_STOP) != saved)
            return NX_ERROR_IO;
        port->effects &= (uint8_t)~OWN_DEBUG;
    }
    if ((port->effects & OWN_SLEEP) != 0U) {
        uint32_t saved =
            (port->effects & SAVED_SLEEP) != 0U ? RCC_APB1LPENR_WWDGLPEN : 0U;
        NX_STM32_WWDG_WRITE(
            &RCC->APB1LPENR,
            (RCC->APB1LPENR & ~(uint32_t)RCC_APB1LPENR_WWDGLPEN) | saved);
        nx_arch_dsb();
        if ((RCC->APB1LPENR & RCC_APB1LPENR_WWDGLPEN) != saved)
            return NX_ERROR_IO;
        port->effects &= (uint8_t)~OWN_SLEEP;
    }
    if ((port->effects & OWN_CLOCK) != 0U) {
        NX_STM32_WWDG_WRITE(&RCC->APB1ENR,
                            RCC->APB1ENR & ~(uint32_t)RCC_APB1ENR_WWDGEN);
        nx_arch_dsb();
        if ((RCC->APB1ENR & RCC_APB1ENR_WWDGEN) != 0U)
            return NX_ERROR_IO;
    }
    *port = (nx_stm32_window_watchdog_state_t){0};
    return NX_SUCCESS;
}

/** \brief Choose the finest nominal ceiling with at least one safe feed count.
 */
static bool timing(uint32_t timeout_us, uint8_t window_ticks, uint8_t* reload,
                   uint8_t* divider) {
    if (timeout_us == 0U)
        return false;
    uint64_t clocks = (uint64_t)timeout_us * NEXUS_APB1_HZ;
    for (uint8_t shift = 0U; shift < 4U; ++shift) {
        uint64_t cycles = (UINT64_C(4096) << shift) * 1000000U;
        uint64_t steps = clocks / cycles + (clocks % cycles != 0U ? 1U : 0U);
        if (steps <= 64U && steps >= (uint32_t)window_ticks + 2U) {
            *reload = (uint8_t)(63U + steps);
            *divider = shift;
            return true;
        }
    }
    return false;
}

/** \brief Configure and prove all reversible fields before the sole WDGA write.
 */
static nx_result_t enable(void* context, uint32_t timeout_us, bool debug_freeze,
                          nx_watchdog_state_t* state) {
    nx_stm32_window_watchdog_state_t* port = context;
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port),
                 _Alignof(nx_stm32_window_watchdog_state_t)) ||
        !output_valid(port, state) || timeout_us == 0U)
        return NX_ERROR_INVALID;
    *state = observed(port);
    if (state->enabled || (port->effects & PREPARED) == 0U)
        return NX_ERROR_STATE;
    if (!clock_valid() || (RCC->APB1ENR & RCC_APB1ENR_WWDGEN) == 0U ||
        (RCC->APB1LPENR & RCC_APB1LPENR_WWDGLPEN) == 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_WWDGRST) != 0U || !vector_idle() ||
        LL_WWDG_IsEnabledIT_EWKUP(WWDG))
        return NX_ERROR_STATE;
    uint8_t reload, divider;
    if (!timing(timeout_us, port->window_ticks, &reload, &divider))
        return NX_ERROR_UNSUPPORTED;
    uint32_t window = (uint32_t)reload - port->window_ticks + 1U;
    uint32_t config = ((uint32_t)divider << 7U) | window;
    port->effects |= OWN_CONFIG;
    NX_STM32_WWDG_WRITE(&WWDG->CFR,
                        (WWDG->CFR & ~(uint32_t)CONFIG_FIELDS) | config);
    nx_arch_dsb();
    if ((WWDG->CFR & CONFIG_FIELDS) != config)
        return NX_ERROR_IO;
    if ((port->effects & OWN_DEBUG) == 0U) {
        if ((DBGMCU->APB1FZ & DBGMCU_APB1_FZ_DBG_WWDG_STOP) != 0U)
            port->effects |= SAVED_DEBUG;
        port->effects |= OWN_DEBUG;
    }
    uint32_t debug = debug_freeze ? DBGMCU_APB1_FZ_DBG_WWDG_STOP : 0U;
    NX_STM32_WWDG_WRITE(
        &DBGMCU->APB1FZ,
        (DBGMCU->APB1FZ & ~(uint32_t)DBGMCU_APB1_FZ_DBG_WWDG_STOP) | debug);
    nx_arch_dsb();
    *state = observed(port);
    if ((DBGMCU->APB1FZ & DBGMCU_APB1_FZ_DBG_WWDG_STOP) != debug)
        return NX_ERROR_IO;
    port->reload = reload;
    port->divider = divider;
    /* HAL activates CR before configuring CFR; LL_Enable RMW preserves the
     * free-running old counter. This single write installs WDGA and T6 with
     * the reviewed reload only after all reversible settings are verified. */
    port->effects |= ACTIVATED;
    NX_STM32_WWDG_WRITE(&WWDG->CR, WWDG_CR_WDGA | reload);
    nx_arch_dsb();
    *state = observed(port);
    return LL_WWDG_IsEnabled(WWDG) ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief One protected observation/write; early and late calls never reload.
 */
static nx_result_t feed(void* context) {
    nx_stm32_window_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port),
                 _Alignof(nx_stm32_window_watchdog_state_t)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if ((port->effects & ACTIVATED) != 0U &&
        (RCC->APB1ENR & RCC_APB1ENR_WWDGEN) != 0U && LL_WWDG_IsEnabled(WWDG)) {
        uint32_t config = WWDG->CFR & CONFIG_FIELDS;
        uint32_t window = (uint32_t)port->reload - port->window_ticks + 1U;
        uint32_t expected =
            ((uint32_t)port->divider << 7U) | window |
            ((port->saved_cfr & OWN_EWI) != 0U ? WWDG_CFR_EWI : 0U);
        if (config == expected) {
            uint32_t counter = LL_WWDG_GetCounter(WWDG);
            if (counter > 0x40U) {
                result = NX_ERROR_BUSY;
                if (counter < window) {
                    NX_STM32_WWDG_WRITE(&WWDG->CR, WWDG_CR_WDGA | port->reload);
                    nx_arch_dsb();
                    result = NX_SUCCESS;
                }
            }
        }
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Unknown physical bounds do not erase an observed activation effect.
 */
static nx_result_t query(const void* context, nx_watchdog_state_t* state) {
    const nx_stm32_window_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port),
                 _Alignof(nx_stm32_window_watchdog_state_t)) ||
        !output_valid(port, state))
        return NX_ERROR_INVALID;
    if ((port->effects & (OWN_CLOCK | ACTIVATED)) == 0U)
        return NX_ERROR_STATE;
    *state = observed(port);
    return state->enabled ? NX_ERROR_UNSUPPORTED : NX_SUCCESS;
}

const nx_watchdog_ops_t nx_stm32_window_watchdog_ops = {
    .enable = enable, .feed = feed, .state = query};

/** \brief A face identifies its real provider without a hardware registry. */
static nx_stm32_window_watchdog_state_t*
warning_owner(const nx_watchdog_port_t* face) {
    if (!storage(face, sizeof(*face), _Alignof(nx_watchdog_port_t)) ||
        face->ops != &nx_stm32_window_watchdog_ops ||
        !storage(face->context, sizeof(nx_stm32_window_watchdog_state_t),
                 _Alignof(nx_stm32_window_watchdog_state_t)))
        return NULL;
    return face->context;
}

/** \brief Check optional state against the complete retained provider/face. */
static bool warning_storage(const nx_stm32f407_wwdg_ewi_t* warning,
                            const nx_watchdog_port_t* face,
                            const nx_stm32_window_watchdog_state_t* port) {
    if (!storage(warning, sizeof(*warning), _Alignof(nx_stm32f407_wwdg_ewi_t)))
        return false;
    uintptr_t begin = (uintptr_t)warning;
    uintptr_t end = begin + sizeof(*warning);
    return !(begin < (uintptr_t)face + sizeof(*face) &&
             (uintptr_t)face < end) &&
           !(begin < (uintptr_t)port + sizeof(*port) && (uintptr_t)port < end);
}

nx_result_t nx_stm32f407_wwdg_ewi_arm(nx_stm32f407_wwdg_ewi_t* warning,
                                      const nx_watchdog_port_t* watchdog,
                                      uint8_t priority) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    nx_stm32_window_watchdog_state_t* port = warning_owner(watchdog);
    if (port == NULL || !warning_storage(warning, watchdog, port) ||
        priority > 15U)
        return NX_ERROR_INVALID;
    if (warning->retained || warning->watchdog != NULL ||
        warning->occurrences != 0U || warning->overflow)
        return NX_ERROR_STATE;
    if ((port->effects & (PREPARED | ACTIVATED)) != (PREPARED | ACTIVATED) ||
        !LL_WWDG_IsEnabled(WWDG) || !clock_valid())
        return NX_ERROR_STATE;
    if (!vector_idle() || (port->saved_cfr & OWN_EWI) != 0U ||
        LL_WWDG_IsEnabledIT_EWKUP(WWDG) || (WWDG->SR & WWDG_SR_EWIF) != 0U)
        return NX_ERROR_BUSY;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    /* The source is sticky. Publish its static owner before attempting it;
     * failed readback cannot revoke this lifetime or the watchdog gate. */
    warning->watchdog = watchdog;
    warning->retained = true;
    port->saved_cfr |= OWN_EWI;
    NX_STM32_WWDG_WRITE(&WWDG->CFR, WWDG->CFR | WWDG_CFR_EWI);
    nx_arch_dsb();
    nx_result_t result = NX_ERROR_IO;
    if (LL_WWDG_IsEnabledIT_EWKUP(WWDG)) {
        NVIC_SetPriority(WWDG_IRQn, priority);
        NVIC_EnableIRQ(WWDG_IRQn);
        nx_arch_dsb();
        if (NVIC_GetPriority(WWDG_IRQn) == priority &&
            NVIC_GetEnableIRQ(WWDG_IRQn) != 0U)
            result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_stm32f407_wwdg_ewi_irq(nx_stm32f407_wwdg_ewi_t* warning) {
    if (!nx_arch_is_privileged() || !nx_arch_in_isr())
        return NX_ERROR_CONTEXT;
    if (!storage(warning, sizeof(*warning), _Alignof(nx_stm32f407_wwdg_ewi_t)))
        return NX_ERROR_INVALID;
    nx_stm32_window_watchdog_state_t* port = warning_owner(warning->watchdog);
    if (!warning->retained || port == NULL ||
        (port->saved_cfr & OWN_EWI) == 0U || !LL_WWDG_IsEnabledIT_EWKUP(WWDG))
        return NX_ERROR_STATE;
    if ((WWDG->SR & WWDG_SR_EWIF) == 0U)
        return NX_ERROR_STATE;
    NX_STM32_WWDG_WRITE(&WWDG->SR, 0U);
    nx_arch_dsb();
    if ((WWDG->SR & WWDG_SR_EWIF) != 0U)
        return NX_ERROR_IO;
    if (warning->occurrences == UINT32_MAX)
        warning->overflow = true;
    else
        ++warning->occurrences;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f407_wwdg_ewi_read(const nx_stm32f407_wwdg_ewi_t* warning,
                                       uint32_t* occurrences, bool* overflow) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!storage(warning, sizeof(*warning),
                 _Alignof(nx_stm32f407_wwdg_ewi_t)) ||
        !storage(occurrences, sizeof(*occurrences), _Alignof(uint32_t)) ||
        !storage(overflow, sizeof(*overflow), _Alignof(bool)))
        return NX_ERROR_INVALID;
    uintptr_t count = (uintptr_t)occurrences;
    uintptr_t lost = (uintptr_t)overflow;
    uintptr_t begin = (uintptr_t)warning;
    uintptr_t end = begin + sizeof(*warning);
    if ((count < end && begin < count + sizeof(*occurrences)) ||
        (lost < end && begin < lost + sizeof(*overflow)) ||
        (lost < count + sizeof(*occurrences) &&
         count < lost + sizeof(*overflow)))
        return NX_ERROR_INVALID;
    if (!warning->retained)
        return NX_ERROR_STATE;
    nx_stm32_window_watchdog_state_t* port = warning_owner(warning->watchdog);
    if (port == NULL)
        return NX_ERROR_STATE;
    uintptr_t provider = (uintptr_t)port;
    uintptr_t face = (uintptr_t)warning->watchdog;
    if ((count < provider + sizeof(*port) &&
         provider < count + sizeof(*occurrences)) ||
        (lost < provider + sizeof(*port) &&
         provider < lost + sizeof(*overflow)) ||
        (count < face + sizeof(*warning->watchdog) &&
         face < count + sizeof(*occurrences)) ||
        (lost < face + sizeof(*warning->watchdog) &&
         face < lost + sizeof(*overflow)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    *occurrences = warning->occurrences;
    *overflow = warning->overflow;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
