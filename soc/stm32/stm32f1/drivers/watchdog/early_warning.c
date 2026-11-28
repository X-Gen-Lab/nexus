/**
 * \file            early_warning.c
 * \brief           Record a F103RB watchdog early warning in reserved BKP DR1.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/watchdog/early_warning.h"
#include "drivers/watchdog/window_watchdog.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_wwdg.h"

#if !defined(STM32F103xB)
#error "The reviewed early warning fixture requires STM32F103xB"
#endif

_Static_assert(WWDG_IRQn == 0, "Exact STM32F103xB WWDG vector");

enum { GATES = 1U, WRITE_ACCESS = 2U, READY = 4U, STICKY = 8U };
#define BACKUP_GATES (RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN)
#define WWDG_CONFIG  (WWDG_CFR_W | WWDG_CFR_WDGTB | WWDG_CFR_EWI)
#define EWI_MARKER   UINT32_C(0x57D6)

static bool backup_clocked(void) {
    return (RCC->APB1ENR & BACKUP_GATES) == BACKUP_GATES;
}

static bool backup_ready(const nx_stm32f1_wwdg_ewi_t* warning) {
    return (warning->effects & READY) != 0U && backup_clocked() &&
           (PWR->CR & PWR_CR_DBP) != 0U && (RCC->BDCR & RCC_BDCR_BDRST) == 0U &&
           (BKP->CR & BKP_CR_TPE) == 0U;
}

/** \brief Change DBP with no write pulse to the two PWR status clears. */
static bool write_access(bool enable) {
    const uint32_t fields = PWR_CR_DBP | PWR_CR_CWUF | PWR_CR_CSBF;
    const uint32_t selected = enable ? PWR_CR_DBP : 0U;
    NX_STM32F1_WRITE(&PWR->CR, (PWR->CR & ~fields) | selected);
    nx_arch_dsb();
    return (PWR->CR & PWR_CR_DBP) == selected;
}

static uint32_t retained_word(void) {
    return BKP->DR1 & BKP_DR1_D;
}

static bool write_word(uint32_t word) {
    NX_STM32F1_WRITE(&BKP->DR1, word);
    nx_arch_dsb();
    return retained_word() == word;
}

nx_result_t nx_stm32f1_wwdg_ewi_prepare(nx_stm32f1_wwdg_ewi_t* warning) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (warning == NULL)
        return NX_ERROR_INVALID;
    if (warning->effects != 0U || (RCC->APB1ENR & BACKUP_GATES) != 0U ||
        (RCC->APB1RSTR & (RCC_APB1RSTR_PWRRST | RCC_APB1RSTR_BKPRST)) != 0U ||
        (RCC->BDCR & RCC_BDCR_BDRST) != 0U)
        return NX_ERROR_BUSY;
    warning->effects |= GATES;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, BACKUP_GATES, BACKUP_GATES))
        return NX_ERROR_IO;
    if ((PWR->CR & PWR_CR_DBP) != 0U || (BKP->CR & BKP_CR_TPE) != 0U)
        return NX_ERROR_BUSY;
    warning->effects |= WRITE_ACCESS;
    if (!write_access(true))
        return NX_ERROR_IO;
    warning->effects |= READY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_wwdg_ewi_take(const nx_stm32f1_wwdg_ewi_t* warning,
                                     bool* observed) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (warning == NULL || observed == NULL || (const void*)warning == observed)
        return NX_ERROR_INVALID;
    if ((warning->effects & STICKY) != 0U || !backup_ready(warning))
        return NX_ERROR_STATE;
    const uint32_t word = retained_word();
    if (word != 0U && word != EWI_MARKER)
        return NX_ERROR_BUSY;
    if (word == EWI_MARKER && !write_word(0U))
        return NX_ERROR_IO;
    *observed = word == EWI_MARKER;
    return NX_SUCCESS;
}

static bool watchdog_clocked(void) {
    const uint32_t clock = RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1;
    return (RCC->APB1ENR & RCC_APB1ENR_WWDGEN) != 0U &&
           (RCC->APB1RSTR & RCC_APB1RSTR_WWDGRST) == 0U &&
           (RCC->CR & RCC_CR_HSIRDY) != 0U &&
           (RCC->CFGR & clock) == RCC_CFGR_SWS_HSI;
}

static bool vector_idle(void) {
    return NVIC_GetEnableIRQ(WWDG_IRQn) == 0U &&
           NVIC_GetPendingIRQ(WWDG_IRQn) == 0U &&
           NVIC_GetActive(WWDG_IRQn) == 0U;
}

nx_result_t nx_stm32f1_wwdg_ewi_arm(nx_stm32f1_wwdg_ewi_t* warning,
                                    const nx_watchdog_port_t* watchdog,
                                    uint8_t priority) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (warning == NULL || watchdog == NULL ||
        watchdog->ops != &nx_stm32f1_wwdg_ops || watchdog->context == NULL ||
        priority >= (1U << __NVIC_PRIO_BITS))
        return NX_ERROR_INVALID;
    if ((warning->effects & STICKY) != 0U || !backup_ready(warning) ||
        !watchdog_clocked())
        return NX_ERROR_STATE;
    nx_stm32f1_wwdg_t* owner = watchdog->context;
    nx_watchdog_state_t current;
    if (nx_watchdog_port_state(watchdog, &current) != NX_ERROR_UNSUPPORTED ||
        !current.enabled || !LL_WWDG_IsEnabled(WWDG) ||
        (WWDG->CFR & WWDG_CONFIG) != owner->config)
        return NX_ERROR_STATE;

    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_BUSY;
    if (vector_idle() && retained_word() == 0U &&
        (WWDG->CFR & WWDG_CFR_EWI) == 0U && (WWDG->SR & WWDG_SR_EWIF) == 0U &&
        LL_WWDG_GetCounter(WWDG) > WWDG_CR_T_6 + 1U) {
        /* Retain on any outcome once the reset-only bit might be set. The
         * existing watchdog feed compares against the actual CFR receipt. */
        warning->effects |= STICKY;
        owner->config |= WWDG_CFR_EWI;
        NX_STM32F1_WRITE(&WWDG->CFR, owner->config);
        nx_arch_dsb();
        result = NX_ERROR_IO;
        if (WWDG->CFR == owner->config) {
            NVIC_SetPriority(WWDG_IRQn, priority);
            NVIC_EnableIRQ(WWDG_IRQn);
            nx_arch_dsb();
            if (NVIC_GetPriority(WWDG_IRQn) == priority &&
                NVIC_GetEnableIRQ(WWDG_IRQn) != 0U)
                result = NX_SUCCESS;
        }
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_stm32f1_wwdg_ewi_irq(const nx_stm32f1_wwdg_ewi_t* warning) {
    if (!nx_arch_is_privileged() ||
        nx_arch_exception_number() != 16U + WWDG_IRQn)
        return NX_ERROR_CONTEXT;
    if (warning == NULL)
        return NX_ERROR_INVALID;
    if ((warning->effects & STICKY) == 0U || !watchdog_clocked() ||
        !LL_WWDG_IsEnabled(WWDG) || (WWDG->CFR & WWDG_CFR_EWI) == 0U ||
        (WWDG->SR & WWDG_SR_EWIF) == 0U)
        return NX_ERROR_STATE;
    const bool recorded = backup_ready(warning) && write_word(EWI_MARKER);
    /* EWIF is W0C. No handler path writes WWDG CR. */
    NX_STM32F1_WRITE(&WWDG->SR, 0U);
    nx_arch_dsb();
    return recorded && (WWDG->SR & WWDG_SR_EWIF) == 0U ? NX_SUCCESS
                                                       : NX_ERROR_IO;
}

nx_result_t nx_stm32f1_wwdg_ewi_close(nx_stm32f1_wwdg_ewi_t* warning) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (warning == NULL)
        return NX_ERROR_INVALID;
    if ((warning->effects & STICKY) != 0U)
        return NX_ERROR_UNSUPPORTED;
    warning->effects &= (uint8_t)~READY;
    if ((warning->effects & WRITE_ACCESS) != 0U) {
        if (!backup_clocked() || !write_access(false))
            return NX_ERROR_IO;
        warning->effects &= (uint8_t)~WRITE_ACCESS;
    }
    if ((warning->effects & GATES) != 0U &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, BACKUP_GATES, 0U))
        return NX_ERROR_IO;
    warning->effects = 0U;
    return NX_SUCCESS;
}
