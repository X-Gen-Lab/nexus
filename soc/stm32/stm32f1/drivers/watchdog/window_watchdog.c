/**
 * \file            window_watchdog.c
 * \brief           STM32F103xB polled window watchdog
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/watchdog/window_watchdog.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_wwdg.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "The selected window watchdog mode is reviewed for STM32F103xB"
#endif

enum {
    OWN_GATE = 1U,
    PREPARED = 2U,
    OWN_CONFIG = 4U,
    ACTIVATED = 8U,
    VERIFIED = 16U,
    NOMINAL_TICK_US = 512U,
    MAXIMUM_TICKS = 64U
};

#define CONFIG_FIELDS (WWDG_CFR_W | WWDG_CFR_WDGTB | WWDG_CFR_EWI)

static bool usable_storage(const void* ptr, size_t bytes, size_t alignment) {
    const uintptr_t begin = (uintptr_t)ptr;
    if (ptr == NULL || begin % alignment != 0U || bytes > UINTPTR_MAX - begin)
        return false;
    const uintptr_t end = begin + bytes;
    return !(begin < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(begin < UINT32_C(0xf0000000) && end > UINT32_C(0xe0000000));
}

static bool owner_valid(const nx_stm32f1_wwdg_t* port) {
    return usable_storage(port, sizeof(*port), _Alignof(nx_stm32f1_wwdg_t));
}

static bool output_valid(const nx_stm32f1_wwdg_t* port,
                         const nx_watchdog_state_t* state) {
    const uintptr_t owner = (uintptr_t)port;
    const uintptr_t output = (uintptr_t)state;
    return usable_storage(state, sizeof(*state),
                          _Alignof(nx_watchdog_state_t)) &&
           !(output < owner + sizeof(*port) && owner < output + sizeof(*state));
}

static bool vector_idle(void) {
    return NVIC_GetEnableIRQ(WWDG_IRQn) == 0U &&
           NVIC_GetPendingIRQ(WWDG_IRQn) == 0U &&
           NVIC_GetActive(WWDG_IRQn) == 0U;
}

static bool gate_enabled(void) {
    return (RCC->APB1ENR & RCC_APB1ENR_WWDGEN) != 0U;
}

static bool reset_asserted(void) {
    return (RCC->APB1RSTR & RCC_APB1RSTR_WWDGRST) != 0U;
}

static bool hsi8_apb1(void) {
    const uint32_t dividers = RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1;
    return (RCC->CR & RCC_CR_HSIRDY) != 0U &&
           (RCC->CFGR & dividers) == RCC_CFGR_SWS_HSI;
}

static bool running(const nx_stm32f1_wwdg_t* port) {
    return (port->effects & ACTIVATED) != 0U ||
           ((port->effects & OWN_GATE) != 0U && gate_enabled() &&
            !reset_asserted() && LL_WWDG_IsEnabled(WWDG) != 0U);
}

static nx_watchdog_state_t observe(const nx_stm32f1_wwdg_t* port) {
    const bool enabled = running(port);
    /* Neither prescaler phase nor external debugger settings are observed by
     * this mode. ES096 2.2.2 makes CPU-managed debug freeze unsupported. */
    return (nx_watchdog_state_t){0U, 0U, enabled, enabled, false};
}

nx_result_t nx_stm32f1_wwdg_admit(const nx_stm32f1_wwdg_t* port,
                                  uint8_t window_ticks) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!owner_valid(port) || window_ticks == 0U || window_ticks > 62U)
        return NX_ERROR_INVALID;
    if (port->effects != 0U || gate_enabled() || reset_asserted() ||
        !vector_idle())
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_wwdg_prepare(nx_stm32f1_wwdg_t* port,
                                    uint8_t window_ticks) {
    const nx_result_t admitted = nx_stm32f1_wwdg_admit(port, window_ticks);
    if (admitted != NX_SUCCESS)
        return admitted;
    if (!nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if (!hsi8_apb1())
        return NX_ERROR_UNSUPPORTED;
    port->effects = OWN_GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_WWDGEN,
                                 RCC_APB1ENR_WWDGEN))
        return NX_ERROR_IO;
    if (LL_WWDG_IsEnabled(WWDG) != 0U) {
        port->effects |= ACTIVATED;
        return NX_ERROR_BUSY;
    }
    if ((WWDG->CFR & CONFIG_FIELDS) != WWDG_CFR_W)
        return NX_ERROR_BUSY;
    port->window_ticks = window_ticks;
    port->effects |= PREPARED;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_wwdg_close(nx_stm32f1_wwdg_t* port) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!owner_valid(port))
        return NX_ERROR_INVALID;
    if (running(port)) {
        port->effects |= ACTIVATED;
        return NX_ERROR_UNSUPPORTED;
    }
    port->effects &= (uint8_t)~PREPARED;
    if ((port->effects & OWN_GATE) == 0U)
        return NX_SUCCESS;
    if (reset_asserted() || !vector_idle() ||
        (gate_enabled() && LL_WWDG_IsEnabledIT_EWKUP(WWDG) != 0U))
        return NX_ERROR_BUSY;
    if ((port->effects & OWN_CONFIG) != 0U) {
        if (!gate_enabled())
            return NX_ERROR_STATE;
        if (!nx_stm32f1_write_masked(&WWDG->CFR, CONFIG_FIELDS, WWDG_CFR_W))
            return NX_ERROR_IO;
        port->effects &= (uint8_t)~OWN_CONFIG;
    }
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_WWDGEN, 0U))
        return NX_ERROR_IO;
    *port = (nx_stm32f1_wwdg_t){0};
    return NX_SUCCESS;
}

/** \brief Select the finest nominal ceiling with at least one safe count. */
static bool timing(nx_stm32f1_wwdg_t* port, uint32_t timeout_us) {
    for (uint32_t shift = 0U; shift < 4U; ++shift) {
        const uint32_t tick_us = NOMINAL_TICK_US << shift;
        const uint32_t ticks = 1U + (timeout_us - 1U) / tick_us;
        if (ticks <= MAXIMUM_TICKS &&
            ticks >= (uint32_t)port->window_ticks + 2U) {
            port->reload = (uint8_t)(63U + ticks);
            const uint32_t window =
                (uint32_t)port->reload - port->window_ticks + 1U;
            port->config = (uint16_t)((shift << WWDG_CFR_WDGTB_Pos) | window);
            return true;
        }
    }
    return false;
}

/** \brief Keep the first CR write safe even if the disabled counter is high. */
static nx_result_t activate(nx_stm32f1_wwdg_t* port) {
    if (!gate_enabled() || reset_asserted() || !vector_idle() ||
        (WWDG->CFR & CONFIG_FIELDS) != WWDG_CFR_W)
        return NX_ERROR_STATE;
    if (LL_WWDG_IsEnabled(WWDG) != 0U) {
        port->effects |= ACTIVATED;
        return NX_ERROR_STATE;
    }
    const uint32_t wide = (port->config & WWDG_CFR_WDGTB) | WWDG_CFR_W;
    if (wide != WWDG_CFR_W) {
        port->effects |= OWN_CONFIG;
        if (!nx_stm32f1_write_masked(&WWDG->CFR, CONFIG_FIELDS, wide))
            return NX_ERROR_IO;
    }

    /* RM0008 20.3: CR reload may compare the old free-running counter to
     * the current window. Use W=0x7F for the sole activation and narrow
     * after WDGA readback, with the requested prescaler already selected. */
    port->effects |= ACTIVATED;
    NX_STM32F1_WRITE(&WWDG->CR, WWDG_CR_WDGA | port->reload);
    nx_arch_dsb();
    if (LL_WWDG_IsEnabled(WWDG) == 0U)
        return NX_ERROR_IO;
    if (port->config != wide &&
        !nx_stm32f1_write_masked(&WWDG->CFR, CONFIG_FIELDS, port->config))
        return NX_ERROR_IO;
    port->effects |= VERIFIED;
    return NX_SUCCESS;
}

static nx_result_t enable(void* context, uint32_t timeout_us, bool debug_freeze,
                          nx_watchdog_state_t* state) {
    nx_stm32f1_wwdg_t* port = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!owner_valid(port) || !output_valid(port, state) || timeout_us == 0U)
        return NX_ERROR_INVALID;
    *state = observe(port);
    if (state->enabled || (port->effects & PREPARED) == 0U ||
        !nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if (debug_freeze || !hsi8_apb1() || !timing(port, timeout_us))
        return NX_ERROR_UNSUPPORTED;
    const nx_arch_irq_state_t irq = nx_arch_irq_save();
    const nx_result_t result = activate(port);
    nx_arch_irq_restore(irq);
    *state = observe(port);
    return result;
}

/** \brief One snapshot and at most one full CR write in the admitted window. */
static nx_result_t feed(void* context) {
    const nx_stm32f1_wwdg_t* port = context;
    const uint32_t exception = nx_arch_exception_number();
    if (!nx_arch_is_privileged() || exception == 2U || exception == 3U)
        return NX_ERROR_CONTEXT;
    if (!owner_valid(port))
        return NX_ERROR_INVALID;
    const nx_arch_irq_state_t irq = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_STATE;
    if ((port->effects & VERIFIED) != 0U && hsi8_apb1() && gate_enabled() &&
        !reset_asserted() && (WWDG->CFR & CONFIG_FIELDS) == port->config) {
        const uint32_t control = WWDG->CR;
        const uint32_t counter = control & WWDG_CR_T;
        if ((control & WWDG_CR_WDGA) != 0U && counter > WWDG_CR_T_6) {
            result = NX_ERROR_BUSY;
            if (counter < (port->config & WWDG_CFR_W)) {
                NX_STM32F1_WRITE(&WWDG->CR, WWDG_CR_WDGA | port->reload);
                nx_arch_dsb();
                result = NX_SUCCESS;
            }
        }
    }
    nx_arch_irq_restore(irq);
    return result;
}

static nx_result_t state(const void* context, nx_watchdog_state_t* output) {
    const nx_stm32f1_wwdg_t* port = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!owner_valid(port) || !output_valid(port, output))
        return NX_ERROR_INVALID;
    if ((port->effects & (OWN_GATE | ACTIVATED)) == 0U)
        return NX_ERROR_STATE;
    *output = observe(port);
    return output->enabled ? NX_ERROR_UNSUPPORTED : NX_SUCCESS;
}

const nx_watchdog_ops_t nx_stm32f1_wwdg_ops = {
    .enable = enable, .feed = feed, .state = state};
