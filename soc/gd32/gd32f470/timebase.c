/**
 * \file            timebase.c
 * \brief           GD32F470 owned startup rollback and fixed TIMER1 clock
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f4xx.h"
#include "nexus/core/fault.h"
#include "private/system.h"
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
#include "private/gd32f470_lowpower.h"
#endif

#ifndef NX_GD32_TIMEBASE_ACK
#define NX_GD32_TIMEBASE_ACK(value) (TIMER_INTF(TIMER1) = (value))
#endif
#ifndef NX_GD32_TIMEBASE_WRITE
#define NX_GD32_TIMEBASE_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif

static volatile uint32_t s_epoch;
static bool s_clock_owned;
enum {
    TIMER_EFFECT_OWNED = 1U,
    TIMER_EFFECT_GATE = 2U,
    TIMER_EFFECT_SLEEP_GATE = 4U,
    TIMER_EFFECT_QUIET = 8U
};
static volatile uint8_t s_timer_effects;
static volatile bool s_timer_live;
static volatile nx_result_t s_timer_error;

int nx_gd32_clock_start(void);
int nx_gd32_clock_stop(void);

/** \brief Observe each initial timer field before publishing a live source.
 */
static bool timer_write_checked(volatile uint32_t* reg, uint32_t value,
                                uint32_t mask) {
    NX_GD32_TIMEBASE_WRITE(reg, value);
    nx_arch_dsb();
    return (*reg & mask) == (value & mask);
}

/** \brief Bound the update shadow acknowledgement without a running timer.
 */
static bool timer_update_ready(void) {
    for (uint32_t polls = 0U; polls < 1000000U; ++polls) {
        if ((TIMER_INTF(TIMER1) & TIMER_INTF_UPIF) != 0U)
            return true;
    }
    return false;
}

/** \brief Retained trigger/channel state is not a free monotonic timer. */
static bool timer_mode_idle(void) {
    return TIMER_CTL0(TIMER1) == 0U && TIMER_CTL1(TIMER1) == 0U &&
           TIMER_SMCFG(TIMER1) == 0U && TIMER_DMAINTEN(TIMER1) == 0U &&
           TIMER_CHCTL0(TIMER1) == 0U && TIMER_CHCTL1(TIMER1) == 0U &&
           TIMER_CHCTL2(TIMER1) == 0U && TIMER_DMACFG(TIMER1) == 0U;
}

/** \brief Isolate the active timer without releasing its clock or storage. */
static bool isolate_timer_irq(void) {
    NX_GD32_TIMEBASE_WRITE(&TIMER_DMAINTEN(TIMER1), 0U);
    NX_GD32_TIMEBASE_WRITE(&TIMER_CTL0(TIMER1),
                           TIMER_CTL0(TIMER1) & ~(uint32_t)TIMER_CTL0_CEN);
    NVIC_DisableIRQ(TIMER1_IRQn);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    nx_gd32_peripheral_barrier();
    uint32_t bank = (uint32_t)TIMER1_IRQn / 32U;
    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32U);
    /* Own ACTIVE stays set until return; task stop still checks every owner. */
    return TIMER_DMAINTEN(TIMER1) == 0U &&
           (TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) == 0U &&
           ((NVIC->ISER[bank] | NVIC->ISPR[bank]) & bit) == 0U;
}

/** \brief Retain the current mask and effects after loss of the time domain. */
static _Noreturn void timer_park(void) {
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}

/** \brief           Check all external interrupt and DMA resources at stop. */
static bool external_resources_idle(void) {
    /* Cold stop already requires every OS consumer joined. CTRL is a
     * read-clear register: this defensive enabled check consumes COUNTFLAG,
     * and is not a strict no-effect probe of a running scheduler. */
    if ((SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) != 0u ||
        (SCB->ICSR & (SCB_ICSR_PENDSTSET_Msk | SCB_ICSR_PENDSVSET_Msk)) != 0u) {
        return false;
    }
    for (uint32_t irq = 0u; irq <= (uint32_t)IPA_IRQn; ++irq) {
        uint32_t bank = irq / 32u;
        uint32_t bit = UINT32_C(1) << (irq % 32u);
        if ((NVIC->IABR[bank] & bit) != 0u ||
            ((irq != (uint32_t)TIMER1_IRQn ||
              (s_timer_effects & TIMER_EFFECT_OWNED) == 0U) &&
             ((NVIC->ISER[bank] | NVIC->ISPR[bank]) & bit) != 0u)) {
            return false;
        }
    }
    if ((USART_CTL0(USART0) & USART_CTL0_UEN) != 0u ||
        (USART_CTL0(USART1) & USART_CTL0_UEN) != 0u ||
        (SPI_CTL0(SPI0) & SPI_CTL0_SPIEN) != 0u ||
        (SPI_CTL0(SPI4) & SPI_CTL0_SPIEN) != 0u ||
        (I2C_CTL0(I2C0) & I2C_CTL0_I2CEN) != 0u ||
        (I2C_CTL0(I2C1) & I2C_CTL0_I2CEN) != 0u ||
        (TIMER_CTL0(TIMER0) & TIMER_CTL0_CEN) != 0u ||
        (TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN) != 0u ||
        (TIMER_CTL0(TIMER3) & TIMER_CTL0_CEN) != 0u ||
        (ADC_CTL1(ADC0) & ADC_CTL1_ADCON) != 0u ||
        (FMC_STAT & FMC_STAT_BUSY) != 0u) {
        return false;
    }
    for (uint32_t channel = 0u; channel < 8u; ++channel) {
        if ((DMA_CHCTL(DMA0, channel) & DMA_CHXCTL_CHEN) != 0u ||
            (DMA_CHCTL(DMA1, channel) & DMA_CHXCTL_CHEN) != 0u) {
            return false;
        }
    }
    return true;
}

/** \brief           Release timer effects, retaining ownership on failure. */
static int stop_timer(void) {
    if (s_timer_effects == 0U) {
        return 0;
    }
    if ((s_timer_effects & TIMER_EFFECT_OWNED) == 0U)
        goto release_gates;
    uint32_t bank = (uint32_t)TIMER1_IRQn / 32u;
    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32u);
    if ((NVIC->IABR[bank] & bit) != 0u) {
        return -1;
    }
    /* No consumer may retain deadlines beyond clock withdrawal. End the live
     * epoch before interrupt or timer access is removed, even if cleanup fails.
     */
    s_timer_live = false;
    NVIC_DisableIRQ(TIMER1_IRQn);
    if (!timer_write_checked(&RCU_APB1RST,
                             RCU_APB1RST & ~(uint32_t)RCU_APB1RST_TIMER1RST,
                             RCU_APB1RST_TIMER1RST))
        return -1;
    NX_GD32_TIMEBASE_WRITE(&TIMER_CTL0(TIMER1),
                           TIMER_CTL0(TIMER1) & ~TIMER_CTL0_CEN);
    NX_GD32_TIMEBASE_WRITE(&TIMER_DMAINTEN(TIMER1), 0u);
    NX_GD32_TIMEBASE_WRITE(&TIMER_INTF(TIMER1), 0u);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    nx_gd32_peripheral_barrier();
    if ((TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) != 0u ||
        TIMER_DMAINTEN(TIMER1) != 0U || TIMER_INTF(TIMER1) != 0U ||
        ((NVIC->ISER[bank] | NVIC->ISPR[bank]) & bit) != 0u) {
        return -1;
    }
    if ((s_timer_effects & TIMER_EFFECT_QUIET) != 0U) {
        if (!timer_write_checked(&TIMER_CTL1(TIMER1), 0U, TIMER_CTL1_MMC))
            return -1;
        s_timer_effects &= (uint8_t)~TIMER_EFFECT_QUIET;
    }
    s_timer_effects &= (uint8_t)~TIMER_EFFECT_OWNED;
release_gates:
    if ((s_timer_effects & TIMER_EFFECT_SLEEP_GATE) != 0U) {
        rcu_periph_clock_sleep_disable(RCU_TIMER1_SLP);
        if ((RCU_APB1SPEN & RCU_APB1SPEN_TIMER1SPEN) != 0U)
            return -1;
        s_timer_effects &= (uint8_t)~TIMER_EFFECT_SLEEP_GATE;
    }
    if ((s_timer_effects & TIMER_EFFECT_GATE) != 0U) {
        rcu_periph_clock_disable(RCU_TIMER1);
        if ((RCU_APB1EN & RCU_APB1EN_TIMER1EN) != 0U)
            return -1;
        s_timer_effects &= (uint8_t)~TIMER_EFFECT_GATE;
    }
    s_epoch = 0u;
    return 0;
}

/** \brief Inspect the cold clock and known execution owners without writes. */
nx_result_t nx_gd32_soc_preflight(void) {
    if (!nx_arch_is_privileged() || nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (s_clock_owned || s_timer_effects != 0U || s_timer_live) {
        return NX_ERROR_BUSY;
    }
    nx_result_t clock = nx_gd32_clock_preflight();
    if (clock != NX_SUCCESS) {
        return clock;
    }
    uint32_t bank = (uint32_t)TIMER1_IRQn / 32u;
    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32u);
    if (!timer_mode_idle() || (RCU_APB1RST & RCU_APB1RST_TIMER1RST) != 0U ||
        (RCU_APB1EN & RCU_APB1EN_TIMER1EN) != 0u ||
        ((NVIC->ISER[bank] | NVIC->ISPR[bank] | NVIC->IABR[bank]) & bit) !=
            0u ||
        !external_resources_idle()) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

/** \brief           Start the fixed clock and timer with explicit rollback. */
int nx_gd32_soc_start(void) {
    if (nx_gd32_soc_preflight() != NX_SUCCESS) {
        return -1;
    }
    s_timer_error = NX_SUCCESS;
    s_clock_owned = true;
    if (nx_gd32_clock_start() != 0) {
        if (nx_gd32_clock_stop() == 0) {
            s_clock_owned = false;
        }
        return -2;
    }
    s_timer_effects |= TIMER_EFFECT_GATE;
    rcu_periph_clock_enable(RCU_TIMER1);
    nx_arch_dsb();
    if ((RCU_APB1EN & RCU_APB1EN_TIMER1EN) == 0U || !timer_mode_idle() ||
        (RCU_APB1RST & RCU_APB1RST_TIMER1RST) != 0U)
        goto timer_failure;
    if ((RCU_APB1SPEN & RCU_APB1SPEN_TIMER1SPEN) == 0U) {
        s_timer_effects |= TIMER_EFFECT_SLEEP_GATE;
        rcu_periph_clock_sleep_enable(RCU_TIMER1_SLP);
        nx_arch_dsb();
        if ((RCU_APB1SPEN & RCU_APB1SPEN_TIMER1SPEN) == 0U)
            goto timer_failure;
    }
    s_timer_effects |= TIMER_EFFECT_OWNED;
    timer_deinit(TIMER1);
    if (!timer_mode_idle() || (RCU_APB1RST & RCU_APB1RST_TIMER1RST) != 0U)
        goto timer_failure;
    NX_GD32_TIMEBASE_WRITE(&TIMER_PSC(TIMER1), 99u);
    NX_GD32_TIMEBASE_WRITE(&TIMER_CAR(TIMER1), UINT32_MAX);
    if (!timer_write_checked(&TIMER_INTF(TIMER1), 0U, UINT32_MAX))
        goto timer_failure;
    /* UPG must not escape on TRGO while establishing the prescaler epoch. */
    s_timer_effects |= TIMER_EFFECT_QUIET;
    if (!timer_write_checked(&TIMER_CTL1(TIMER1), TIMER_TRI_OUT_SRC_ENABLE,
                             TIMER_CTL1_MMC))
        goto timer_failure;
    NX_GD32_TIMEBASE_WRITE(&TIMER_SWEVG(TIMER1), TIMER_SWEVG_UPG);
    nx_arch_dsb();
    if (!timer_update_ready() ||
        !timer_write_checked(&TIMER_INTF(TIMER1), 0U, UINT32_MAX) ||
        !timer_write_checked(&TIMER_CNT(TIMER1), 0U, UINT32_MAX) ||
        !timer_write_checked(&TIMER_CTL1(TIMER1), 0U, TIMER_CTL1_MMC))
        goto timer_failure;
    s_timer_effects &= (uint8_t)~TIMER_EFFECT_QUIET;
    NX_GD32_TIMEBASE_WRITE(&TIMER_DMAINTEN(TIMER1), TIMER_DMAINTEN_UPIE);
    s_epoch = 0u;
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_SetPriority(TIMER1_IRQn, 5u);
    NVIC_EnableIRQ(TIMER1_IRQn);
    NX_GD32_TIMEBASE_WRITE(&TIMER_CTL0(TIMER1), TIMER_CTL0_CEN);
    nx_gd32_peripheral_barrier();
    if ((TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) == 0u ||
        TIMER_PSC(TIMER1) != 99u || TIMER_CAR(TIMER1) != UINT32_MAX ||
        (RCU_APB1EN & RCU_APB1EN_TIMER1EN) == 0u) {
        goto timer_failure;
    }
    s_timer_live = true;
    return 0;
timer_failure:
    (void)nx_gd32_soc_stop();
    return -3;
}

/** \brief           Stop only after all external execution owners are quiet. */
int nx_gd32_soc_stop(void) {
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
    if (nx_gd32_lowpower_busy()) {
        return -1;
    }
#endif
    if (nx_gd32_in_isr() || nx_gd32_irq_masked() ||
        !external_resources_idle() || stop_timer() != 0) {
        return -1;
    }
    if (s_clock_owned) {
        if (nx_gd32_clock_stop() != 0) {
            return -2;
        }
        s_clock_owned = false;
    }
    return 0;
}

/** \brief           Sample pending wrap without stealing the ISR's flag. */
uint64_t nx_gd32_now_us(void) {
    if (!s_timer_live) {
        return 0u;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if (!s_timer_live) {
        nx_gd32_critical_leave(saved);
        return 0U;
    }
    uint32_t epoch = s_epoch;
    uint32_t count = TIMER_CNT(TIMER1);
    if ((TIMER_INTF(TIMER1) & TIMER_INTF_UPIF) != 0u) {
        count = TIMER_CNT(TIMER1);
        ++epoch;
    }
    uint64_t value = ((uint64_t)epoch << 32) | count;
    nx_gd32_critical_leave(saved);
    return value;
}

/** \brief Read the runtime diagnostic without implying a live time domain. */
nx_result_t nx_gd32_timebase_error(void) {
    return s_timer_error;
}

/** \brief           Extend the hardware's 32-bit counter once per wrap. */
void TIMER1_IRQHandler(void) {
    if ((s_timer_effects & TIMER_EFFECT_OWNED) == 0U ||
        s_timer_error != NX_SUCCESS) {
        return;
    }
    uint32_t saved = nx_gd32_critical_enter();
    if ((TIMER_INTF(TIMER1) & TIMER_INTF_UPIF) != 0u) {
        /* W0C ones preserve compare flags arriving after this UPIF read. */
        NX_GD32_TIMEBASE_ACK(~(uint32_t)TIMER_INTF_UPIF);
        nx_arch_dsb();
        if ((TIMER_INTF(TIMER1) & TIMER_INTF_UPIF) != 0U) {
            s_timer_live = false;
            s_timer_error = NX_ERROR_IO;
            (void)isolate_timer_irq();
            /* Even observed isolation cannot revive consumer deadlines. Keep
             * the saved global mask, clock and storage through terminal park.
             */
            timer_park();
        }
        if (s_timer_live) {
            ++s_epoch;
        }
    }
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
    if (s_timer_live) {
        nx_gd32_lowpower_alarm_irq();
    }
#endif
    nx_gd32_critical_leave(saved);
}

/** \brief           Publish the selected monotonic boot domain. */
uint64_t nx_time_now_us(void) {
    return nx_gd32_now_us();
}
