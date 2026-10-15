/**
 * \file            timebase.c
 * \brief           GD32F470 owned startup rollback and fixed TIMER1 clock
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f4xx.h"
#include "private/system.h"

static volatile uint32_t s_epoch;
static bool s_clock_owned;
static bool s_timer_owned;

int nx_gd32_clock_start(void);
int nx_gd32_clock_stop(void);

/** \brief           Check all external interrupt and DMA resources at stop. */
static bool external_resources_idle(void) {
    if ((SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) != 0u ||
        (SCB->ICSR & (SCB_ICSR_PENDSTSET_Msk | SCB_ICSR_PENDSVSET_Msk)) != 0u) {
        return false;
    }
    for (uint32_t irq = 0u; irq <= (uint32_t)IPA_IRQn; ++irq) {
        uint32_t bank = irq / 32u;
        uint32_t bit = UINT32_C(1) << (irq % 32u);
        if ((NVIC->IABR[bank] & bit) != 0u ||
            ((irq != (uint32_t)TIMER1_IRQn || !s_timer_owned) &&
             ((NVIC->ISER[bank] | NVIC->ISPR[bank]) & bit) != 0u)) {
            return false;
        }
    }
    if ((USART_CTL0(USART0) & USART_CTL0_UEN) != 0u ||
        (SPI_CTL0(SPI4) & SPI_CTL0_SPIEN) != 0u ||
        (I2C_CTL0(I2C0) & I2C_CTL0_I2CEN) != 0u ||
        (TIMER_CTL0(TIMER2) & TIMER_CTL0_CEN) != 0u ||
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
    if (!s_timer_owned) {
        return 0;
    }
    uint32_t bank = (uint32_t)TIMER1_IRQn / 32u;
    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32u);
    if ((NVIC->IABR[bank] & bit) != 0u) {
        return -1;
    }
    NVIC_DisableIRQ(TIMER1_IRQn);
    TIMER_CTL0(TIMER1) &= ~TIMER_CTL0_CEN;
    TIMER_DMAINTEN(TIMER1) = 0u;
    TIMER_INTF(TIMER1) = 0u;
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    nx_gd32_peripheral_barrier();
    if ((TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) != 0u ||
        (NVIC->ISER[bank] & bit) != 0u) {
        return -1;
    }
    rcu_periph_clock_sleep_disable(RCU_TIMER1_SLP);
    rcu_periph_clock_disable(RCU_TIMER1);
    if ((RCU_APB1EN & RCU_APB1EN_TIMER1EN) != 0u ||
        (RCU_APB1SPEN & RCU_APB1SPEN_TIMER1SPEN) != 0u) {
        return -1;
    }
    s_timer_owned = false;
    s_epoch = 0u;
    return 0;
}

/** \brief           Start the fixed clock and timer with explicit rollback. */
int nx_gd32_soc_start(void) {
    if (nx_gd32_in_isr() || nx_gd32_irq_masked() || s_clock_owned ||
        s_timer_owned) {
        return -1;
    }
    s_clock_owned = true;
    if (nx_gd32_clock_start() != 0) {
        if (nx_gd32_clock_stop() == 0) {
            s_clock_owned = false;
        }
        return -2;
    }
    s_timer_owned = true;
    rcu_periph_clock_enable(RCU_TIMER1);
    rcu_periph_clock_sleep_enable(RCU_TIMER1_SLP);
    timer_deinit(TIMER1);
    TIMER_PSC(TIMER1) = 99u;
    TIMER_CAR(TIMER1) = UINT32_MAX;
    TIMER_CNT(TIMER1) = 0u;
    TIMER_SWEVG(TIMER1) = TIMER_SWEVG_UPG;
    TIMER_INTF(TIMER1) = 0u;
    TIMER_DMAINTEN(TIMER1) = TIMER_DMAINTEN_UPIE;
    s_epoch = 0u;
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_SetPriority(TIMER1_IRQn, 5u);
    NVIC_EnableIRQ(TIMER1_IRQn);
    TIMER_CTL0(TIMER1) = TIMER_CTL0_CEN;
    nx_gd32_peripheral_barrier();
    if ((TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) == 0u ||
        TIMER_PSC(TIMER1) != 99u || TIMER_CAR(TIMER1) != UINT32_MAX ||
        (RCU_APB1EN & RCU_APB1EN_TIMER1EN) == 0u) {
        (void)nx_gd32_soc_stop();
        return -3;
    }
    return 0;
}

/** \brief           Stop only after all external execution owners are quiet. */
int nx_gd32_soc_stop(void) {
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
    uint32_t saved = nx_gd32_critical_enter();
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

/** \brief           Extend the hardware's 32-bit counter once per wrap. */
void TIMER1_IRQHandler(void) {
    uint32_t saved = nx_gd32_critical_enter();
    if ((TIMER_INTF(TIMER1) & TIMER_INTF_UPIF) != 0u) {
        ++s_epoch;
        TIMER_INTF(TIMER1) &= ~TIMER_INTF_UPIF;
    }
    nx_gd32_critical_leave(saved);
}

/** \brief           Publish the selected monotonic boot domain. */
uint64_t nx_time_now_us(void) {
    return nx_gd32_now_us();
}
