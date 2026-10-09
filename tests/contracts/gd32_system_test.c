/**
 * \file            gd32_system_test.c
 * \brief           Real GD32 clock rollback and monotonic overflow source tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _GNU_SOURCE
#include "gd32f4xx.h"
#include "private/system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
const uint32_t __gVectors[] = {0u};
extern uint32_t SystemCoreClock;
void TIMER1_IRQHandler(void);
static unsigned s_fault;
static unsigned s_checks;

/** \brief           Keep failure checks live independently of NDEBUG. */
static void check(bool value, const char* expression, unsigned line) {
    ++s_checks;
    if (!value) {
        fprintf(stderr, "GD32 clock model check %u failed: %s\n", line,
                expression);
        exit(1);
    }
}
#define CHECK(expression) check((expression), #expression, __LINE__)

/** \brief           Advance clock hardware phases unless specifically faulted.
 */
void nx_gd32_model_clock_poll(void) {
    if ((RCU_CTL & RCU_CTL_IRC16MEN) != 0u && s_fault != 6u) {
        RCU_CTL |= RCU_CTL_IRC16MSTB;
    }
    if ((RCU_CTL & RCU_CTL_HXTALEN) != 0u && s_fault != 1u) {
        RCU_CTL |= RCU_CTL_HXTALSTB;
    } else if ((RCU_CTL & RCU_CTL_HXTALEN) == 0u) {
        RCU_CTL &= ~RCU_CTL_HXTALSTB;
    }
    if ((RCU_CTL & RCU_CTL_PLLEN) != 0u && s_fault != 2u) {
        RCU_CTL |= RCU_CTL_PLLSTB;
    } else if ((RCU_CTL & RCU_CTL_PLLEN) == 0u) {
        RCU_CTL &= ~RCU_CTL_PLLSTB;
    }
    if ((PMU_CTL & PMU_CTL_HDEN) != 0u && s_fault != 4u) {
        PMU_CS |= PMU_CS_HDRF;
    }
    if ((PMU_CTL & PMU_CTL_HDS) != 0u && s_fault != 5u) {
        PMU_CS |= PMU_CS_HDSRF;
    }
    if ((RCU_CFG0 & RCU_CFG0_SCS) == RCU_CKSYSSRC_PLLP && s_fault != 3u) {
        RCU_CFG0 = (RCU_CFG0 & ~(uint32_t)RCU_CFG0_SCSS) | RCU_SCSS_PLLP;
    } else if ((RCU_CFG0 & RCU_CFG0_SCS) == RCU_CKSYSSRC_IRC16M &&
               s_fault != 6u) {
        RCU_CFG0 &= ~(uint32_t)RCU_CFG0_SCSS;
    }
}

/** \brief           Model frequency decode from observed system-clock status.
 */
uint32_t rcu_clock_freq_get(rcu_clock_freq_enum clock) {
    bool pll = (RCU_CFG0 & RCU_CFG0_SCSS) == RCU_SCSS_PLLP;
    if (clock == CK_AHB) {
        return pll ? 200000000u : 16000000u;
    }
    if (clock == CK_APB1) {
        return pll ? 50000000u : 16000000u;
    }
    return pll ? 100000000u : 16000000u;
}
/** \brief           Model Flash wait-state configuration. */
void fmc_wscnt_set(uint32_t states) {
    FMC_WS = states;
}
/** \brief           The clock profile's timer multiplier is fixed by its
 * caller. */
void rcu_timer_clock_prescaler_config(uint32_t divider) {
    (void)divider;
}
/** \brief           Decode the official SDK clock-bit encoding. */
void rcu_periph_clock_enable(rcu_periph_enum periph) {
    uint32_t offset = (uint32_t)periph >> 6;
    uint32_t bit = (uint32_t)periph & 31u;
    REG32(RCU + offset) |= 1u << bit;
}
/** \brief           Decode the official SDK clock-bit release encoding. */
void rcu_periph_clock_disable(rcu_periph_enum periph) {
    REG32(RCU + ((uint32_t)periph >> 6)) &= ~(1u << ((uint32_t)periph & 31u));
}
/** \brief           Model sleep clock ownership independently of run clock. */
void rcu_periph_clock_sleep_enable(rcu_periph_sleep_enum periph) {
    REG32(RCU + ((uint32_t)periph >> 6)) |= 1u << ((uint32_t)periph & 31u);
}
/** \brief           Release the exact modeled sleep-clock bit. */
void rcu_periph_clock_sleep_disable(rcu_periph_sleep_enum periph) {
    REG32(RCU + ((uint32_t)periph >> 6)) &= ~(1u << ((uint32_t)periph & 31u));
}
/** \brief           Reset modeled timer state before real initialization
 * writes. */
void timer_deinit(uint32_t timer) {
    memset((void*)(uintptr_t)timer, 0, 0x50u);
}

/** \brief           Reset hardware only between completed ownership releases.
 */
static void fresh(unsigned fault) {
    memset((void*)0x40000000u, 0, 0x40000u);
    memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
    RCU_CTL = RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB;
    s_fault = fault;
}

/** \brief           Verify clock faults, retained ownership and wrap
 * publication. */
int main(void) {
    void* mapped =
        mmap((void*)0x40000000u, 0x40000u, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(mapped == (void*)0x40000000u);
    fresh(0u);
    CHECK(nx_gd32_soc_start() == 0 && SystemCoreClock == 200000000u);
    CHECK(TIMER_PSC(TIMER1) == 99u && TIMER_CAR(TIMER1) == UINT32_MAX);
    CHECK(nx_gd32_soc_start() != 0);
    TIMER_CNT(TIMER1) = 123u;
    CHECK(nx_gd32_now_us() == 123u);
    TIMER_CNT(TIMER1) = 7u;
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    CHECK(nx_gd32_now_us() == (UINT64_C(1) << 32) + 7u);
    TIMER1_IRQHandler();
    CHECK(nx_gd32_now_us() == (UINT64_C(1) << 32) + 7u);
    g_gd32_model_mask = 1u;
    CHECK(nx_gd32_now_us() == (UINT64_C(1) << 32) + 7u);
    CHECK(g_gd32_model_mask == 1u && nx_gd32_soc_stop() != 0);
    g_gd32_model_mask = 0u;
    NVIC->ISER[0] = 1u;
    CHECK(nx_gd32_soc_stop() != 0);
    NVIC->ISER[0] = 0u;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
    CHECK(nx_gd32_soc_stop() != 0);
    SysTick->CTRL = 0u;
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    CHECK(nx_gd32_soc_stop() != 0);
    SCB->ICSR = 0u;
    CHECK(nx_gd32_soc_stop() == 0 && SystemCoreClock == 16000000u);
    CHECK((RCU_CTL & RCU_CTL_PLLEN) == 0u);
    for (unsigned fault = 1u; fault <= 5u; ++fault) {
        fresh(fault);
        CHECK(nx_gd32_soc_start() != 0);
        CHECK(nx_gd32_soc_stop() == 0);
        CHECK(SystemCoreClock == 16000000u && (RCU_CTL & RCU_CTL_PLLEN) == 0u);
    }
    fresh(0u);
    CHECK(nx_gd32_soc_start() == 0);
    s_fault = 6u;
    CHECK(nx_gd32_soc_stop() != 0);
    CHECK((RCU_CTL & RCU_CTL_PLLEN) != 0u);
    CHECK(nx_gd32_soc_start() != 0);
    s_fault = 0u;
    CHECK(nx_gd32_soc_stop() == 0);
    fresh(0u);
    CHECK(nx_gd32_soc_start() == 0);
    USART_CTL0(USART0) = USART_CTL0_UEN;
    CHECK(nx_gd32_soc_stop() != 0);
    USART_CTL0(USART0) = 0u;
    CHECK(nx_gd32_soc_stop() == 0);
    printf("GD32 production clock host model: %u checks passed\n", s_checks);
    return 0;
}
