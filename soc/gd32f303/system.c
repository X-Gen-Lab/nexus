/**
 * \file            system.c
 * \brief           Checked GD32F303RCT6 reset-like IRC8 startup and time domain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_system.h"
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
nx_gd32f303_system_t g_nx_gd32f303_system;
uint32_t SystemCoreClock = 8000000U;
#ifndef NX_GD303_CPU_INIT
#define NX_GD303_CPU_INIT()                                                    \
    do {                                                                       \
        SCB->CPACR |= UINT32_C(0x00f00000);                                    \
        SCB->VTOR = UINT32_C(0x08000000);                                      \
        NVIC_SetPriorityGrouping(3U);                                          \
    } while (0)
#endif
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
void SystemInit(void) {
    NX_GD303_CPU_INIT();
    nx_arch_dsb();
    nx_arch_isb();
}
nx_result_t nx_gd32f303_system_preflight(const nx_gd32f303_system_t* s) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (s->started || s->remaining_effects != 0U)
        return NX_ERROR_BUSY;
    if (NX_GD303_DENSITY() != ((48U << 16U) | 256U))
        return NX_ERROR_UNSUPPORTED;
    if ((NX_GD303_READ(&RCU_CTL) &
         (RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB |
          RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_HXTALBPS |
          RCU_CTL_CKMEN)) != (RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB) ||
        (NX_GD303_READ(&RCU_CFG0) &
         (RCU_CFG0_SCS | RCU_CFG0_SCSS | RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC |
          RCU_CFG0_APB2PSC)) != 0U ||
        (NX_GD303_READ(&RCU_APB1EN) & RCU_APB1EN_TIMER1EN) != 0U ||
        !NX_GD303_IRQ_AVAILABLE(TIMER1_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_GD303_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_GD303_READ(reg) & mask) == (value & mask);
}
nx_result_t nx_gd32f303_system_stop(nx_gd32f303_system_t* s) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if ((s->remaining_effects & 2U) != 0U) {
        s->started = false;
        nx_result_t irq = NX_GD303_IRQ_CONTROL(TIMER1_IRQn, false, 0U);
        bool sources = write_checked(&TIMER_DMAINTEN(TIMER1), 0U, UINT32_MAX);
        bool counter = write_checked(&TIMER_CTL0(TIMER1), 0U, UINT32_MAX);
        bool master = write_checked(&TIMER_CTL1(TIMER1), 0U, UINT32_MAX);
        NX_GD303_WRITE(&TIMER_INTF(TIMER1), 0U);
        nx_arch_dsb();
        bool flags = NX_GD303_READ(&TIMER_INTF(TIMER1)) == 0U;
        /* A source can repend while being withdrawn. Release only after the
         * final NVIC disable/clear/readback follows every timer source. */
        nx_result_t final_irq = NX_GD303_IRQ_CONTROL(TIMER1_IRQn, false, 0U);
        if (irq != NX_SUCCESS || final_irq != NX_SUCCESS || !sources ||
            !counter || !master || !flags)
            return NX_ERROR_IO;
        s->remaining_effects &= ~2U;
    }
    if ((s->remaining_effects & 1U) != 0U) {
        if (!write_checked(&RCU_APB1EN,
                           NX_GD303_READ(&RCU_APB1EN) &
                               ~(uint32_t)RCU_APB1EN_TIMER1EN,
                           RCU_APB1EN_TIMER1EN))
            return NX_ERROR_IO;
        s->remaining_effects &= ~1U;
    }
    s->core_hz = 0U;
    return NX_SUCCESS;
}
nx_gd32f303_start_result_t nx_gd32f303_system_start(nx_gd32f303_system_t* s,
                                                    uint32_t limit) {
    nx_gd32f303_start_result_t result = {NX_ERROR_INVALID, NX_SUCCESS, 0U};
    if (limit == 0U)
        return result;
    result.primary = nx_gd32f303_system_preflight(s);
    if (result.primary != NX_SUCCESS)
        return result;
    s->reset_cause = NX_GD303_READ(&RCU_RSTSCK);
    s->remaining_effects = 1U;
    if (!write_checked(&RCU_APB1EN,
                       NX_GD303_READ(&RCU_APB1EN) | RCU_APB1EN_TIMER1EN,
                       RCU_APB1EN_TIMER1EN)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* A powered-down timer can still retain foreign state. Never reset it. */
    if (NX_GD303_READ(&TIMER_CTL0(TIMER1)) != 0U ||
        NX_GD303_READ(&TIMER_CTL1(TIMER1)) != 0U ||
        NX_GD303_READ(&TIMER_SMCFG(TIMER1)) != 0U ||
        NX_GD303_READ(&TIMER_CHCTL0(TIMER1)) != 0U ||
        NX_GD303_READ(&TIMER_CHCTL1(TIMER1)) != 0U ||
        NX_GD303_READ(&TIMER_CHCTL2(TIMER1)) != 0U ||
        NX_GD303_READ(&TIMER_INTF(TIMER1)) != 0U ||
        NX_GD303_READ(&TIMER_DMAINTEN(TIMER1)) != 0U) {
        result.primary = NX_ERROR_BUSY;
        goto rollback;
    }
    s->remaining_effects |= 2U;
    s->overflow_us = 0U;
    s->timer_error = NX_SUCCESS;
    bool valid = write_checked(&TIMER_PSC(TIMER1), 7U, 0xffffU);
    valid = write_checked(&TIMER_CAR(TIMER1), 0xffffU, 0xffffU) && valid;
    valid = write_checked(&TIMER_CNT(TIMER1), 0U, 0xffffU) && valid;
    /* MMC1 follows CEN, held low: UPG must not escape as reset-mode TRGO. */
    valid =
        write_checked(&TIMER_CTL1(TIMER1), 1U << 4U, TIMER_CTL1_MMC) && valid;
    if (!valid) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    NX_GD303_WRITE(&TIMER_SWEVG(TIMER1), TIMER_SWEVG_UPG);
    bool updated = false;
    while (limit-- != 0U) {
        NX_GD303_POLL();
        if ((NX_GD303_READ(&TIMER_INTF(TIMER1)) & TIMER_INTF_UPIF) != 0U) {
            updated = true;
            break;
        }
    }
    if (!updated) {
        result.primary = NX_ERROR_TIMEOUT;
        goto rollback;
    }
    valid = write_checked(&TIMER_INTF(TIMER1), 0U, TIMER_INTF_UPIF) && valid;
    valid = write_checked(&TIMER_CTL1(TIMER1), 0U, UINT32_MAX) && valid;
    valid = write_checked(&TIMER_DMAINTEN(TIMER1), TIMER_DMAINTEN_UPIE,
                          UINT32_MAX) &&
            valid;
    valid =
        write_checked(&TIMER_CTL0(TIMER1), TIMER_CTL0_CEN, UINT32_MAX) && valid;
    if (!valid) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    result.primary = NX_GD303_IRQ_CONTROL(TIMER1_IRQn, true, 0U);
    if (result.primary != NX_SUCCESS)
        goto rollback;
    s->core_hz = 8000000U;
    s->started = true;
    return result;
rollback:
    result.cleanup = nx_gd32f303_system_stop(s);
    result.remaining_effects = s->remaining_effects;
    return result;
}
/** \brief A lost monotonic domain cannot resume any deadline consumer. */
static _Noreturn void timer_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}
void nx_gd32f303_system_timer_irq(nx_gd32f303_system_t* s) {
    if (s == NULL || !s->started ||
        (NX_GD303_READ(&TIMER_INTF(TIMER1)) & TIMER_INTF_UPIF) == 0U)
        return;
    NX_GD303_WRITE(&TIMER_INTF(TIMER1), ~(uint32_t)TIMER_INTF_UPIF);
    nx_arch_dsb();
    if ((NX_GD303_READ(&TIMER_INTF(TIMER1)) & TIMER_INTF_UPIF) != 0U) {
        s->started = false;
        s->timer_error = NX_ERROR_IO;
        (void)write_checked(&TIMER_DMAINTEN(TIMER1), 0U, UINT32_MAX);
        (void)write_checked(&TIMER_CTL0(TIMER1), 0U, UINT32_MAX);
        (void)NX_GD303_IRQ_CONTROL(TIMER1_IRQn, false, 0U);
        timer_park();
    }
    s->overflow_us += UINT64_C(65536);
}
uint64_t nx_gd32f303_system_now(nx_gd32f303_system_t* s) {
    if (s == NULL || !s->started || !nx_arch_is_privileged())
        return 0U;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    uint64_t high = s->overflow_us;
    uint32_t count = NX_GD303_READ(&TIMER_CNT(TIMER1)) & 0xffffU;
    if ((NX_GD303_READ(&TIMER_INTF(TIMER1)) & TIMER_INTF_UPIF) != 0U) {
        high += UINT64_C(65536);
        count = NX_GD303_READ(&TIMER_CNT(TIMER1)) & 0xffffU;
    }
    nx_arch_irq_restore(guard);
    return high + count;
}
nx_time_us_t nx_time_now_us(void) {
    return nx_gd32f303_system_now(&g_nx_gd32f303_system);
}
void TIMER1_IRQHandler(void) {
    nx_gd32f303_system_timer_irq(&g_nx_gd32f303_system);
}
