/**
 * \file            system.c
 * \brief           TZEN0 MSIS4MHz cold profile and independent TIM2 ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "stm32u575_system.h"
#if defined(NEXUS_MEMORY_SRAM4) && NEXUS_MEMORY_SRAM4 == 1
#include "stm32u575_memory_init.h"
#endif
#ifndef NX_U575_MODEL
nx_stm32u575_system_t g_nx_stm32u575_system = {.rcc = RCC,
                                               .flash = FLASH,
                                               .timer = TIM2,
                                               .icache = ICACHE,
                                               .dcache = DCACHE1};
#endif
#ifndef NX_U575_FLASH_KIB
#define NX_U575_FLASH_KIB()     (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_U575_FLASH_OPTIONS() (FLASH->OPTR)
#endif
#ifndef NX_U575_CPU_SCB
#define NX_U575_CPU_SCB          SCB
#define NX_U575_CPU_GROUP(value) NVIC_SetPriorityGrouping(value)
#endif
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_U575_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_U575_READ(reg) & mask) == (value & mask);
}
static bool wait_update(nx_stm32u575_system_t* s, uint32_t limit) {
    while (limit-- != 0U) {
        NX_U575_SYSTEM_POLL(s);
        if ((NX_U575_READ(&s->timer->SR) & TIM_SR_UIF) != 0U)
            return true;
    }
    return false;
}
/** \brief A lost time domain cannot authorize subsequent deadline progress. */
static _Noreturn void timer_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
void SystemInit(void) {
    /* Official Reset_Handler invokes this before primary .data/.bss. The
     * pointers are CPU/MMIO constants, not initialized context or clocks. */
    NX_U575_CPU_SCB->CPACR |= UINT32_C(0x00f00000);
    nx_arch_dsb();
    nx_arch_isb();
    NX_U575_CPU_SCB->VTOR = UINT32_C(0x08000000);
    NX_U575_CPU_GROUP(3U);
    nx_arch_dsb();
    nx_arch_isb();
#if defined(NEXUS_MEMORY_SRAM4) && NEXUS_MEMORY_SRAM4 == 1
    /* Only linker constants and the reset SRAM1 stack exist. A failed SRAM4
     * image or prepared run gate cannot reach primary RAM or constructors. */
    if (!nx_stm32u575_memory_boot()) {
        (void)nx_arch_irq_save();
        for (;;) {
        }
    }
#endif
}
nx_result_t nx_stm32u575_validate_identity(void) {
    uint32_t options = NX_U575_FLASH_OPTIONS();
    return NX_U575_FLASH_KIB() == 2048U &&
                   (options & (FLASH_OPTR_TZEN | FLASH_OPTR_SWAP_BANK)) == 0U
               ? NX_SUCCESS
               : NX_ERROR_UNSUPPORTED;
}
static bool timer_available(TIM_TypeDef* timer);
nx_result_t nx_stm32u575_system_preflight(const nx_stm32u575_system_t* s) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->rcc == NULL || s->flash == NULL || s->timer == NULL ||
        s->icache == NULL || s->dcache == NULL)
        return NX_ERROR_INVALID;
    if (s->started || s->remaining_effects != 0U)
        return NX_ERROR_BUSY;
    nx_result_t identity = nx_stm32u575_validate_identity();
    if (identity != NX_SUCCESS)
        return identity;
    uint32_t cr = NX_U575_READ(&s->rcc->CR);
    uint32_t ranges = NX_U575_READ(&s->rcc->ICSCR1);
    uint32_t range =
        (ranges & RCC_ICSCR1_MSIRGSEL) == 0U
            ? (NX_U575_READ(&s->rcc->CSR) & (uint32_t)RCC_CSR_MSISSRANGE) >>
                  RCC_CSR_MSISSRANGE_Pos
            : (ranges & (uint32_t)RCC_ICSCR1_MSISRANGE) >>
                  RCC_ICSCR1_MSISRANGE_Pos;
    if ((cr & (RCC_CR_MSISON | RCC_CR_MSISRDY)) !=
            (RCC_CR_MSISON | RCC_CR_MSISRDY) ||
        range != 4U ||
        (cr & (RCC_CR_MSIPLLEN | RCC_CR_HSION | RCC_CR_HSEON | RCC_CR_PLL1ON |
               RCC_CR_PLL2ON | RCC_CR_PLL3ON | RCC_CR_PLL1RDY | RCC_CR_PLL2RDY |
               RCC_CR_PLL3RDY)) != 0U ||
        NX_U575_READ(&s->rcc->CFGR1) != 0U ||
        NX_U575_READ(&s->rcc->CFGR2) != 0U ||
        NX_U575_READ(&s->rcc->CFGR3) != 0U ||
        (NX_U575_READ(&s->icache->CR) & ICACHE_CR_EN) != 0U ||
        (NX_U575_READ(&s->dcache->CR) & DCACHE_CR_EN) != 0U ||
        (NX_U575_READ(&s->flash->NSSR) & FLASH_NSSR_BSY) != 0U ||
        (NX_U575_READ(&s->rcc->APB1ENR1) & RCC_APB1ENR1_TIM2EN) != 0U ||
        !timer_available(s->timer) || !NX_U575_IRQ_AVAILABLE(TIM2_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
/** \brief UG is safe only after excluding trigger and channel ownership. */
static bool timer_available(TIM_TypeDef* timer) {
    return NX_U575_READ(&timer->CR1) == 0U && NX_U575_READ(&timer->CR2) == 0U &&
           NX_U575_READ(&timer->SMCR) == 0U &&
           NX_U575_READ(&timer->DIER) == 0U &&
           NX_U575_READ(&timer->CCER) == 0U &&
           NX_U575_READ(&timer->CCMR1) == 0U &&
           NX_U575_READ(&timer->CCMR2) == 0U;
}
nx_result_t nx_stm32u575_system_stop(nx_stm32u575_system_t* s, uint32_t limit) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->rcc == NULL || s->timer == NULL || limit == 0U)
        return NX_ERROR_INVALID;
    if (s->remaining_effects == 0U)
        return NX_SUCCESS;
    s->started = false;
    if ((s->remaining_effects & NX_STM32U575_EFFECT_TIMER) != 0U &&
        (NX_U575_READ(&s->rcc->APB1ENR1) & RCC_APB1ENR1_TIM2EN) != 0U) {
        if (!write_checked(&s->timer->DIER, 0U, UINT32_MAX) ||
            !write_checked(&s->timer->CR1, 0U, UINT32_MAX) ||
            !write_checked(&s->timer->CR2, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        NX_U575_WRITE(&s->timer->SR, 0U);
        nx_arch_dsb();
        if (NX_U575_READ(&s->timer->SR) != 0U)
            return NX_ERROR_IO;
    }
    if ((s->remaining_effects & NX_STM32U575_EFFECT_TIMER) != 0U) {
        /* Source withdrawal may itself race the last overflow. Clear/read
         * NVIC only after DIER/CEN and the peripheral flag are observed off. */
        nx_result_t irq = NX_U575_IRQ_CONTROL(TIM2_IRQn, false, 0U);
        if (irq != NX_SUCCESS)
            return irq;
    }
    if (!write_checked(&s->rcc->APB1ENR1,
                       NX_U575_READ(&s->rcc->APB1ENR1) &
                           ~(uint32_t)RCC_APB1ENR1_TIM2EN,
                       RCC_APB1ENR1_TIM2EN))
        return NX_ERROR_IO;
    s->remaining_effects = 0U;
    return NX_SUCCESS;
}
nx_stm32u575_start_result_t nx_stm32u575_system_start(nx_stm32u575_system_t* s,
                                                      uint32_t limit) {
    nx_stm32u575_start_result_t r = {NX_SUCCESS, NX_SUCCESS, 0U};
    r.primary = nx_stm32u575_system_preflight(s);
    if (r.primary != NX_SUCCESS)
        return r;
    if (limit == 0U) {
        r.primary = NX_ERROR_INVALID;
        return r;
    }
    /* No RCC, power, Flash or option-byte reset sequence is needed. A verified
     * existing 4MHz source remains unchanged; only the reserved timer is owned.
     */
    s->remaining_effects = NX_STM32U575_EFFECT_TIMER_GATE;
    s->timer_error = NX_SUCCESS;
    s->reset_cause = NX_U575_READ(&s->rcc->CSR);
    if (!write_checked(&s->rcc->APB1ENR1,
                       NX_U575_READ(&s->rcc->APB1ENR1) | RCC_APB1ENR1_TIM2EN,
                       RCC_APB1ENR1_TIM2EN)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* An unclocked APB read may return zero. Recheck the actual timer after
     * observing its gate, before the first timer write or UG side effect. */
    if (!timer_available(s->timer)) {
        r.primary = NX_ERROR_BUSY;
        goto rollback;
    }
    s->remaining_effects = NX_STM32U575_EFFECT_TIMER;
    /* Reset MMS exports UG as TRGO. Select CEN while CEN=0 so the prescaler
     * update cannot stimulate an external timer/ADC consumer. CR2 was admitted
     * unused and is now owned through cleanup, including failed readback. */
    if (!write_checked(&s->timer->CR2, TIM_CR2_MMS_0, UINT32_MAX) ||
        !write_checked(&s->timer->PSC, 3U, UINT32_MAX) ||
        !write_checked(&s->timer->ARR, UINT32_MAX, UINT32_MAX) ||
        !write_checked(&s->timer->CNT, 0U, UINT32_MAX) ||
        !write_checked(&s->timer->SR, 0U, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    NX_U575_WRITE(&s->timer->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if (!wait_update(s, limit)) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    if (!write_checked(&s->timer->SR, 0U, UINT32_MAX) ||
        !write_checked(&s->timer->CR2, 0U, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    s->overflow_us = 0U;
    s->core_hz = 4000000U;
    if (!write_checked(&s->timer->DIER, TIM_DIER_UIE, UINT32_MAX) ||
        !write_checked(&s->timer->CR1, TIM_CR1_CEN | TIM_CR1_URS, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    r.primary = NX_U575_IRQ_CONTROL(TIM2_IRQn, true, 0U);
    if (r.primary != NX_SUCCESS)
        goto rollback;
    s->started = true;
    return r;
rollback:
    r.cleanup = nx_stm32u575_system_stop(s, limit);
    r.remaining_effects = s->remaining_effects;
    return r;
}
void nx_stm32u575_system_timer_irq(nx_stm32u575_system_t* s) {
    if (s != NULL && s->timer != NULL &&
        (s->remaining_effects & NX_STM32U575_EFFECT_TIMER) != 0U &&
        (NX_U575_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        if (!write_checked(&s->timer->SR, ~(uint32_t)TIM_SR_UIF, TIM_SR_UIF)) {
            /* Never count one still-pending wrap twice. Retain cleanup
             * ownership while finite ISR actions withdraw the source. */
            s->timer_error = NX_ERROR_IO;
            s->started = false;
            s->remaining_effects |= NX_STM32U575_EFFECT_TIMER;
            (void)write_checked(&s->timer->DIER, 0U, UINT32_MAX);
            (void)write_checked(&s->timer->CR1, 0U, UINT32_MAX);
            (void)NX_U575_IRQ_CONTROL(TIM2_IRQn, false, 0U);
            /* Source isolation does not make old consumer deadlines usable. */
            timer_park();
        }
        if (s->started)
            s->overflow_us += (UINT64_C(1) << 32U);
    }
}
uint64_t nx_stm32u575_system_now(nx_stm32u575_system_t* s) {
    if (s == NULL || !s->started || !nx_arch_is_privileged())
        return 0U;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    /* A delivered overflow fault may invalidate the epoch before masking. */
    if (!s->started) {
        nx_arch_irq_restore(guard);
        return 0U;
    }
    uint64_t high = s->overflow_us;
    uint32_t count = NX_U575_READ(&s->timer->CNT);
    if ((NX_U575_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        high += (UINT64_C(1) << 32U);
        count = NX_U575_READ(&s->timer->CNT);
    }
    nx_arch_irq_restore(guard);
    return high + count;
}
nx_time_us_t nx_time_now_us(void) {
    return nx_stm32u575_system_now(&g_nx_stm32u575_system);
}
void TIM2_IRQHandler(void) {
    nx_stm32u575_system_timer_irq(&g_nx_stm32u575_system);
}
