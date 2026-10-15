/**
 * \file            system.c
 * \brief           STM32F407 precise clock startup, rollback and timer timebase
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "stm32f407_system.h"

#ifndef NEXUS_STM32_MODEL
nx_stm32_system_t g_nx_stm32_system = {
    .rcc = RCC, .flash = FLASH, .power = PWR, .timer = TIM2};
#endif

extern nx_stm32_system_t g_nx_stm32_system;

/** \brief Read the unique fixed provider clock without an OS dependency. */
nx_time_us_t nx_time_now_us(void) {
    return nx_stm32_system_now(&g_nx_stm32_system);
}

/** \brief Forward the explicitly reserved timer vector to the static context.
 */
void TIM2_IRQHandler(void) {
    nx_stm32_system_timer_irq(&g_nx_stm32_system);
}

#ifndef NX_STM32_POLL
#define NX_STM32_POLL(system) ((void)(system))
#endif

#define NX_STM32_EFFECT_CLOCK 1U
#define NX_STM32_EFFECT_TIMER 2U

/** \brief Wait for one hardware condition within an explicit boot poll bound.
 */
static bool wait_bits(nx_stm32_system_t* system, volatile uint32_t* reg,
                      uint32_t mask, uint32_t value, uint32_t limit) {
    while (limit-- != 0U) {
        NX_STM32_POLL(system);
        if ((*reg & mask) == value) {
            return true;
        }
    }
    return false;
}

/** \brief Restore HSI only after its ready and source status are observed. */
static int restore_hsi(nx_stm32_system_t* system, uint32_t limit) {
    system->rcc->CR |= RCC_CR_HSION;
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY,
                   limit)) {
        return -1;
    }
    system->rcc->CFGR &= ~(uint32_t)RCC_CFGR_SW;
    if (!wait_bits(system, &system->rcc->CFGR, RCC_CFGR_SWS, 0U, limit)) {
        return -1;
    }
    system->rcc->CR &= ~(uint32_t)(RCC_CR_PLLON | RCC_CR_HSEON);
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_PLLRDY, 0U, limit)) {
        return -1;
    }
    system->rcc->CFGR = 0U;
    system->flash->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    system->core_hz = 16000000U;
    system->remaining_effects = 0U;
    return 0;
}

/** \brief Initialize CPU registers without touching not-yet-initialized RAM. */
void SystemInit(void) {
#ifndef NEXUS_STM32_MODEL
    SCB->CPACR |= (0xFU << 20U);
    __DSB();
    __ISB();
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0U;
    RCC->CR &= ~(uint32_t)(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->PLLCFGR = 0x24003010U;
    RCC->CIR = 0U;
    SCB->VTOR = FLASH_BASE;
    NVIC_SetPriorityGrouping(3U);
#endif
}

/** \brief Establish the one supported fixed clock profile and TIM2 source. */
nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t* system,
                                              uint32_t poll_limit) {
    nx_stm32_start_result_t result = {0, 0, 0U};
    if (system == NULL || system->rcc == NULL || system->flash == NULL ||
        system->power == NULL || system->timer == NULL || poll_limit == 0U ||
        system->started || system->remaining_effects != 0U) {
        result.primary = -2;
        return result;
    }
    system->reset_cause = system->rcc->CSR;
    system->rcc->APB1ENR |= RCC_APB1ENR_PWREN;
    system->power->CR |= PWR_CR_VOS;
    system->flash->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_ICEN |
                         FLASH_ACR_DCEN | FLASH_ACR_PRFTEN;
    system->remaining_effects = NX_STM32_EFFECT_CLOCK;
    system->rcc->CR |= RCC_CR_HSEON;
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_HSERDY, RCC_CR_HSERDY,
                   poll_limit)) {
        result.primary = -3;
        goto rollback;
    }
    system->rcc->PLLCFGR =
        8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
    system->rcc->CFGR = RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
    system->rcc->CR |= RCC_CR_PLLON;
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY,
                   poll_limit)) {
        result.primary = -4;
        goto rollback;
    }
    system->rcc->CFGR |= RCC_CFGR_SW_PLL;
    if (!wait_bits(system, &system->rcc->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL,
                   poll_limit)) {
        result.primary = -5;
        goto rollback;
    }
    system->rcc->APB1ENR |= RCC_APB1ENR_TIM2EN;
    system->rcc->APB1RSTR |= RCC_APB1RSTR_TIM2RST;
    system->rcc->APB1RSTR &= ~(uint32_t)RCC_APB1RSTR_TIM2RST;
    system->timer->CR1 = 0U;
    system->timer->PSC = 83U;
    system->timer->ARR = UINT32_MAX;
    system->timer->EGR = TIM_EGR_UG;
    system->timer->SR = 0U;
    system->timer->CNT = 0U;
    system->overflow_us = 0U;
    system->timer->DIER = TIM_DIER_UIE;
#ifndef NEXUS_STM32_MODEL
    NVIC_SetPriority(TIM2_IRQn, 0U);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);
#endif
    system->timer->CR1 = TIM_CR1_CEN;
    system->core_hz = 168000000U;
    system->remaining_effects |= NX_STM32_EFFECT_TIMER;
    system->started = true;
    return result;
rollback:
    result.cleanup = restore_hsi(system, poll_limit);
    result.remaining_effects = system->remaining_effects;
    return result;
}

/** \brief Stop timer access before attempting clock cleanup. */
int nx_stm32_system_stop(nx_stm32_system_t* system, uint32_t poll_limit) {
    if (system == NULL || system->rcc == NULL || system->timer == NULL ||
        system->flash == NULL || poll_limit == 0U) {
        return -2;
    }
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(TIM2_IRQn);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
#endif
    system->timer->DIER = 0U;
    system->timer->CR1 = 0U;
    system->rcc->APB1ENR &= ~(uint32_t)RCC_APB1ENR_TIM2EN;
    system->started = false;
    system->remaining_effects &= ~NX_STM32_EFFECT_TIMER;
    return restore_hsi(system, poll_limit);
}

/** \brief Count a hardware overflow once, clearing only the update source. */
void nx_stm32_system_timer_irq(nx_stm32_system_t* system) {
    if (system != NULL && system->started &&
        (system->timer->SR & TIM_SR_UIF) != 0U) {
        system->timer->SR = ~(uint32_t)TIM_SR_UIF;
        system->overflow_us += (UINT64_C(1) << 32U);
    }
}

/** \brief Fold pending overflow into one interrupt-protected timer snapshot. */
uint64_t nx_stm32_system_now(nx_stm32_system_t* system) {
    if (system == NULL || !system->started) {
        return 0U;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    uint64_t high = system->overflow_us;
    uint32_t count = system->timer->CNT;
    if ((system->timer->SR & TIM_SR_UIF) != 0U) {
        high += (UINT64_C(1) << 32U);
        count = system->timer->CNT;
    }
    nx_arch_irq_restore(mask);
    return high + count;
}

/** \brief Validate observed density without pretending DEV_ID encodes package.
 */
nx_result_t nx_stm32_validate_identity(uint32_t expected_flash_bytes) {
    if (expected_flash_bytes != 524288U && expected_flash_bytes != 1048576U) {
        return NX_ERROR_INVALID;
    }
#ifndef NX_STM32_FLASH_KIB
#define NX_STM32_FLASH_KIB() (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_STM32_DEVICE_ID() (DBGMCU->IDCODE)
#endif
    uint32_t observed_bytes = (uint32_t)NX_STM32_FLASH_KIB() * 1024U;
    if (observed_bytes != expected_flash_bytes ||
        (NX_STM32_DEVICE_ID() & 0xFFFU) != 0x413U) {
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
