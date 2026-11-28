/**
 * \file            platform.c
 * \brief           Reset HSI8, cold TIM2 lease and 16-bit monotonic timebase.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "system/platform.h"
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "private/hardware.h"

#ifndef NX_STM32F1_FLASH_KIB
#define NX_STM32F1_FLASH_KIB() (*(volatile const uint16_t*)FLASHSIZE_BASE)
#endif
#define TIMER_PERIOD_US UINT64_C(65536)

enum {
    TIMER_GATE = 1U,
    TIMER_REGISTERS = 2U,
    TIMER_VECTOR = 4U,
    TIMER_SYNC_POLLS = 64U
};

static uint64_t elapsed_us;
static uint8_t effects;
static uint8_t priority_before;
static bool active;

uint32_t SystemCoreClock = 8000000U;

/** \brief Prove the fixed reset clock without changing foreign sources. */
static bool hsi8_ready(void) {
    const uint32_t active_clock = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSITRIM;
    const uint32_t hsi8 =
        RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
    const uint32_t foreign_sources = RCC_CR_HSEON | RCC_CR_HSERDY |
                                     RCC_CR_HSEBYP | RCC_CR_CSSON |
                                     RCC_CR_PLLON | RCC_CR_PLLRDY;
    const uint32_t bus_dividers = RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE |
                                  RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2;
    return (RCC->CR & (active_clock | foreign_sources)) == hsi8 &&
           (RCC->CFGR & bus_dividers) == 0U && (FLASH->SR & FLASH_SR_BSY) == 0U;
}

/** \brief Reject a retained timer configuration before generating any event. */
static bool timer_idle(void) {
    return TIM2->CR1 == 0U && TIM2->CR2 == 0U && TIM2->SMCR == 0U &&
           TIM2->DIER == 0U && TIM2->CCER == 0U && TIM2->CCMR1 == 0U &&
           TIM2->CCMR2 == 0U && TIM2->DCR == 0U && TIM2->SR == 0U &&
           TIM2->CNT == 0U && TIM2->PSC == 0U && TIM2->ARR == UINT16_MAX &&
           TIM2->CCR1 == 0U && TIM2->CCR2 == 0U && TIM2->CCR3 == 0U &&
           TIM2->CCR4 == 0U;
}

/** \brief Finish setup by loading the prescaler while the counter is stopped.
 */
static nx_result_t start_timer(void) {
    if (!nx_stm32f1_write_masked(&TIM2->PSC, UINT16_MAX, 7U) ||
        !nx_stm32f1_write_masked(&TIM2->CR2, TIM_CR2_MMS, TIM_CR2_MMS_0))
        return NX_ERROR_IO;

    /* MMS reset forwards UG as TRGO. Use the stopped CEN signal instead
     * while loading PSC, so this setup cannot release a borrowed trigger. */
    NX_STM32F1_WRITE(&TIM2->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    unsigned spins = TIMER_SYNC_POLLS;
    while ((TIM2->SR & TIM_SR_UIF) == 0U) {
        if (--spins == 0U)
            return NX_ERROR_TIMEOUT;
    }
    NX_STM32F1_WRITE(&TIM2->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((TIM2->SR & TIM_SR_UIF) != 0U ||
        !nx_stm32f1_write_masked(&TIM2->CR2, TIM_CR2_MMS, 0U))
        return NX_ERROR_IO;

    /* Publish the epoch and activate its sources within the same mask. */
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    priority_before = (uint8_t)NVIC_GetPriority(TIM2_IRQn);
    effects |= TIMER_VECTOR;
    NVIC_SetPriority(TIM2_IRQn, 0U);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);
    nx_arch_dsb();
    nx_arch_isb();
    active = NVIC_GetEnableIRQ(TIM2_IRQn) != 0U &&
             NVIC_GetPendingIRQ(TIM2_IRQn) == 0U &&
             NVIC_GetPriority(TIM2_IRQn) == 0U &&
             nx_stm32f1_write_masked(&TIM2->DIER, TIM_DIER_UIE, TIM_DIER_UIE) &&
             nx_stm32f1_write_masked(&TIM2->CR1, TIM_CR1_CEN, TIM_CR1_CEN);
    const nx_result_t result = active ? NX_SUCCESS : NX_ERROR_IO;
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Withdraw the hardware IRQ before returning its clock and registers.
 */
static nx_result_t withdraw_timer(void) {
    if (!nx_stm32f1_write_masked(&TIM2->DIER, TIM_DIER_UIE, 0U) ||
        !nx_stm32f1_write_masked(&TIM2->CR1, TIM_CR1_CEN, 0U))
        return NX_ERROR_IO;
    if ((effects & TIMER_VECTOR) != 0U) {
        NVIC_DisableIRQ(TIM2_IRQn);
        NVIC_ClearPendingIRQ(TIM2_IRQn);
        nx_arch_dsb();
        nx_arch_isb();
        if (NVIC_GetEnableIRQ(TIM2_IRQn) != 0U ||
            NVIC_GetPendingIRQ(TIM2_IRQn) != 0U ||
            NVIC_GetActive(TIM2_IRQn) != 0U)
            return NX_ERROR_IO;
        NVIC_SetPriority(TIM2_IRQn, priority_before);
        nx_arch_dsb();
        if (NVIC_GetPriority(TIM2_IRQn) != priority_before)
            return NX_ERROR_IO;
        effects &= (uint8_t)~TIMER_VECTOR;
    }
    /* A register lease is only admitted from reset state. Reset it before
     * release, so a subsequent acquisition can prove the same baseline. */
    const bool asserted = nx_stm32f1_write_masked(
        &RCC->APB1RSTR, RCC_APB1RSTR_TIM2RST, RCC_APB1RSTR_TIM2RST);
    if (!nx_stm32f1_write_masked(&RCC->APB1RSTR, RCC_APB1RSTR_TIM2RST, 0U) ||
        !asserted || !timer_idle())
        return NX_ERROR_IO;
    effects &= (uint8_t)~TIMER_REGISTERS;
    return NX_SUCCESS;
}

/** \brief Release the actual retained effects, including failed startup. */
static nx_result_t release_system(void) {
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    active = false;
    nx_arch_irq_restore(saved);
    if ((effects & TIMER_REGISTERS) != 0U) {
        nx_result_t result = withdraw_timer();
        if (result != NX_SUCCESS)
            return result;
    }
    if ((effects & TIMER_GATE) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM2EN, 0U))
            return NX_ERROR_IO;
        effects &= (uint8_t)~TIMER_GATE;
    }
    return NX_SUCCESS;
}

nx_platform_start_result_t
nx_stm32f1_platform_system_start(const nx_stm32f1_platform_config_t* config) {
    nx_platform_start_result_t result = {NX_SUCCESS, NX_SUCCESS, effects};
    if (!nx_stm32f1_cold_context()) {
        result.primary = NX_ERROR_CONTEXT;
        return result;
    }
    if (config == NULL || config->flash_bytes == 0U) {
        result.primary = NX_ERROR_INVALID;
        return result;
    }
    if (effects != 0U) {
        result.primary = NX_ERROR_STATE;
        return result;
    }
    /* ES096 2.2.2 excludes ordinary DBGMCU reads. The size halfword is a
     * density check only. Physical part and cut belonging remain external. */
    if ((uint32_t)NX_STM32F1_FLASH_KIB() * 1024U != config->flash_bytes) {
        result.primary = NX_ERROR_IO;
        return result;
    }
    if (!hsi8_ready() || (RCC->APB1ENR & RCC_APB1ENR_TIM2EN) != 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_TIM2RST) != 0U ||
        NVIC_GetEnableIRQ(TIM2_IRQn) != 0U ||
        NVIC_GetPendingIRQ(TIM2_IRQn) != 0U ||
        NVIC_GetActive(TIM2_IRQn) != 0U) {
        result.primary = NX_ERROR_BUSY;
        return result;
    }
    effects = TIMER_GATE;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_TIM2EN,
                                 RCC_APB1ENR_TIM2EN)) {
        result.primary = NX_ERROR_IO;
    } else if (!timer_idle()) {
        result.primary = NX_ERROR_BUSY;
    } else {
        effects |= TIMER_REGISTERS;
        elapsed_us = 0U;
        result.primary = start_timer();
    }
    if (result.primary != NX_SUCCESS) {
        result.cleanup = release_system();
        result.remaining_effects = effects;
    }
    return result;
}

nx_result_t
nx_stm32f1_platform_no_resources(const nx_stm32f1_platform_config_t* config) {
    return config != NULL ? NX_SUCCESS : NX_ERROR_INVALID;
}

nx_result_t
nx_stm32f1_platform_system_release(const nx_stm32f1_platform_config_t* config,
                                   bool started) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (config == NULL)
        return NX_ERROR_INVALID;
    return !started && effects != 0U ? NX_ERROR_STATE : release_system();
}

bool nx_stm32f1_system_live(void) {
    if (!nx_arch_is_privileged())
        return false;
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    const bool live = active;
    nx_arch_irq_restore(saved);
    return live;
}

nx_time_us_t nx_time_now_us(void) {
    if (!nx_arch_is_privileged())
        return 0U;
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_time_us_t now = 0U;
    if (active) {
        now = elapsed_us + (TIM2->CNT & UINT16_MAX);
        /* Counter-before-flag ordering permits a wrap between reads. Observe
         * CNT a second time with the pending wrap folded into the epoch. */
        if ((TIM2->SR & TIM_SR_UIF) != 0U) {
            now = elapsed_us + TIMER_PERIOD_US + (TIM2->CNT & UINT16_MAX);
        }
    }
    nx_arch_irq_restore(saved);
    return now;
}

void TIM2_IRQHandler(void) {
    if ((effects & TIMER_VECTOR) == 0U || (TIM2->SR & TIM_SR_UIF) == 0U)
        return;
    NX_STM32F1_WRITE(&TIM2->SR, ~(uint32_t)TIM_SR_UIF);
    nx_arch_dsb();
    if ((TIM2->SR & TIM_SR_UIF) != 0U ||
        (active && elapsed_us > UINT64_MAX - 3U * TIMER_PERIOD_US)) {
        /* A lost epoch cannot satisfy already accepted finite deadlines. */
        (void)nx_arch_irq_save();
        active = false;
        NVIC_DisableIRQ(TIM2_IRQn);
        nx_arch_dsb();
        nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
        for (;;) {
        }
    }
    if (active)
        elapsed_us += TIMER_PERIOD_US;
    nx_arch_dsb();
}
