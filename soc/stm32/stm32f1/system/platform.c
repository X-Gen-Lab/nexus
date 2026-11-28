/**
 * \file            platform.c
 * \brief           Cold F1 clock ownership and 16-bit monotonic timebase.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "system/platform.h"
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "nexus/io/watchdog.h"
#include "private/hardware.h"
#include "system/clock_profile.h"

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
static uint8_t boot_reset_causes;
static bool reset_captured;

#if defined(NX_STM32F1_HSE72)
_Static_assert(
    NEXUS_CLOCK_APB1_HZ == 36000000U && NEXUS_CLOCK_APB2_HZ == 72000000U &&
        NEXUS_CLOCK_APB1_TIMER_HZ == 72000000U &&
        NEXUS_CLOCK_FLASH_WAIT_STATES == 2U,
    "F1 PLL72 bus timing and Flash latency must match the clock owner");
#define TIMER_PRESCALER 71U
#else
#define TIMER_PRESCALER 7U
#endif

uint32_t SystemCoreClock = 8000000U;

static void capture_reset_cause(void) {
    if (reset_captured)
        return;
    const uint32_t flags = RCC->CSR;
    /* F1 PORRSTF is the combined POR/PDR flag. There is no separate BOR bit. */
    boot_reset_causes =
        (uint8_t)(((flags & RCC_CSR_PORRSTF) ? NX_RESET_POWER_ON : 0U) |
                  ((flags & RCC_CSR_PINRSTF) ? NX_RESET_PIN : 0U) |
                  ((flags & RCC_CSR_SFTRSTF) ? NX_RESET_SOFTWARE : 0U) |
                  ((flags & RCC_CSR_IWDGRSTF) ? NX_RESET_IWDG : 0U) |
                  ((flags & RCC_CSR_WWDGRSTF) ? NX_RESET_WWDG : 0U) |
                  ((flags & RCC_CSR_LPWRRSTF) ? NX_RESET_LOW_POWER : 0U));
    reset_captured = true;
}

uint32_t nx_reset_cause(void) {
    return boot_reset_causes;
}

/** \brief Prove the fixed reset clock without changing foreign sources. */
static bool hsi8_ready(void) {
    const uint32_t clock_sources = RCC->CR;
#if defined(RCC_CR_PLL2ON)
    const uint32_t auxiliary =
        RCC_CR_PLL2ON | RCC_CR_PLL2RDY | RCC_CR_PLL3ON | RCC_CR_PLL3RDY;
    if ((clock_sources & auxiliary) != 0U)
        return false;
#endif
#if defined(RCC_CFGR2_PREDIV1)
    if (RCC->CFGR2 != 0U)
        return false;
#endif
    const uint32_t active_clock = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSITRIM;
    const uint32_t hsi8 =
        RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
    const uint32_t foreign_sources = RCC_CR_HSEON | RCC_CR_HSERDY |
                                     RCC_CR_HSEBYP | RCC_CR_CSSON |
                                     RCC_CR_PLLON | RCC_CR_PLLRDY;
    const uint32_t bus_dividers = RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE |
                                  RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2;
    /* PM0075 admits half-cycle access only below 8 MHz. Require the
     * selected zero-wait profile without changing an inherited Flash mode. */
#if defined(FLASH_ACR_LATENCY)
    const uint32_t flash_timing = FLASH_ACR_HLFCYA | FLASH_ACR_LATENCY;
#else
    const uint32_t flash_timing = FLASH_ACR_HLFCYA;
#endif
    return (clock_sources & (active_clock | foreign_sources)) == hsi8 &&
           (RCC->CFGR & bus_dividers) == 0U &&
           (FLASH->ACR & flash_timing) == 0U &&
           (FLASH->SR & FLASH_SR_BSY) == 0U;
}

#if defined(NX_STM32F1_HSE72)
enum { CLOCK_CONFIG = 8U, CLOCK_READY_POLLS = 80000U };

#if defined(NX_STM32F1_CONNECTIVITY_PLL2)
_Static_assert(NEXUS_CLOCK_PLL48_HZ == 48000000U,
               "The connectivity PLL VCO divided by three supplies 48 MHz");
#define PLL_FIELDS                                                             \
    (RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL | RCC_CFGR_OTGFSPRE)
#define PLL2_FIELDS                                                            \
    (RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |               \
     RCC_CFGR2_PREDIV1SRC)
#define PLL2_SETTINGS                                                          \
    (RCC_CFGR2_PREDIV1_DIV5 | RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |    \
     RCC_CFGR2_PREDIV1SRC_PLL2)
#else
#define PLL_FIELDS (RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL)
#endif
#define BUS_FIELDS                                                             \
    (RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE)
#define PLL72_SETTINGS                                                         \
    (RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9 | RCC_CFGR_PPRE1_DIV2 |               \
     RCC_CFGR_ADCPRE_DIV6)

static bool clock_idle(void) {
    uint32_t clock_irqs = RCC_CIR_LSIRDYIE | RCC_CIR_LSERDYIE |
                          RCC_CIR_HSIRDYIE | RCC_CIR_HSERDYIE |
                          RCC_CIR_PLLRDYIE | RCC_CIR_CSSF;
#if defined(NX_STM32F1_CONNECTIVITY_PLL2)
    if (RCC->AHBRSTR != 0U)
        return false;
    clock_irqs |= RCC_CIR_PLL2RDYIE | RCC_CIR_PLL3RDYIE;
#endif
    const uint32_t flash_state = FLASH_ACR_LATENCY | FLASH_ACR_HLFCYA |
                                 FLASH_ACR_PRFTBE | FLASH_ACR_PRFTBS;
    return RCC->AHBENR == (RCC_AHBENR_FLITFEN | RCC_AHBENR_SRAMEN) &&
           RCC->APB1ENR == 0U && RCC->APB2ENR == 0U && RCC->APB1RSTR == 0U &&
           RCC->APB2RSTR == 0U &&
           (RCC->CFGR & (PLL_FIELDS | BUS_FIELDS | RCC_CFGR_MCO)) == 0U &&
           (RCC->CIR & clock_irqs) == 0U &&
           (FLASH->ACR & flash_state) == (FLASH_ACR_PRFTBE | FLASH_ACR_PRFTBS);
}

/** \brief Wait for a hardware acknowledgement before using its clock.
 * \note This bounded iteration budget does not consume a timer epoch.
 */
static bool clock_wait(volatile const uint32_t* reg, uint32_t mask,
                       uint32_t expected) {
    unsigned remaining = CLOCK_READY_POLLS;
    while ((*reg & mask) != expected) {
        if (--remaining == 0U)
            return false;
    }
    return true;
}

/** \brief Switch sources only after the dividers and Flash wait states are
 * safe. */
static nx_result_t clock_start(void) {
    effects |= CLOCK_CONFIG;
    /* Select the reviewed HSE input while HSE is off. Keep HSI enabled. */
    const uint32_t bypass = NEXUS_CLOCK_HSE_BYPASS ? RCC_CR_HSEBYP : 0U;
    if (!nx_stm32f1_write_masked(&RCC->CR, RCC_CR_HSEBYP, bypass) ||
        !nx_stm32f1_write_masked(&RCC->CR, RCC_CR_HSEON, RCC_CR_HSEON))
        return NX_ERROR_IO;
    if (!clock_wait(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY))
        return NX_ERROR_TIMEOUT;

    if (!nx_stm32f1_write_masked(&RCC->CR, RCC_CR_CSSON, RCC_CR_CSSON) ||
        !nx_stm32f1_write_masked(&FLASH->ACR, FLASH_ACR_LATENCY, 2U) ||
        !nx_stm32f1_write_masked(&RCC->CFGR, PLL_FIELDS | BUS_FIELDS,
                                 PLL72_SETTINGS))
        return NX_ERROR_IO;
#if defined(NX_STM32F1_CONNECTIVITY_PLL2)
    if (!nx_stm32f1_write_masked(&RCC->CFGR2, PLL2_FIELDS, PLL2_SETTINGS) ||
        !nx_stm32f1_write_masked(&RCC->CR, RCC_CR_PLL2ON, RCC_CR_PLL2ON))
        return NX_ERROR_IO;
    if (!clock_wait(&RCC->CR, RCC_CR_PLL2RDY, RCC_CR_PLL2RDY))
        return NX_ERROR_TIMEOUT;
#endif
    if (!nx_stm32f1_write_masked(&RCC->CR, RCC_CR_PLLON, RCC_CR_PLLON))
        return NX_ERROR_IO;
    if (!clock_wait(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY))
        return NX_ERROR_TIMEOUT;
    if (!nx_stm32f1_write_masked(&RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL))
        return NX_ERROR_IO;
    if (!clock_wait(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL))
        return NX_ERROR_TIMEOUT;
    nx_arch_isb();
    SystemCoreClock = 72000000U;
    return NX_SUCCESS;
}

/** \brief Restore the proven HSI clock before retiring the high-speed path. */
static nx_result_t clock_stop(void) {
#if defined(NX_STM32F1_CONNECTIVITY_PLL2)
    if (RCC->AHBENR != (RCC_AHBENR_FLITFEN | RCC_AHBENR_SRAMEN) ||
        RCC->AHBRSTR != 0U ||
        (RCC->CR & (RCC_CR_PLL3ON | RCC_CR_PLL3RDY)) != 0U)
        return NX_ERROR_BUSY;
#endif
    const uint32_t hsi = RCC_CR_HSION | RCC_CR_HSIRDY;
    if ((RCC->CR & hsi) != hsi)
        return NX_ERROR_IO;
    if (!nx_stm32f1_write_masked(&RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_HSI))
        return NX_ERROR_IO;
    if (!clock_wait(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI))
        return NX_ERROR_TIMEOUT;
    nx_arch_isb();
    SystemCoreClock = 8000000U;

    if (!nx_stm32f1_write_masked(&RCC->CR, RCC_CR_CSSON | RCC_CR_PLLON, 0U))
        return NX_ERROR_IO;
    if (!clock_wait(&RCC->CR, RCC_CR_PLLRDY, 0U))
        return NX_ERROR_TIMEOUT;
#if defined(NX_STM32F1_CONNECTIVITY_PLL2)
    /* Disconnect the stopped main PLL from PLL2 before retiring PLL2.
     * HSI drives SYSCLK directly throughout, without starting the PLL on HSI/2.
     */
    if (!nx_stm32f1_write_masked(&RCC->CFGR, RCC_CFGR_PLLSRC, 0U) ||
        !nx_stm32f1_write_masked(&RCC->CR, RCC_CR_PLL2ON, 0U))
        return NX_ERROR_IO;
    if (!clock_wait(&RCC->CR, RCC_CR_PLL2RDY, 0U))
        return NX_ERROR_TIMEOUT;
    if (!nx_stm32f1_write_masked(&RCC->CFGR2, PLL2_FIELDS, 0U))
        return NX_ERROR_IO;
#endif
    if (!nx_stm32f1_write_masked(&RCC->CR, RCC_CR_HSEON, 0U))
        return NX_ERROR_IO;
    if (!clock_wait(&RCC->CR, RCC_CR_HSERDY, 0U))
        return NX_ERROR_TIMEOUT;
    if (!nx_stm32f1_write_masked(&RCC->CR, RCC_CR_HSEBYP, 0U) ||
        !nx_stm32f1_write_masked(&RCC->CFGR, PLL_FIELDS | BUS_FIELDS, 0U) ||
        !nx_stm32f1_write_masked(&FLASH->ACR, FLASH_ACR_LATENCY, 0U) ||
        !hsi8_ready() || !clock_idle())
        return NX_ERROR_IO;

    effects &= (uint8_t)~CLOCK_CONFIG;
    return NX_SUCCESS;
}
#endif

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
    if (!nx_stm32f1_write_masked(&TIM2->PSC, UINT16_MAX, TIMER_PRESCALER) ||
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
#if defined(NX_STM32F1_HSE72)
    if ((effects & CLOCK_CONFIG) != 0U)
        return clock_stop();
#endif
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
    capture_reset_cause();
#if defined(NX_STM32F1_HSE72)
    if (!clock_idle()) {
        result.primary = NX_ERROR_BUSY;
        return result;
    }
    result.primary = clock_start();
    if (result.primary != NX_SUCCESS) {
        result.cleanup = release_system();
        result.remaining_effects = effects;
        return result;
    }
    effects |= TIMER_GATE;
#else
    effects = TIMER_GATE;
#endif
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

#if defined(NX_STM32F1_HSE72)
void NMI_Handler(void);

/** \brief CSS may switch to HSI in hardware; the old epoch cannot resume. */
void NMI_Handler(void) {
    (void)nx_arch_irq_save();
    active = false;
    NX_STM32F1_WRITE(&RCC->CIR, RCC_CIR_CSSC);
    nx_arch_dsb();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
#endif
