/**
 * \file            system.c
 * \brief           Cold H743 HSI64 reference and reserved TIM2 ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "boot_registers.h"
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "stm32h743_system.h"
#include <stddef.h>
_Static_assert(PWR_BASE + offsetof(PWR_TypeDef, CR3) == NX_H743_BOOT_PWR_CR3,
               "Actual SDK PWR CR3 differs from pre-RAM assembly");
_Static_assert(PWR_BASE + offsetof(PWR_TypeDef, CSR1) == NX_H743_BOOT_PWR_CSR1,
               "Actual SDK PWR CSR1 differs from pre-RAM assembly");
_Static_assert(PWR_CR3_LDOEN == NX_H743_BOOT_LDO &&
                   PWR_CR3_BYPASS == NX_H743_BOOT_BYPASS &&
                   PWR_CSR1_ACTVOSRDY == NX_H743_BOOT_READY,
               "Actual SDK supply/ready bits differ from pre-RAM assembly");
#ifndef NX_H743_MODEL
/* The optional control uses the pointer-to-epoch alignment gap. Keep the
 * default profile free of cache storage and bound the selected ARM layout. */
_Static_assert(sizeof(nx_stm32h743_system_t) ==
                   (NX_H743_CACHE_CONTROL_ENABLED ? 56U : 48U),
               "H743 selected system state exceeds its reviewed ARM budget");
nx_stm32h743_system_t g_nx_stm32h743_system = {
    .rcc = RCC, .flash = FLASH, .power = PWR, .timer = TIM2, .core = SCB};
#endif
#ifndef NX_H743_FLASH_KIB
#define NX_H743_FLASH_KIB() (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_H743_DEVICE_ID() (DBGMCU->IDCODE)
#endif
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_H743_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_H743_READ(reg) & mask) == (value & mask);
}
/** \brief CEN/DIER alone cannot exclude trigger or pin/capture owners. */
static bool timer_idle(const nx_stm32h743_system_t* s) {
    return (NX_H743_READ(&s->timer->CR1) | NX_H743_READ(&s->timer->CR2) |
            NX_H743_READ(&s->timer->SMCR) | NX_H743_READ(&s->timer->DIER) |
            NX_H743_READ(&s->timer->CCER) | NX_H743_READ(&s->timer->CCMR1) |
            NX_H743_READ(&s->timer->CCMR2)) == 0U;
}
static bool wait_update(nx_stm32h743_system_t* s, uint32_t limit) {
    while (limit-- != 0U) {
        NX_H743_SYSTEM_POLL(s);
        if ((NX_H743_READ(&s->timer->SR) & TIM_SR_UIF) != 0U)
            return true;
    }
    return false;
}
/** \brief A latched divider is not proof of its propagated clock frequency. */
static nx_result_t wait_divider(nx_stm32h743_system_t* s, uint32_t divider,
                                uint32_t limit) {
    while (limit-- != 0U) {
        NX_H743_SYSTEM_POLL(s);
        const uint32_t cr = NX_H743_READ(&s->rcc->CR);
        if ((cr &
             (RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSIDIV | RCC_CR_HSIDIVF)) ==
            (RCC_CR_HSION | RCC_CR_HSIRDY | divider | RCC_CR_HSIDIVF))
            return NX_SUCCESS;
    }
    return NX_ERROR_TIMEOUT;
}
#if NX_H743_CACHE_CONTROL_ENABLED
static nx_result_t cache_result(nx_arch_cache_control_result_t result) {
    switch (result) {
        case NX_ARCH_CACHE_OK:
            return NX_SUCCESS;
        case NX_ARCH_CACHE_INVALID:
            return NX_ERROR_INVALID;
        case NX_ARCH_CACHE_CONTEXT:
            return NX_ERROR_CONTEXT;
        case NX_ARCH_CACHE_UNSUPPORTED:
            return NX_ERROR_UNSUPPORTED;
        case NX_ARCH_CACHE_STATE:
            return NX_ERROR_BUSY;
        default:
            return NX_ERROR_IO;
    }
}
#endif
/** \brief A lost time domain cannot authorize subsequent deadline progress. */
static _Noreturn void timer_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
nx_result_t nx_stm32h743_system_preflight(const nx_stm32h743_system_t* s) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->rcc == NULL || s->flash == NULL || s->power == NULL ||
        s->timer == NULL || s->core == NULL)
        return NX_ERROR_INVALID;
    if (s->started || s->remaining_effects != 0U)
        return NX_ERROR_BUSY;
#if NX_H743_CACHE_CONTROL_ENABLED
    if (s->cache.owned_mask != 0U || s->cache.selector_pending)
        return NX_ERROR_BUSY;
#endif
    /* Never adopt a clock/cache/power state whose owner cannot be withdrawn.
     * The assembly power entry has already permitted initialized RAM use. */
    uint32_t cr = NX_H743_READ(&s->rcc->CR);
    if ((cr & (RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSIDIV)) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY) ||
        (cr & (RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_PLL1ON | RCC_CR_PLL2ON |
               RCC_CR_PLL3ON)) != 0U ||
        (NX_H743_READ(&s->rcc->CFGR) & (RCC_CFGR_SW | RCC_CFGR_SWS)) !=
            (RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI) ||
        (NX_H743_READ(&s->rcc->D1CFGR) &
         (RCC_D1CFGR_D1CPRE | RCC_D1CFGR_HPRE | RCC_D1CFGR_D1PPRE)) != 0U ||
        (NX_H743_READ(&s->rcc->D2CFGR) &
         (RCC_D2CFGR_D2PPRE1 | RCC_D2CFGR_D2PPRE2)) != 0U ||
        (NX_H743_READ(&s->rcc->D3CFGR) & RCC_D3CFGR_D3PPRE) != 0U ||
        (NX_H743_READ(&s->core->CCR) & (SCB_CCR_DC_Msk | SCB_CCR_IC_Msk)) !=
            0U ||
        (NX_H743_READ(&s->power->CR3) & (PWR_CR3_LDOEN | PWR_CR3_BYPASS)) !=
            PWR_CR3_LDOEN ||
        (NX_H743_READ(&s->power->CSR1) & PWR_CSR1_ACTVOSRDY) == 0U ||
        (NX_H743_READ(&s->flash->ACR) & FLASH_ACR_LATENCY) !=
            FLASH_LATENCY_DEFAULT ||
        ((NX_H743_READ(&s->flash->SR1) | NX_H743_READ(&s->flash->SR2)) &
         FLASH_SR_BSY) != 0U ||
        (NX_H743_READ(&s->rcc->APB1LENR) & RCC_APB1LENR_TIM2EN) != 0U ||
        !timer_idle(s) || !NX_H743_IRQ_AVAILABLE(TIM2_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_stm32h743_start_result_t nx_stm32h743_system_start(nx_stm32h743_system_t* s,
                                                      uint32_t core_hz,
                                                      uint32_t cache_mask,
                                                      uint32_t limit) {
    nx_stm32h743_start_result_t r = {NX_SUCCESS, NX_SUCCESS, 0U};
    if (limit == 0U || (core_hz != 64000000U && core_hz != 32000000U) ||
        (cache_mask != 0U &&
         cache_mask != (NX_ARCH_CACHE_DATA | NX_ARCH_CACHE_INSTRUCTION))) {
        r.primary = NX_ERROR_INVALID;
        return r;
    }
#ifndef NX_H743_MODEL
    if (core_hz != NEXUS_CORE_HZ || cache_mask != NEXUS_SOC_CACHE_MASK) {
        r.primary = NX_ERROR_INVALID;
        return r;
    }
#endif
    r.primary = nx_stm32h743_system_preflight(s);
    if (r.primary != NX_SUCCESS)
        return r;
    s->timer_error = NX_SUCCESS;
    s->core_hz = 64000000U;
    s->reset_cause = NX_H743_READ(&s->rcc->RSR);
    /* An off peripheral gate can hide retained controls. Only the reversible
     * gate is ours until a second observation proves the timer is unowned. */
    s->remaining_effects = NX_STM32H743_EFFECT_TIMER_GATE;
    if (!write_checked(&s->rcc->APB1LENR,
                       NX_H743_READ(&s->rcc->APB1LENR) | RCC_APB1LENR_TIM2EN,
                       RCC_APB1LENR_TIM2EN)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    if (!timer_idle(s)) {
        r.primary = NX_ERROR_BUSY;
        goto rollback;
    }
    s->remaining_effects = NX_STM32H743_EFFECT_TIMER;
    if (core_hz == 32000000U) {
        s->remaining_effects |= NX_STM32H743_EFFECT_DIVIDER;
        NX_H743_WRITE(&s->rcc->CR,
                      (NX_H743_READ(&s->rcc->CR) & ~(uint32_t)RCC_CR_HSIDIV) |
                          RCC_CR_HSIDIV_2);
        nx_arch_dsb();
        const uint32_t divider = NX_H743_READ(&s->rcc->CR) & RCC_CR_HSIDIV;
        if (divider != RCC_CR_HSIDIV_2) {
            /* The exclusively owned register never accepted DIV2. Reset DIV1
             * has HSIDIVF=0, so it must not require an invented transition. */
            if (divider == RCC_CR_HSIDIV_1)
                s->remaining_effects &= ~NX_STM32H743_EFFECT_DIVIDER;
            r.primary = NX_ERROR_IO;
            goto rollback;
        }
        r.primary = wait_divider(s, RCC_CR_HSIDIV_2, limit);
        if (r.primary != NX_SUCCESS)
            goto rollback;
        s->core_hz = core_hz;
    }
#if NX_H743_CACHE_CONTROL_ENABLED
    if (cache_mask != 0U) {
        s->remaining_effects |= NX_STM32H743_EFFECT_CACHE;
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        r.primary = cache_result(NX_H743_CACHE_ENABLE(&s->cache, cache_mask));
        nx_arch_irq_restore(saved);
        if (r.primary != NX_SUCCESS) {
            if (s->cache.owned_mask == 0U && !s->cache.selector_pending)
                s->remaining_effects &= ~NX_STM32H743_EFFECT_CACHE;
            goto rollback;
        }
    }
#endif
    /* Reset only the reserved controller, never another peripheral domain. */
    if (!write_checked(&s->rcc->APB1LRSTR,
                       NX_H743_READ(&s->rcc->APB1LRSTR) | RCC_APB1LRSTR_TIM2RST,
                       RCC_APB1LRSTR_TIM2RST) ||
        !write_checked(&s->rcc->APB1LRSTR,
                       NX_H743_READ(&s->rcc->APB1LRSTR) &
                           ~(uint32_t)RCC_APB1LRSTR_TIM2RST,
                       RCC_APB1LRSTR_TIM2RST) ||
        !write_checked(&s->timer->PSC, core_hz / 1000000U - 1U, UINT32_MAX) ||
        !write_checked(&s->timer->ARR, UINT32_MAX, UINT32_MAX) ||
        !write_checked(&s->timer->SR, 0U, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* MMS=000 exports UG on TRGO. Select the disabled CEN signal instead
     * while updating the prescaler, then restore reset MMS before CEN starts.
     */
    s->remaining_effects |= NX_STM32H743_EFFECT_TIMER_QUIET;
    if (!write_checked(&s->timer->CR2, TIM_CR2_MMS_0, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    NX_H743_WRITE(&s->timer->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if (!wait_update(s, limit)) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    if (!write_checked(&s->timer->SR, 0U, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    if (!write_checked(&s->timer->CR2,
                       NX_H743_READ(&s->timer->CR2) & ~(uint32_t)TIM_CR2_MMS,
                       TIM_CR2_MMS)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    s->remaining_effects &= ~NX_STM32H743_EFFECT_TIMER_QUIET;
    NX_H743_WRITE(&s->timer->CNT, 0U);
    s->overflow_us = 0U;
    if (!write_checked(&s->timer->DIER, TIM_DIER_UIE, UINT32_MAX) ||
        !write_checked(&s->timer->CR1, TIM_CR1_CEN, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    r.primary = NX_H743_IRQ_CONTROL(TIM2_IRQn, true, 0U);
    if (r.primary != NX_SUCCESS)
        goto rollback;
    s->started = true;
    return r;
rollback:
    r.cleanup = nx_stm32h743_system_stop(s, limit);
    r.remaining_effects = s->remaining_effects;
    return r;
}
nx_result_t nx_stm32h743_system_stop(nx_stm32h743_system_t* s, uint32_t limit) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->rcc == NULL || s->timer == NULL || limit == 0U)
        return NX_ERROR_INVALID;
    if ((s->remaining_effects & NX_STM32H743_EFFECT_TIMER) != 0U) {
        s->started = false;
        if (!write_checked(&s->timer->DIER, 0U, UINT32_MAX) ||
            !write_checked(&s->timer->CR1, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        if ((s->remaining_effects & NX_STM32H743_EFFECT_TIMER_QUIET) != 0U) {
            if (!write_checked(&s->timer->CR2,
                               NX_H743_READ(&s->timer->CR2) &
                                   ~(uint32_t)TIM_CR2_MMS,
                               TIM_CR2_MMS))
                return NX_ERROR_IO;
            s->remaining_effects &= ~NX_STM32H743_EFFECT_TIMER_QUIET;
        }
        if (!write_checked(&s->timer->SR, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        /* A wrap can pend while DIER/CEN withdraw. Only after the peripheral
         * is quiet can final NVIC disable/clear prove no future publication. */
        nx_result_t irq = NX_H743_IRQ_CONTROL(TIM2_IRQn, false, 0U);
        if (irq != NX_SUCCESS)
            return irq;
        if (!write_checked(&s->rcc->APB1LRSTR,
                           NX_H743_READ(&s->rcc->APB1LRSTR) &
                               ~(uint32_t)RCC_APB1LRSTR_TIM2RST,
                           RCC_APB1LRSTR_TIM2RST) ||
            !write_checked(&s->rcc->APB1LENR,
                           NX_H743_READ(&s->rcc->APB1LENR) &
                               ~(uint32_t)RCC_APB1LENR_TIM2EN,
                           RCC_APB1LENR_TIM2EN))
            return NX_ERROR_IO;
        s->remaining_effects &= ~NX_STM32H743_EFFECT_TIMER;
    }
    if ((s->remaining_effects & NX_STM32H743_EFFECT_TIMER_GATE) != 0U) {
        /* Foreign retained timer state was never acquired: do not touch its
         * registers, reset, pending flags or vector during gate rollback. */
        if (!write_checked(&s->rcc->APB1LENR,
                           NX_H743_READ(&s->rcc->APB1LENR) &
                               ~(uint32_t)RCC_APB1LENR_TIM2EN,
                           RCC_APB1LENR_TIM2EN))
            return NX_ERROR_IO;
        s->remaining_effects &= ~NX_STM32H743_EFFECT_TIMER_GATE;
    }
#if NX_H743_CACHE_CONTROL_ENABLED
    if ((s->remaining_effects & NX_STM32H743_EFFECT_CACHE) != 0U) {
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        nx_result_t cache = cache_result(NX_H743_CACHE_DISABLE(&s->cache));
        nx_arch_irq_restore(saved);
        if (cache != NX_SUCCESS)
            return cache;
        s->remaining_effects &= ~NX_STM32H743_EFFECT_CACHE;
    }
#endif
    if ((s->remaining_effects & NX_STM32H743_EFFECT_DIVIDER) != 0U) {
        uint32_t cr = NX_H743_READ(&s->rcc->CR);
        if ((cr & (RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON)) != 0U)
            return NX_ERROR_BUSY;
        if (!write_checked(&s->rcc->CR, cr & ~(uint32_t)RCC_CR_HSIDIV,
                           RCC_CR_HSIDIV))
            return NX_ERROR_IO;
        nx_result_t clock = wait_divider(s, RCC_CR_HSIDIV_1, limit);
        if (clock != NX_SUCCESS)
            return clock;
        s->core_hz = 64000000U;
        s->remaining_effects &= ~NX_STM32H743_EFFECT_DIVIDER;
    }
    return NX_SUCCESS;
}
uint64_t nx_stm32h743_system_now(nx_stm32h743_system_t* s) {
    if (s == NULL || !nx_arch_is_privileged())
        return 0U;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    /* An overflow fault may invalidate the epoch before mask acquisition.
     * Decide validity inside the same exclusion as both counter words. */
    if (!s->started) {
        nx_arch_irq_restore(guard);
        return 0U;
    }
    uint64_t high = s->overflow_us;
    uint32_t count = NX_H743_READ(&s->timer->CNT);
    if ((NX_H743_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        high += (UINT64_C(1) << 32U);
        count = NX_H743_READ(&s->timer->CNT);
    }
    nx_arch_irq_restore(guard);
    return high + count;
}
void nx_stm32h743_system_timer_irq(nx_stm32h743_system_t* s) {
    if (s != NULL && s->timer != NULL &&
        (s->started ||
         (s->remaining_effects & NX_STM32H743_EFFECT_TIMER) != 0U) &&
        (NX_H743_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        if (!write_checked(&s->timer->SR, ~(uint32_t)TIM_SR_UIF, TIM_SR_UIF)) {
            /* A failed acknowledgement invalidates this epoch. Never count
             * the same still-pending flag twice or revoke cleanup ownership.
             * Withdrawal is finite in ISR context; no poll or kernel call. */
            s->timer_error = NX_ERROR_IO;
            s->started = false;
            s->remaining_effects |= NX_STM32H743_EFFECT_TIMER;
            (void)write_checked(&s->timer->DIER, 0U, UINT32_MAX);
            (void)write_checked(&s->timer->CR1, 0U, UINT32_MAX);
            (void)NX_H743_IRQ_CONTROL(TIM2_IRQn, false, 0U);
            /* Source isolation does not make old consumer deadlines usable. */
            timer_park();
        }
        if (s->started)
            s->overflow_us += (UINT64_C(1) << 32U);
    }
}
nx_result_t nx_stm32h743_validate_identity(void) {
    return (uint32_t)NX_H743_FLASH_KIB() == 2048U &&
                   ((uint32_t)NX_H743_DEVICE_ID() & 0xfffU) == 0x450U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
void SystemInit(void) {
#ifndef NX_H743_MODEL
    /* ExitRun0Mode permits stack use, but DATA/BSS still do not exist. */
    SCB->CPACR |= UINT32_C(0x00f00000);
    nx_arch_dsb();
    nx_arch_isb();
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(3U);
#endif
}
nx_time_us_t nx_time_now_us(void) {
    return nx_stm32h743_system_now(&g_nx_stm32h743_system);
}
void TIM2_IRQHandler(void) {
    nx_stm32h743_system_timer_irq(&g_nx_stm32h743_system);
}
