/**
 * \file            watchdog.c
 * \brief           F103xB independent watchdog and irreversible ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/watchdog/instance.h"
#include "nexus/core/time.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_iwdg.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "The selected watchdog mode is reviewed for STM32F103xB"
#endif

enum {
    PREPARED = 1U,
    LSI_OWNED = 2U,
    STARTED = 4U,
    POLL_LIMIT = 4096U,
    READY_LIMIT_US = 256U,
    SETTLE_US = 128U,
    SHADOW_LIMIT_US = 512U,
    MINIMUM_NOMINAL_US = 100U,
    MAXIMUM_NOMINAL_US = 26214400U,
    MAXIMUM_COUNT = 4096U
};

static bool prepared(const nx_stm32f1_watchdog_state_t* port) {
    return port != NULL && (port->effects & PREPARED) != 0U;
}

static bool enabled(const nx_stm32f1_watchdog_state_t* port) {
    return (port->effects & STARTED) != 0U ||
           (FLASH->OBR & FLASH_OBR_IWDG_SW) == 0U;
}

static bool overlaps(uintptr_t address, size_t size, uintptr_t other,
                     size_t other_size) {
    return address < other + other_size && other < address + size;
}

static bool output_valid(const nx_stm32f1_watchdog_state_t* port,
                         const nx_watchdog_state_t* output) {
    const uintptr_t address = (uintptr_t)output;
    const size_t size = sizeof(*output);
    return output != NULL && address % _Alignof(nx_watchdog_state_t) == 0U &&
           address <= UINTPTR_MAX - size &&
           !overlaps(address, size, (uintptr_t)port, sizeof(*port)) &&
           !overlaps(address, size, (uintptr_t)RCC, sizeof(*RCC)) &&
           !overlaps(address, size, (uintptr_t)FLASH, sizeof(*FLASH)) &&
           !overlaps(address, size, (uintptr_t)IWDG, sizeof(*IWDG));
}

static nx_watchdog_state_t observe(const nx_stm32f1_watchdog_state_t* port) {
    const bool running = enabled(port);
    /* DS5319 Table 25 characterizes LSI at 3 V. Stock board supplies do not
     * admit that profile. ES096 2.2.2 excludes CPU control of DBGMCU. */
    return (nx_watchdog_state_t){0U, 0U, running, running, false};
}

nx_result_t nx_stm32f1_watchdog_prepare(nx_stm32f1_watchdog_state_t* port) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->effects != 0U) {
        return NX_ERROR_BUSY;
    }
    port->effects = PREPARED;
    return NX_SUCCESS;
}

static bool select_lsi(bool enabled_lsi) {
    const uint32_t selected = enabled_lsi ? RCC_CSR_LSION : 0U;
    const uint32_t bit_mask = RCC_CSR_LSION | RCC_CSR_RMVF;
    NX_STM32F1_WRITE(&RCC->CSR, (RCC->CSR & ~bit_mask) | selected);
    nx_arch_dsb();
    return (RCC->CSR & RCC_CSR_LSION) == selected;
}

static bool release_lsi(nx_stm32f1_watchdog_state_t* port) {
    if ((port->effects & LSI_OWNED) != 0U) {
        if (!select_lsi(false)) {
            return false;
        }
        port->effects &= (uint8_t)~LSI_OWNED;
    }
    return true;
}

nx_result_t nx_stm32f1_watchdog_close(nx_stm32f1_watchdog_state_t* port) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!prepared(port)) {
        return NX_SUCCESS;
    }
    if (enabled(port)) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (!release_lsi(port)) {
        return NX_ERROR_IO;
    }
    port->effects = 0U;
    return NX_SUCCESS;
}

static bool wait_mask(volatile const uint32_t* register_addr, uint32_t mask,
                      uint32_t selected, uint32_t limit_us) {
    const nx_time_us_t start = nx_time_now_us();
    for (uint32_t attempt = 0U; attempt < POLL_LIMIT; ++attempt) {
        if ((*register_addr & mask) == selected) {
            return true;
        }
        if (nx_time_now_us() - start >= limit_us) {
            break;
        }
    }
    return false;
}

static bool stabilized_lsi(void) {
    const uint32_t ready = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    if (!wait_mask(&RCC->CSR, ready, ready, READY_LIMIT_US)) {
        return false;
    }
    /* ES096 2.2.12 needs 100 us after LSIRDY, beyond the nominal tick error. */
    const nx_time_us_t start = nx_time_now_us();
    for (uint32_t attempt = 0U; attempt < POLL_LIMIT; ++attempt) {
        if ((RCC->CSR & ready) != ready) {
            break;
        }
        if (nx_time_now_us() - start >= SETTLE_US) {
            return true;
        }
    }
    return false;
}

static bool reviewed_bus_clock(void) {
    const uint32_t divider_mask = RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1;
    /* HSI8/APB1 /1 stays above twice LSI for ES096 2.7.1 and 2.7.2. */
    return (RCC->CR & RCC_CR_HSIRDY) != 0U &&
           (RCC->CFGR & divider_mask) == RCC_CFGR_SWS_HSI;
}

static nx_result_t enable(void* context, uint32_t timeout_us, bool debug_freeze,
                          nx_watchdog_state_t* output) {
    nx_stm32f1_watchdog_state_t* port = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!prepared(port) || !output_valid(port, output)) {
        return NX_ERROR_INVALID;
    }
    *output = observe(port);
    if (output->enabled || !nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    if (debug_freeze || !reviewed_bus_clock()) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (timeout_us < MINIMUM_NOMINAL_US || timeout_us > MAXIMUM_NOMINAL_US) {
        return NX_ERROR_INVALID;
    }
    if ((port->effects & LSI_OWNED) != 0U || LL_IWDG_IsReady(IWDG) == 0U) {
        return NX_ERROR_BUSY;
    }
    if ((RCC->CSR & RCC_CSR_LSION) == 0U) {
        port->effects |= LSI_OWNED;
        if (!select_lsi(true)) {
            (void)release_lsi(port);
            return NX_ERROR_IO;
        }
    }
    if (!stabilized_lsi()) {
        (void)release_lsi(port);
        return NX_ERROR_IO;
    }

    uint32_t prescaler = 0U;
    uint32_t tick_us = MINIMUM_NOMINAL_US;
    while (timeout_us > tick_us * MAXIMUM_COUNT) {
        ++prescaler;
        tick_us <<= 1U;
    }
    const uint32_t reload = (timeout_us - 1U) / tick_us;

    /* KR is write-only. Publish the remaining effect before issuing CCCC. */
    port->effects |= STARTED;
    *output = observe(port);
    LL_IWDG_Enable(IWDG);
    LL_IWDG_EnableWriteAccess(IWDG);
    LL_IWDG_SetPrescaler(IWDG, prescaler);
    LL_IWDG_SetReloadCounter(IWDG, reload);
    nx_arch_dsb();
    /* ES096 2.7.3: settle both shadow writes in Run before returning. */
    if (!wait_mask(&IWDG->SR, IWDG_SR_PVU | IWDG_SR_RVU, 0U, SHADOW_LIMIT_US) ||
        LL_IWDG_GetPrescaler(IWDG) != prescaler ||
        LL_IWDG_GetReloadCounter(IWDG) != reload) {
        LL_IWDG_DisableWriteAccess(IWDG);
        nx_arch_dsb();
        return NX_ERROR_IO;
    }
    LL_IWDG_ReloadCounter(IWDG);
    nx_arch_dsb();
    return NX_SUCCESS;
}

static nx_result_t feed(void* context) {
    const nx_stm32f1_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (!prepared(port) || !enabled(port)) {
        return NX_ERROR_STATE;
    }
    LL_IWDG_ReloadCounter(IWDG);
    return NX_SUCCESS;
}

static nx_result_t state(const void* context, nx_watchdog_state_t* output) {
    const nx_stm32f1_watchdog_state_t* port = context;
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    if (!prepared(port) || !output_valid(port, output)) {
        return NX_ERROR_INVALID;
    }
    *output = observe(port);
    return output->enabled ? NX_ERROR_UNSUPPORTED : NX_SUCCESS;
}

const nx_watchdog_ops_t nx_stm32f1_watchdog_ops = {
    .enable = enable,
    .feed = feed,
    .state = state,
};
