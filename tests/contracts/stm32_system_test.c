/**
 * \file            stm32_system_test.c
 * \brief           STM32 clock rollback and timer overflow register model tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_system.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

nx_stm32_system_t g_nx_stm32_system;

static RCC_TypeDef s_rcc;
static FLASH_TypeDef s_flash;
static PWR_TypeDef s_power;
static TIM_TypeDef s_timer;
static unsigned s_fault;
static unsigned s_polls;
static uint32_t s_mask;
static uint16_t s_flash_kib = 512U;
static uint32_t s_device_id = 0x413U;

/** \brief Supply an observed Flash-size register value. */
uint16_t nx_stm32_model_flash_kib(void) {
    return s_flash_kib;
}

/** \brief Supply an observed device group register value. */
uint32_t nx_stm32_model_device_id(void) {
    return s_device_id;
}

/** \brief Supply CPU mask state without claiming hardware IRQ execution. */
nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t old = {s_mask};
    s_mask = 1U;
    return old;
}

/** \brief Restore the incoming model mask exactly. */
void nx_arch_irq_restore(nx_arch_irq_state_t old) {
    s_mask = old.value;
}

/** \brief Advance only hardware phases permitted by the injected fault. */
void nx_stm32_model_poll(void* raw) {
    nx_stm32_system_t* system = raw;
    ++s_polls;
    RCC_TypeDef* rcc = system->rcc;
    if ((rcc->CR & RCC_CR_HSION) != 0U && s_fault != 4U) {
        rcc->CR |= RCC_CR_HSIRDY;
    }
    if ((rcc->CR & RCC_CR_HSEON) != 0U && s_fault != 1U && s_fault != 4U) {
        rcc->CR |= RCC_CR_HSERDY;
    }
    if ((rcc->CR & RCC_CR_PLLON) != 0U && s_fault != 2U) {
        rcc->CR |= RCC_CR_PLLRDY;
    } else if ((rcc->CR & RCC_CR_PLLON) == 0U) {
        rcc->CR &= ~(uint32_t)RCC_CR_PLLRDY;
    }
    if ((rcc->CFGR & RCC_CFGR_SW) == RCC_CFGR_SW_PLL && s_fault != 3U) {
        rcc->CFGR = (rcc->CFGR & ~(uint32_t)RCC_CFGR_SWS) | RCC_CFGR_SWS_PLL;
    } else if ((rcc->CFGR & RCC_CFGR_SW) == 0U && s_fault != 4U) {
        rcc->CFGR &= ~(uint32_t)RCC_CFGR_SWS;
    }
}

/** \brief Reset isolated registers and model state for one scenario. */
static nx_stm32_system_t fresh(unsigned fault) {
    memset(&s_rcc, 0, sizeof(s_rcc));
    memset(&s_flash, 0, sizeof(s_flash));
    memset(&s_power, 0, sizeof(s_power));
    memset(&s_timer, 0, sizeof(s_timer));
    s_fault = fault;
    s_polls = 0U;
    s_mask = 0U;
    return (nx_stm32_system_t){
        .rcc = &s_rcc, .flash = &s_flash, .power = &s_power, .timer = &s_timer};
}

/** \brief Exercise success, each startup failure, failed cleanup and overflow.
 */
int main(void) {
    assert(nx_stm32_validate_identity(524288U) == 0);
    assert(nx_stm32_validate_identity(1048576U) != 0);
    s_flash_kib = 1024U;
    assert(nx_stm32_validate_identity(1048576U) == 0);
    s_device_id = 0x414U;
    assert(nx_stm32_validate_identity(1048576U) != 0);
    nx_stm32_system_t system = fresh(0U);
    s_rcc.CSR = RCC_CSR_IWDGRSTF;
    nx_stm32_start_result_t result = nx_stm32_system_start(&system, 4U);
    assert(result.primary == 0 && system.started);
    assert(system.reset_cause == RCC_CSR_IWDGRSTF);
    assert(system.core_hz == 168000000U && s_timer.PSC == 83U);
    assert(s_timer.ARR == UINT32_MAX && s_timer.CNT == 0U);
    s_timer.CNT = 99U;
    assert(nx_stm32_system_now(&system) == 99U);
    s_timer.SR = TIM_SR_UIF;
    s_timer.CNT = 7U;
    assert(nx_stm32_system_now(&system) == (UINT64_C(1) << 32U) + 7U);
    nx_stm32_system_timer_irq(&system);
    assert(nx_stm32_system_now(&system) == (UINT64_C(1) << 32U) + 7U);
    s_mask = 1U;
    assert(nx_stm32_system_now(&system) != 0U && s_mask == 1U);
    assert(nx_stm32_system_stop(&system, 4U) == 0);
    assert(system.remaining_effects == 0U && !system.started);
    for (unsigned fault = 1U; fault <= 3U; ++fault) {
        system = fresh(fault);
        result = nx_stm32_system_start(&system, 4U);
        assert(result.primary != 0 && result.cleanup == 0);
        assert(result.remaining_effects == 0U && !system.started);
        assert((s_rcc.CR & RCC_CR_PLLON) == 0U);
        assert(s_polls <= 20U);
    }
    system = fresh(4U);
    result = nx_stm32_system_start(&system, 4U);
    assert(result.primary != 0 && result.cleanup != 0);
    assert(result.remaining_effects != 0U && !system.started);
    assert(nx_stm32_system_start(&system, 4U).primary == -2);
    assert(nx_stm32_system_start(NULL, 4U).primary == -2);
    puts("STM32 clock/timebase: 7 fault and lifecycle scenarios passed");
    return 0;
}
