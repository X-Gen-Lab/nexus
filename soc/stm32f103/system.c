/**
 * \file            system.c
 * \brief           Reset HSI8 admission and independent F103 TIM2 ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "stm32f103_system.h"
#ifndef NX_F103_MODEL
nx_stm32f103_system_t g_nx_stm32f103_system = {
    .rcc = RCC, .flash = FLASH, .timer = TIM2};
#endif
#ifndef NX_F103_FLASH_KIB
#define NX_F103_FLASH_KIB() (*(volatile const uint16_t*)UINT32_C(0x1ffff7e0))
#define NX_F103_DEVICE_ID() (DBGMCU->IDCODE)
#endif
#define NX_F103_COUNTER_MAX    UINT32_C(65535)
#define NX_F103_COUNTER_PERIOD UINT64_C(65536)
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool checked(volatile uint32_t* reg, uint32_t value, uint32_t mask) {
    NX_F103_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_F103_READ(reg) & mask) == (value & mask);
}
static bool timer_idle(const nx_stm32f103_system_t* s) {
    return NX_F103_READ(&s->timer->CR1) == 0U &&
           NX_F103_READ(&s->timer->CR2) == 0U &&
           NX_F103_READ(&s->timer->SMCR) == 0U &&
           NX_F103_READ(&s->timer->DIER) == 0U &&
           NX_F103_READ(&s->timer->CCER) == 0U &&
           NX_F103_READ(&s->timer->CCMR1) == 0U &&
           NX_F103_READ(&s->timer->CCMR2) == 0U &&
           NX_F103_READ(&s->timer->DCR) == 0U;
}
static _Noreturn void timer_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
void SystemInit(void) {
#ifndef NX_F103_MODEL
    /* The official startup calls this before .data/.bss exist. */
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(3U);
    nx_arch_dsb();
    nx_arch_isb();
#endif
}
nx_result_t nx_stm32f103_validate_identity(void) {
    return NX_F103_FLASH_KIB() == 128U &&
                   ((uint32_t)NX_F103_DEVICE_ID() & 0xfffU) == 0x410U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
nx_result_t nx_stm32f103_system_preflight(const nx_stm32f103_system_t* s) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (s == NULL || s->rcc == NULL || s->flash == NULL || s->timer == NULL) {
        return NX_ERROR_INVALID;
    }
    uint32_t cr = NX_F103_READ(&s->rcc->CR);
    uint32_t cfgr = NX_F103_READ(&s->rcc->CFGR);
    if (s->started || s->remaining_effects != 0U ||
        (cr & (RCC_CR_HSION | RCC_CR_HSIRDY)) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY) ||
        (cr & RCC_CR_HSITRIM) != (16U << RCC_CR_HSITRIM_Pos) ||
        (cr & (RCC_CR_PLLON | RCC_CR_PLLRDY | RCC_CR_HSEON | RCC_CR_HSERDY |
               RCC_CR_HSEBYP | RCC_CR_CSSON)) != 0U ||
        (cfgr & (RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                 RCC_CFGR_PPRE2)) != 0U ||
        (NX_F103_READ(&s->rcc->APB1ENR) & RCC_APB1ENR_TIM2EN) != 0U ||
        (NX_F103_READ(&s->rcc->APB1RSTR) & RCC_APB1RSTR_TIM2RST) != 0U ||
        !timer_idle(s) || (NX_F103_READ(&s->flash->SR) & FLASH_SR_BSY) != 0U ||
        !NX_F103_IRQ_AVAILABLE(TIM2_IRQn)) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
nx_stm32f103_start_result_t nx_stm32f103_system_start(nx_stm32f103_system_t* s,
                                                      uint32_t limit) {
    nx_stm32f103_start_result_t r = {NX_SUCCESS, NX_SUCCESS, 0U};
    r.primary = nx_stm32f103_system_preflight(s);
    if (r.primary != NX_SUCCESS) {
        return r;
    }
    if (limit == 0U) {
        r.primary = NX_ERROR_INVALID;
        return r;
    }
    r.primary = nx_stm32f103_validate_identity();
    if (r.primary != NX_SUCCESS) {
        return r;
    }
    s->reset_cause = NX_F103_READ(&s->rcc->CSR);
    s->timer_error = NX_SUCCESS;
    s->remaining_effects = NX_STM32F103_EFFECT_GATE;
    s->core_hz = 8000000U;
    bool valid = checked(&s->rcc->APB1ENR,
                         NX_F103_READ(&s->rcc->APB1ENR) | RCC_APB1ENR_TIM2EN,
                         RCC_APB1ENR_TIM2EN);
    if (!valid) {
        r.primary = NX_ERROR_IO;
        if (checked(&s->rcc->APB1ENR,
                    NX_F103_READ(&s->rcc->APB1ENR) &
                        ~(uint32_t)RCC_APB1ENR_TIM2EN,
                    RCC_APB1ENR_TIM2EN)) {
            s->remaining_effects = 0U;
        } else {
            r.cleanup = NX_ERROR_IO;
            r.remaining_effects = s->remaining_effects;
        }
        return r;
    }
    /* Gated-off register reads need not reveal retained configuration. Inspect
     * again after enabling access, before UG could emit a foreign trigger. */
    if (!timer_idle(s)) {
        r.primary = NX_ERROR_BUSY;
        if (checked(&s->rcc->APB1ENR,
                    NX_F103_READ(&s->rcc->APB1ENR) &
                        ~(uint32_t)RCC_APB1ENR_TIM2EN,
                    RCC_APB1ENR_TIM2EN)) {
            s->remaining_effects = 0U;
        } else {
            r.cleanup = NX_ERROR_IO;
            r.remaining_effects = s->remaining_effects;
        }
        return r;
    }
    s->remaining_effects |= NX_STM32F103_EFFECT_TIMER;
    valid = checked(&s->timer->CR1, 0U, UINT32_MAX) && valid;
    valid = checked(&s->timer->DIER, 0U, UINT32_MAX) && valid;
    valid = checked(&s->timer->PSC, 7U, 0xffffU) && valid;
    valid = checked(&s->timer->ARR, NX_F103_COUNTER_MAX, NX_F103_COUNTER_MAX) &&
            valid;
    valid = checked(&s->timer->SR, 0U, 0xffffU) && valid;
    if (!valid) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* MMS reset exports UG as TRGO. Select CEN while stopped to suppress the
     * prescaler update pulse,then restore reset routing before enabling CEN. */
    if (!checked(&s->timer->CR2, TIM_CR2_MMS_0, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    NX_F103_WRITE(&s->timer->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    while (limit-- != 0U) {
        NX_F103_SYSTEM_POLL(s);
        if ((NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
            break;
        }
    }
    if ((NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) == 0U) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    if (!checked(&s->timer->SR, 0U, 0xffffU) ||
        !checked(&s->timer->CNT, 0U, NX_F103_COUNTER_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    if (!checked(&s->timer->CR2, 0U, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    s->overflow_us = 0U;
    if (!checked(&s->timer->DIER, TIM_DIER_UIE, UINT32_MAX) ||
        !checked(&s->timer->CR1, TIM_CR1_CEN, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    r.primary = NX_F103_IRQ_CONTROL(TIM2_IRQn, true, 0U);
    if (r.primary != NX_SUCCESS) {
        goto rollback;
    }
    s->started = true;
    return r;
rollback:
    r.cleanup = nx_stm32f103_system_stop(s, 1U);
    r.remaining_effects = s->remaining_effects;
    return r;
}
nx_result_t nx_stm32f103_system_stop(nx_stm32f103_system_t* s, uint32_t limit) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (s == NULL || s->rcc == NULL || s->timer == NULL || limit == 0U) {
        return NX_ERROR_INVALID;
    }
    s->started = false;
    if ((s->remaining_effects & NX_STM32F103_EFFECT_TIMER) != 0U) {
        if (!checked(&s->timer->DIER, 0U, UINT32_MAX) ||
            !checked(&s->timer->CR1, 0U, UINT32_MAX) ||
            !checked(&s->timer->CR2, 0U, UINT32_MAX) ||
            !checked(&s->timer->SR, 0U, 0xffffU)) {
            return NX_ERROR_IO;
        }
        /* Withdraw the source before the final pending-vector proof. A wrap
         * between these operations must not survive the timer lease. */
        nx_result_t irq = NX_F103_IRQ_CONTROL(TIM2_IRQn, false, 0U);
        if (irq != NX_SUCCESS) {
            return irq;
        }
        s->remaining_effects &= ~NX_STM32F103_EFFECT_TIMER;
    }
    /* A gate lease alone never permits touching a foreign retained timer. */
    if ((s->remaining_effects & NX_STM32F103_EFFECT_GATE) != 0U) {
        if (!checked(&s->rcc->APB1ENR,
                     NX_F103_READ(&s->rcc->APB1ENR) &
                         ~(uint32_t)RCC_APB1ENR_TIM2EN,
                     RCC_APB1ENR_TIM2EN)) {
            return NX_ERROR_IO;
        }
        s->remaining_effects &= ~NX_STM32F103_EFFECT_GATE;
    }
    return NX_SUCCESS;
}
void nx_stm32f103_system_timer_irq(nx_stm32f103_system_t* s) {
    if (s == NULL || (s->remaining_effects & NX_STM32F103_EFFECT_TIMER) == 0U ||
        (NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) == 0U) {
        return;
    }
    NX_F103_WRITE(&s->timer->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        s->started = false;
        s->timer_error = NX_ERROR_IO;
        (void)checked(&s->timer->DIER, 0U, UINT32_MAX);
        (void)checked(&s->timer->CR1, 0U, TIM_CR1_CEN);
        (void)NX_F103_IRQ_CONTROL(TIM2_IRQn, false, 0U);
        timer_park();
    }
    if (s->started) {
        s->overflow_us += NX_F103_COUNTER_PERIOD;
    }
}
uint64_t nx_stm32f103_system_now(nx_stm32f103_system_t* s) {
    if (s == NULL || !s->started || !nx_arch_is_privileged()) {
        return 0U;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!s->started) {
        nx_arch_irq_restore(saved);
        return 0U;
    }
    uint64_t high = s->overflow_us;
    uint32_t count = NX_F103_READ(&s->timer->CNT) & NX_F103_COUNTER_MAX;
    if ((NX_F103_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        high += NX_F103_COUNTER_PERIOD;
        count = NX_F103_READ(&s->timer->CNT) & NX_F103_COUNTER_MAX;
    }
    nx_arch_irq_restore(saved);
    return high + count;
}
nx_time_us_t nx_time_now_us(void) {
    return nx_stm32f103_system_now(&g_nx_stm32f103_system);
}
void TIM2_IRQHandler(void) {
    nx_stm32f103_system_timer_irq(&g_nx_stm32f103_system);
}
