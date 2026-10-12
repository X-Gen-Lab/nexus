/**
 * \file            system.c
 * \brief           Fixed G474 boost170MHz transaction and independent TIM2
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "stm32g474_system.h"
#ifndef NX_G474_MODEL
nx_stm32g474_system_t g_nx_stm32g474_system = {
    .rcc = RCC, .flash = FLASH, .power = PWR, .timer = TIM2};
#endif
#ifndef NX_G474_FLASH_KIB
#define NX_G474_FLASH_KIB()     (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_G474_DEVICE_ID()     (DBGMCU->IDCODE)
#define NX_G474_FLASH_OPTIONS() (FLASH->OPTR)
#endif
/** \brief Preserve masks and effects when an IRQ source cannot be isolated. */
static _Noreturn void timer_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_G474_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_G474_READ(reg) & mask) == (value & mask);
}
static bool wait_bits(nx_stm32g474_system_t* s, volatile uint32_t* reg,
                      uint32_t mask, uint32_t value, uint32_t limit) {
    while (limit-- != 0U) {
        NX_G474_SYSTEM_POLL(s);
        if ((NX_G474_READ(reg) & mask) == value)
            return true;
    }
    return false;
}
static nx_result_t restore_hsi(nx_stm32g474_system_t* s, uint32_t limit) {
    NX_G474_WRITE(&s->rcc->CR, NX_G474_READ(&s->rcc->CR) | RCC_CR_HSION);
    if (!wait_bits(s, &s->rcc->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY, limit))
        return NX_ERROR_TIMEOUT;
    uint32_t cfgr = NX_G474_READ(&s->rcc->CFGR);
    NX_G474_WRITE(&s->rcc->CFGR,
                  (cfgr & ~(uint32_t)RCC_CFGR_SW) | RCC_CFGR_SW_HSI);
    if (!wait_bits(s, &s->rcc->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI, limit))
        return NX_ERROR_TIMEOUT;
    if (!write_checked(
            &s->rcc->CFGR,
            NX_G474_READ(&s->rcc->CFGR) &
                ~(uint32_t)(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2),
            RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
        return NX_ERROR_IO;
    s->core_hz = 16000000U;
    NX_G474_WRITE(&s->rcc->CR, NX_G474_READ(&s->rcc->CR) &
                                   ~(uint32_t)(RCC_CR_PLLON | RCC_CR_HSEON));
    if (!wait_bits(s, &s->rcc->CR, RCC_CR_PLLRDY | RCC_CR_HSERDY, 0U, limit))
        return NX_ERROR_TIMEOUT;
    bool valid = write_checked(&s->rcc->PLLCFGR, s->saved_pll, UINT32_MAX);
    valid = write_checked(&s->flash->ACR, s->saved_flash, UINT32_MAX) && valid;
    if (s->power_snapshotted) {
        valid =
            write_checked(&s->power->CR5, s->saved_boost, UINT32_MAX) && valid;
        valid =
            write_checked(&s->power->CR1, s->saved_power, UINT32_MAX) && valid;
        if (!wait_bits(s, &s->power->SR2, PWR_SR2_VOSF, 0U, limit))
            return NX_ERROR_TIMEOUT;
    }
    if (!valid)
        return NX_ERROR_IO;
    if (s->owned_power_clock &&
        !write_checked(&s->rcc->APB1ENR1,
                       NX_G474_READ(&s->rcc->APB1ENR1) &
                           ~(uint32_t)RCC_APB1ENR1_PWREN,
                       RCC_APB1ENR1_PWREN))
        return NX_ERROR_IO;
    s->remaining_effects &= ~NX_STM32G474_EFFECT_CLOCK;
    return NX_SUCCESS;
}
void SystemInit(void) {
#ifndef NX_G474_MODEL
    /* Called before initialized RAM exists: only CPU register accesses. */
    SCB->CPACR |= UINT32_C(0x00f00000);
    nx_arch_dsb();
    nx_arch_isb();
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(3U);
#endif
}
nx_result_t nx_stm32g474_validate_identity(void) {
    if ((uint32_t)NX_G474_FLASH_KIB() != 512U ||
        ((uint32_t)NX_G474_DEVICE_ID() & 0xfffU) != 0x469U ||
        ((uint32_t)NX_G474_FLASH_OPTIONS() & FLASH_OPTR_DBANK) == 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
/** \brief Inspect cold prerequisites before any clock or Flash writes. */
nx_result_t nx_stm32g474_system_preflight(const nx_stm32g474_system_t* s) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->rcc == NULL || s->flash == NULL || s->power == NULL ||
        s->timer == NULL)
        return NX_ERROR_INVALID;
    if (s->started || s->remaining_effects != 0U)
        return NX_ERROR_BUSY;
    uint32_t cr = NX_G474_READ(&s->rcc->CR);
    uint32_t cfgr = NX_G474_READ(&s->rcc->CFGR);
    if ((cr & (RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_CSSON | RCC_CR_PLLON |
               RCC_CR_PLLRDY)) != 0U ||
        (cr & RCC_CR_HSIRDY) == 0U ||
        (cfgr & (RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                 RCC_CFGR_PPRE2)) != (RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI) ||
        (NX_G474_READ(&s->rcc->APB1ENR1) & RCC_APB1ENR1_TIM2EN) != 0U ||
        NX_G474_READ(&s->timer->CR1) != 0U ||
        NX_G474_READ(&s->timer->DIER) != 0U ||
        (NX_G474_READ(&s->flash->SR) & FLASH_SR_BSY) != 0U ||
        !NX_G474_IRQ_AVAILABLE(TIM2_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_stm32g474_start_result_t nx_stm32g474_system_start(nx_stm32g474_system_t* s,
                                                      uint32_t limit) {
    nx_stm32g474_start_result_t r = {NX_SUCCESS, NX_SUCCESS, 0U};
    r.primary = nx_stm32g474_system_preflight(s);
    if (r.primary != NX_SUCCESS)
        return r;
    if (limit == 0U) {
        r.primary = NX_ERROR_INVALID;
        return r;
    }
    uint32_t cr = NX_G474_READ(&s->rcc->CR);
    uint32_t cfgr = NX_G474_READ(&s->rcc->CFGR);
    r.primary = nx_stm32g474_validate_identity();
    if (r.primary != NX_SUCCESS)
        return r;
    s->timer_error = NX_SUCCESS;
    s->saved_pll = NX_G474_READ(&s->rcc->PLLCFGR);
    s->saved_flash = NX_G474_READ(&s->flash->ACR);
    s->power_snapshotted = false;
    s->owned_power_clock =
        (NX_G474_READ(&s->rcc->APB1ENR1) & RCC_APB1ENR1_PWREN) == 0U;
    s->reset_cause = NX_G474_READ(&s->rcc->CSR);
    s->remaining_effects = NX_STM32G474_EFFECT_CLOCK;
    if (!write_checked(&s->rcc->APB1ENR1,
                       NX_G474_READ(&s->rcc->APB1ENR1) | RCC_APB1ENR1_PWREN,
                       RCC_APB1ENR1_PWREN)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* APB clock readback and barrier precede the first PWR snapshot. */
    s->saved_power = NX_G474_READ(&s->power->CR1);
    s->saved_boost = NX_G474_READ(&s->power->CR5);
    s->power_snapshotted = true;
    if (!write_checked(&s->power->CR1,
                       (s->saved_power & ~(uint32_t)PWR_CR1_VOS) |
                           PWR_CR1_VOS_0,
                       PWR_CR1_VOS)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    if (!wait_bits(s, &s->power->SR2, PWR_SR2_VOSF, 0U, limit)) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    if (!write_checked(&s->rcc->CFGR, cfgr | RCC_CFGR_HPRE_DIV2,
                       RCC_CFGR_HPRE) ||
        !write_checked(&s->power->CR5,
                       s->saved_boost & ~(uint32_t)PWR_CR5_R1MODE,
                       PWR_CR5_R1MODE) ||
        !write_checked(&s->flash->ACR,
                       (s->saved_flash & ~(uint32_t)FLASH_ACR_LATENCY) |
                           FLASH_ACR_LATENCY_4WS | FLASH_ACR_ICEN |
                           FLASH_ACR_DCEN | FLASH_ACR_PRFTEN,
                       FLASH_ACR_LATENCY | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                           FLASH_ACR_PRFTEN)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    NX_G474_WRITE(&s->rcc->CR, cr | RCC_CR_HSEON);
    if (!wait_bits(s, &s->rcc->CR, RCC_CR_HSERDY, RCC_CR_HSERDY, limit)) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    uint32_t pll = RCC_PLLCFGR_PLLSRC_HSE | (5U << RCC_PLLCFGR_PLLM_Pos) |
                   (85U << RCC_PLLCFGR_PLLN_Pos) | RCC_PLLCFGR_PLLREN;
    if (!write_checked(&s->rcc->PLLCFGR, pll, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    NX_G474_WRITE(&s->rcc->CR, NX_G474_READ(&s->rcc->CR) | RCC_CR_PLLON);
    if (!wait_bits(s, &s->rcc->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY, limit)) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    NX_G474_WRITE(&s->rcc->CFGR,
                  (NX_G474_READ(&s->rcc->CFGR) & ~(uint32_t)RCC_CFGR_SW) |
                      RCC_CFGR_SW_PLL);
    if (!wait_bits(s, &s->rcc->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL, limit)) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    NX_G474_BOOST_DELAY(s);
    if (!write_checked(&s->rcc->CFGR,
                       NX_G474_READ(&s->rcc->CFGR) & ~(uint32_t)RCC_CFGR_HPRE,
                       RCC_CFGR_HPRE)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    s->core_hz = 170000000U;
    s->remaining_effects |= NX_STM32G474_EFFECT_TIMER;
    if (!write_checked(&s->rcc->APB1ENR1,
                       NX_G474_READ(&s->rcc->APB1ENR1) | RCC_APB1ENR1_TIM2EN,
                       RCC_APB1ENR1_TIM2EN)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    NX_G474_WRITE(&s->rcc->APB1RSTR1,
                  NX_G474_READ(&s->rcc->APB1RSTR1) | RCC_APB1RSTR1_TIM2RST);
    NX_G474_WRITE(&s->rcc->APB1RSTR1, NX_G474_READ(&s->rcc->APB1RSTR1) &
                                          ~(uint32_t)RCC_APB1RSTR1_TIM2RST);
    bool valid = write_checked(&s->timer->PSC, 169U, UINT32_MAX) &&
                 write_checked(&s->timer->ARR, UINT32_MAX, UINT32_MAX) &&
                 write_checked(&s->timer->SR, 0U, UINT32_MAX);
    if (!valid) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* A fresh UIF transition proves UG transferred the timer shadow fields.
     * Clear and observe the zero counter before publishing any IRQ source. */
    NX_G474_WRITE(&s->timer->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if (!wait_bits(s, &s->timer->SR, TIM_SR_UIF, TIM_SR_UIF, limit)) {
        r.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    if (!write_checked(&s->timer->SR, 0U, UINT32_MAX) ||
        !write_checked(&s->timer->CNT, 0U, UINT32_MAX)) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    s->overflow_us = 0U;
    valid = write_checked(&s->timer->DIER, TIM_DIER_UIE, UINT32_MAX) && valid;
    valid = write_checked(&s->timer->CR1, TIM_CR1_CEN, UINT32_MAX) && valid;
    if (!valid) {
        r.primary = NX_ERROR_IO;
        goto rollback;
    }
    r.primary = NX_G474_IRQ_CONTROL(TIM2_IRQn, true, 0U);
    if (r.primary != NX_SUCCESS)
        goto rollback;
    s->started = true;
    return r;
rollback:
    r.cleanup = nx_stm32g474_system_stop(s, limit);
    r.remaining_effects = s->remaining_effects;
    return r;
}
nx_result_t nx_stm32g474_system_stop(nx_stm32g474_system_t* s, uint32_t limit) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL || s->rcc == NULL || s->flash == NULL || s->power == NULL ||
        s->timer == NULL || limit == 0U)
        return NX_ERROR_INVALID;
    if ((s->remaining_effects & NX_STM32G474_EFFECT_TIMER) != 0U) {
        s->started = false;
        nx_result_t irq = NX_G474_IRQ_CONTROL(TIM2_IRQn, false, 0U);
        if (irq != NX_SUCCESS)
            return irq;
        if (!write_checked(&s->timer->DIER, 0U, UINT32_MAX) ||
            !write_checked(&s->timer->CR1, 0U, UINT32_MAX))
            return NX_ERROR_IO;
        NX_G474_WRITE(&s->timer->SR, 0U);
        if (!write_checked(&s->rcc->APB1ENR1,
                           NX_G474_READ(&s->rcc->APB1ENR1) &
                               ~(uint32_t)RCC_APB1ENR1_TIM2EN,
                           RCC_APB1ENR1_TIM2EN))
            return NX_ERROR_IO;
        s->started = false;
        s->remaining_effects &= ~NX_STM32G474_EFFECT_TIMER;
    }
    if ((s->remaining_effects & NX_STM32G474_EFFECT_CLOCK) != 0U)
        return restore_hsi(s, limit);
    return NX_SUCCESS;
}
void nx_stm32g474_system_timer_irq(nx_stm32g474_system_t* s) {
    if (s != NULL &&
        (s->started ||
         (s->remaining_effects & NX_STM32G474_EFFECT_TIMER) != 0U) &&
        (NX_G474_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        if (s->timer_error != NX_SUCCESS)
            return;
        NX_G474_WRITE(&s->timer->SR, ~(uint32_t)TIM_SR_UIF);
        nx_arch_dsb();
        if ((NX_G474_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
            s->started = false;
            s->timer_error = NX_ERROR_IO;
            /* Attempt each withdrawal once, even when an earlier readback
             * fails. The current handler's ACTIVE bit is not a foreign owner.
             */
            bool source_off = write_checked(&s->timer->DIER, 0U, UINT32_MAX);
            bool counter_off = write_checked(&s->timer->CR1, 0U, TIM_CR1_CEN);
            nx_result_t vector_off = NX_G474_IRQ_CONTROL(TIM2_IRQn, false, 0U);
            if (!source_off || !counter_off || vector_off != NX_SUCCESS)
                timer_park();
            return;
        }
        if (s->started)
            s->overflow_us += (UINT64_C(1) << 32U);
    }
}
uint64_t nx_stm32g474_system_now(nx_stm32g474_system_t* s) {
    if (s == NULL || !s->started || !nx_arch_is_privileged())
        return 0U;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!s->started) {
        nx_arch_irq_restore(guard);
        return 0U;
    }
    uint64_t high = s->overflow_us;
    uint32_t count = NX_G474_READ(&s->timer->CNT);
    if ((NX_G474_READ(&s->timer->SR) & TIM_SR_UIF) != 0U) {
        high += (UINT64_C(1) << 32U);
        count = NX_G474_READ(&s->timer->CNT);
    }
    nx_arch_irq_restore(guard);
    return high + count;
}
nx_time_us_t nx_time_now_us(void) {
    return nx_stm32g474_system_now(&g_nx_stm32g474_system);
}
void TIM2_IRQHandler(void) {
    nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system);
}
