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
        (NX_GD303_READ(&RCU_APB1EN) &
         (RCU_APB1EN_TIMER1EN | RCU_APB1EN_TIMER4EN)) != 0U ||
        (NX_GD303_READ(&RCU_APB1RST) &
         (RCU_APB1RST_TIMER1RST | RCU_APB1RST_TIMER4RST)) != 0U ||
        !NX_GD303_IRQ_AVAILABLE(TIMER1_IRQn) ||
        !NX_GD303_IRQ_AVAILABLE(TIMER4_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_GD303_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_GD303_READ(reg) & mask) == (value & mask);
}
/** \brief Withdraw every source before the final vector proof. */
static bool stop_counter(uint32_t timer, int irq) {
    nx_result_t first = NX_GD303_IRQ_CONTROL(irq, false, 0U);
    bool valid = write_checked(&TIMER_DMAINTEN(timer), 0U, UINT32_MAX);
    valid = write_checked(&TIMER_CTL0(timer), 0U, UINT32_MAX) && valid;
    valid = write_checked(&TIMER_CTL1(timer), 0U, UINT32_MAX) && valid;
    valid = write_checked(&TIMER_SMCFG(timer), 0U, UINT32_MAX) && valid;
    NX_GD303_WRITE(&TIMER_INTF(timer), 0U);
    nx_arch_dsb();
    valid = NX_GD303_READ(&TIMER_INTF(timer)) == 0U && valid;
    nx_result_t final = NX_GD303_IRQ_CONTROL(irq, false, 0U);
    return first == NX_SUCCESS && final == NX_SUCCESS && valid &&
           NX_GD303_IRQ_AVAILABLE(irq);
}
nx_result_t nx_gd32f303_system_stop(nx_gd32f303_system_t* s) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    bool valid = true;
    if ((s->remaining_effects & 6U) != 0U)
        s->started = false;
    /* Remove low TRGO before the high slave; try both independent withdrawals
     * even when one fails. Neither gate returns while any source is retained.
     */
    if ((s->remaining_effects & 2U) != 0U) {
        bool low = stop_counter(TIMER1, TIMER1_IRQn);
        if (low)
            s->remaining_effects &= ~2U;
        valid = low && valid;
    }
    if ((s->remaining_effects & 4U) != 0U) {
        bool high = stop_counter(TIMER4, TIMER4_IRQn);
        if (high)
            s->remaining_effects &= ~4U;
        valid = high && valid;
    }
    if (!valid)
        return NX_ERROR_IO;
    if ((s->remaining_effects & 1U) != 0U) {
        uint32_t gates = RCU_APB1EN_TIMER1EN | RCU_APB1EN_TIMER4EN;
        if (!write_checked(&RCU_APB1EN, NX_GD303_READ(&RCU_APB1EN) & ~gates,
                           gates))
            return NX_ERROR_IO;
        s->remaining_effects &= ~1U;
    }
    s->core_hz = 0U;
    return NX_SUCCESS;
}
static bool counter_cold(uint32_t timer) {
    return NX_GD303_READ(&TIMER_CTL0(timer)) == 0U &&
           NX_GD303_READ(&TIMER_CTL1(timer)) == 0U &&
           NX_GD303_READ(&TIMER_SMCFG(timer)) == 0U &&
           NX_GD303_READ(&TIMER_CHCTL0(timer)) == 0U &&
           NX_GD303_READ(&TIMER_CHCTL1(timer)) == 0U &&
           NX_GD303_READ(&TIMER_CHCTL2(timer)) == 0U &&
           NX_GD303_READ(&TIMER_INTF(timer)) == 0U &&
           NX_GD303_READ(&TIMER_DMAINTEN(timer)) == 0U;
}
/** \brief CEN master selection keeps software UG local while both stop. */
static nx_result_t counter_epoch(uint32_t timer, uint32_t prescaler,
                                 uint32_t limit) {
    if (!write_checked(&TIMER_CTL1(timer), 1U << 4U, TIMER_CTL1_MMC) ||
        !write_checked(&TIMER_PSC(timer), prescaler, 0xffffU) ||
        !write_checked(&TIMER_CAR(timer), 0xffffU, 0xffffU) ||
        !write_checked(&TIMER_CNT(timer), 0U, 0xffffU))
        return NX_ERROR_IO;
    NX_GD303_WRITE(&TIMER_SWEVG(timer), TIMER_SWEVG_UPG);
    while (limit-- != 0U) {
        NX_GD303_POLL();
        if ((NX_GD303_READ(&TIMER_INTF(timer)) & TIMER_INTF_UPIF) != 0U) {
            return write_checked(&TIMER_INTF(timer), 0U, TIMER_INTF_UPIF) &&
                           write_checked(&TIMER_CTL1(timer), 0U, UINT32_MAX)
                       ? NX_SUCCESS
                       : NX_ERROR_IO;
        }
    }
    return NX_ERROR_TIMEOUT;
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
    uint32_t gates = RCU_APB1EN_TIMER1EN | RCU_APB1EN_TIMER4EN;
    if (!write_checked(&RCU_APB1EN, NX_GD303_READ(&RCU_APB1EN) | gates,
                       gates)) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    /* Do not reset a powered-down foreign controller to manufacture a lease. */
    if (!counter_cold(TIMER1) || !counter_cold(TIMER4)) {
        result.primary = NX_ERROR_BUSY;
        goto rollback;
    }
    s->remaining_effects |= 6U;
    s->overflow_us = 0U;
    s->timer_error = NX_SUCCESS;
    result.primary = counter_epoch(TIMER4, 0U, limit);
    if (result.primary == NX_SUCCESS)
        result.primary = counter_epoch(TIMER1, 7U, limit);
    if (result.primary != NX_SUCCESS)
        goto rollback;
    /* ManualRev3.4 Table16-1: TIMER4 ITI0 receives TIMER1_TRGO. External
     * clockmode0 SMC7 counts one high-word tick per low-word update. Arm the
     * slave before exporting low update TRGO; no later UG may inject a carry.
     */
    bool valid = write_checked(&TIMER_SMCFG(TIMER4), 7U, UINT32_MAX);
    valid = write_checked(&TIMER_DMAINTEN(TIMER4), TIMER_DMAINTEN_UPIE,
                          UINT32_MAX) &&
            valid;
    valid =
        write_checked(&TIMER_CTL0(TIMER4), TIMER_CTL0_CEN, UINT32_MAX) && valid;
    valid = write_checked(&TIMER_CTL1(TIMER1), 2U << 4U, UINT32_MAX) && valid;
    valid = write_checked(&TIMER_DMAINTEN(TIMER1), TIMER_DMAINTEN_UPIE,
                          UINT32_MAX) &&
            valid;
    valid =
        write_checked(&TIMER_CTL0(TIMER1), TIMER_CTL0_CEN, UINT32_MAX) && valid;
    if (!valid) {
        result.primary = NX_ERROR_IO;
        goto rollback;
    }
    result.primary = NX_GD303_IRQ_CONTROL(TIMER4_IRQn, true, 0U);
    if (result.primary == NX_SUCCESS)
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
/** \brief A broken high or low acknowledgement ends the whole epoch. */
static _Noreturn void timer_fault(nx_gd32f303_system_t* s) {
    s->started = false;
    s->timer_error = NX_ERROR_IO;
    (void)stop_counter(TIMER1, TIMER1_IRQn);
    (void)stop_counter(TIMER4, TIMER4_IRQn);
    timer_park();
}
void nx_gd32f303_system_timer_irq(nx_gd32f303_system_t* s) {
    if (s == NULL || !s->started ||
        (NX_GD303_READ(&TIMER_INTF(TIMER1)) & TIMER_INTF_UPIF) == 0U)
        return;
    NX_GD303_WRITE(&TIMER_INTF(TIMER1), ~(uint32_t)TIMER_INTF_UPIF);
    nx_arch_dsb();
    if ((NX_GD303_READ(&TIMER_INTF(TIMER1)) & TIMER_INTF_UPIF) != 0U)
        timer_fault(s);
    /* Hardware already counted every carry, including coalesced low IRQs. */
}
void nx_gd32f303_system_cascade_irq(nx_gd32f303_system_t* s) {
    if (s == NULL || !s->started ||
        (NX_GD303_READ(&TIMER_INTF(TIMER4)) & TIMER_INTF_UPIF) == 0U)
        return;
    NX_GD303_WRITE(&TIMER_INTF(TIMER4), ~(uint32_t)TIMER_INTF_UPIF);
    nx_arch_dsb();
    if ((NX_GD303_READ(&TIMER_INTF(TIMER4)) & TIMER_INTF_UPIF) != 0U)
        timer_fault(s);
    s->overflow_us += UINT64_C(4294967296);
}
/** \brief Re-read low after an observed carry so words share one epoch. */
static uint32_t cascade_count(void) {
    uint32_t high = NX_GD303_READ(&TIMER_CNT(TIMER4)) & 0xffffU;
    uint32_t low = NX_GD303_READ(&TIMER_CNT(TIMER1)) & 0xffffU;
    nx_arch_dsb();
    uint32_t next = NX_GD303_READ(&TIMER_CNT(TIMER4)) & 0xffffU;
    if (high != next) {
        high = next;
        low = NX_GD303_READ(&TIMER_CNT(TIMER1)) & 0xffffU;
    }
    return (high << 16U) | low;
}
uint64_t nx_gd32f303_system_now(nx_gd32f303_system_t* s) {
    if (s == NULL || !s->started || !nx_arch_is_privileged())
        return 0U;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    uint64_t high = s->overflow_us;
    uint32_t count = cascade_count();
    if ((NX_GD303_READ(&TIMER_INTF(TIMER4)) & TIMER_INTF_UPIF) != 0U) {
        high += UINT64_C(4294967296);
        count = cascade_count();
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
void TIMER4_IRQHandler(void) {
    nx_gd32f303_system_cascade_irq(&g_nx_gd32f303_system);
}

_Static_assert(sizeof(nx_gd32f303_platform_resources_t) == 20U,
               "exact cold receipt RAM budget");
_Static_assert((NX_GD32F303_OWN_AFIO | NX_GD32F303_OWN_USART0 |
                NX_GD32F303_OWN_SPI0 | NX_GD32F303_OWN_ADC0 |
                NX_GD32F303_OWN_TIMER2 | NX_GD32F303_OWN_DMA0) <= UINT8_MAX,
               "every ownership bit fits in its receipt");
_Static_assert(UINT16_MAX >= 0x6f3U, "validated GPIOA mask/latch fit");
static bool platform_selection_valid(const nx_gd32f303_platform_config_t* c) {
    if (c == NULL) {
        return false;
    }
    uint32_t required = c->exti_lines;
    if ((c->peripherals & NX_GD32F303_PLATFORM_UART) != 0U) {
        required |= 1536U;
    }
    if ((c->peripherals & NX_GD32F303_PLATFORM_SPI) != 0U) {
        required |= 240U;
    }
    if ((c->peripherals & NX_GD32F303_PLATFORM_ADC) != 0U) {
        required |= 1U;
    }
    if ((c->peripherals & NX_GD32F303_PLATFORM_PWM) != 0U) {
        required |= 64U;
    }
    return (required & ~(uint32_t)c->pins) == 0U &&
           (c->pins & ~UINT16_C(0x6f3)) == 0U && (c->exti_lines & ~3U) == 0U &&
           (c->peripherals & ~127U) == 0U &&
           ((c->peripherals & NX_GD32F303_PLATFORM_DMA_TX) == 0U ||
            (c->peripherals & NX_GD32F303_PLATFORM_UART) != 0U) &&
           ((c->peripherals & NX_GD32F303_PLATFORM_DMA_RX) == 0U ||
            (c->peripherals & NX_GD32F303_PLATFORM_DMA_TX) != 0U);
}
nx_result_t
nx_gd32f303_platform_preflight(const nx_gd32f303_platform_config_t* c) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!platform_selection_valid(c)) {
        return NX_ERROR_INVALID;
    }
    uint8_t selected = c->peripherals;
    if (((selected & NX_GD32F303_PLATFORM_DMA_TX) != 0U &&
         !NX_GD303_IRQ_AVAILABLE(DMA0_Channel3_IRQn)) ||
        ((selected & NX_GD32F303_PLATFORM_DMA_RX) != 0U &&
         !NX_GD303_IRQ_AVAILABLE(DMA0_Channel4_IRQn))) {
        return NX_ERROR_BUSY;
    }
    if (c->pins != 0U) {
        nx_result_t pins = nx_gd32f303_pins_preflight(c->pins);
        if (pins != NX_SUCCESS) {
            return pins;
        }
    }
    uint32_t apb2 = NX_GD303_READ(&RCU_APB2EN);
    if ((selected & NX_GD32F303_PLATFORM_UART) != 0U &&
        ((apb2 & RCU_APB2EN_USART0EN) != 0U ||
         !NX_GD303_IRQ_AVAILABLE(USART0_IRQn))) {
        return NX_ERROR_BUSY;
    }
    if ((selected & NX_GD32F303_PLATFORM_SPI) != 0U &&
        ((apb2 & RCU_APB2EN_SPI0EN) != 0U ||
         !NX_GD303_IRQ_AVAILABLE(SPI0_IRQn))) {
        return NX_ERROR_BUSY;
    }
    if ((selected & NX_GD32F303_PLATFORM_ADC) != 0U &&
        ((apb2 & RCU_APB2EN_ADC0EN) != 0U ||
         (NX_GD303_READ(&RCU_APB2RST) & RCU_APB2RST_ADC0RST) != 0U ||
         (NX_GD303_READ(&RCU_CFG0) & (RCU_CFG0_ADCPSC | RCU_CFG0_ADCPSC_2)) !=
             0U ||
         (NX_GD303_READ(&RCU_CFG1) & RCU_CFG1_ADCPSC_3) != 0U ||
         !NX_GD303_IRQ_AVAILABLE(ADC0_1_IRQn))) {
        return NX_ERROR_BUSY;
    }
    if ((selected & NX_GD32F303_PLATFORM_PWM) != 0U &&
        ((NX_GD303_READ(&RCU_APB1EN) & RCU_APB1EN_TIMER2EN) != 0U ||
         (NX_GD303_READ(&RCU_APB1RST) & RCU_APB1RST_TIMER2RST) != 0U ||
         !NX_GD303_IRQ_AVAILABLE(TIMER2_IRQn) ||
         ((apb2 & RCU_APB2EN_AFEN) != 0U &&
          (NX_GD303_READ(&AFIO_PCF0) & AFIO_PCF0_TIMER2_REMAP) != 0U))) {
        return NX_ERROR_BUSY;
    }
    if ((selected & NX_GD32F303_PLATFORM_FLASH) != 0U &&
        !NX_GD303_IRQ_AVAILABLE(FMC_IRQn)) {
        return NX_ERROR_BUSY;
    }
    for (unsigned line = 0U; line < 2U; ++line) {
        uint32_t mask = 1U << line;
        if ((c->exti_lines & mask) != 0U &&
            (((NX_GD303_READ(&EXTI_INTEN) | NX_GD303_READ(&EXTI_EVEN) |
               NX_GD303_READ(&EXTI_RTEN) | NX_GD303_READ(&EXTI_FTEN) |
               NX_GD303_READ(&EXTI_PD)) &
              mask) != 0U ||
             !NX_GD303_IRQ_AVAILABLE((int)EXTI0_IRQn + (int)line))) {
            return NX_ERROR_BUSY;
        }
    }
    return nx_gd32f303_system_preflight(&g_nx_gd32f303_system);
}
/* The private byte keeps matching low APB2 bits and one shifted high byte.
 * Compile-time checks bind this lossless representation to the exact SDK. */
_Static_assert((RCU_APB2EN_AFEN) == (NX_GD32F303_OWN_AFIO),
               "exact APB2 low receipt bits");
_Static_assert(((RCU_APB2EN_USART0EN | RCU_APB2EN_SPI0EN | RCU_APB2EN_ADC0EN) >>
                8U) == (NX_GD32F303_OWN_USART0 | NX_GD32F303_OWN_SPI0 |
                        NX_GD32F303_OWN_ADC0),
               "exact APB2 high receipt bits");
static uint8_t platform_apb2_receipt(uint32_t gates) {
    const uint32_t low = NX_GD32F303_OWN_AFIO;
    const uint32_t high =
        NX_GD32F303_OWN_USART0 | NX_GD32F303_OWN_SPI0 | NX_GD32F303_OWN_ADC0;
    return (uint8_t)((gates & low) | ((gates >> 8U) & high));
}
static uint32_t platform_apb2_gates(uint8_t owned) {
    const uint32_t low = NX_GD32F303_OWN_AFIO;
    const uint32_t high =
        NX_GD32F303_OWN_USART0 | NX_GD32F303_OWN_SPI0 | NX_GD32F303_OWN_ADC0;
    return ((uint32_t)owned & low) | (((uint32_t)owned & high) << 8U);
}
nx_result_t
nx_gd32f303_platform_resources_acquire(nx_gd32f303_platform_resources_t* p,
                                       const nx_gd32f303_platform_config_t* c) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL || !platform_selection_valid(c)) {
        return NX_ERROR_INVALID;
    }
    if (p->pins.retained || p->owned != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!g_nx_gd32f303_system.started ||
        g_nx_gd32f303_system.core_hz != 8000000U) {
        return NX_ERROR_STATE;
    }
    uint8_t selected = c->peripherals;
    if ((selected & NX_GD32F303_PLATFORM_DMA_TX) != 0U) {
        if ((NX_GD303_READ(&RCU_AHBEN) & RCU_AHBEN_DMA0EN) == 0U) {
            p->owned |= NX_GD32F303_OWN_DMA0;
        }
        if (!write_checked(&RCU_AHBEN,
                           NX_GD303_READ(&RCU_AHBEN) | RCU_AHBEN_DMA0EN,
                           RCU_AHBEN_DMA0EN)) {
            return NX_ERROR_IO;
        }
    }
    if (c->pins != 0U) {
        nx_result_t pins = nx_gd32f303_pins_acquire(&p->pins, c->pins);
        if (pins != NX_SUCCESS) {
            return pins;
        }
    }
    uint32_t apb2 = 0U;
    if ((selected & NX_GD32F303_PLATFORM_UART) != 0U) {
        apb2 |= RCU_APB2EN_USART0EN;
    }
    if ((selected & NX_GD32F303_PLATFORM_SPI) != 0U) {
        apb2 |= RCU_APB2EN_SPI0EN;
    }
    if ((selected & NX_GD32F303_PLATFORM_ADC) != 0U) {
        apb2 |= RCU_APB2EN_ADC0EN;
    }
    if (apb2 != 0U || (selected & NX_GD32F303_PLATFORM_PWM) != 0U ||
        c->exti_lines != 0U) {
        apb2 |= RCU_APB2EN_AFEN;
    }
    if (apb2 != 0U) {
        p->owned |= platform_apb2_receipt(~NX_GD303_READ(&RCU_APB2EN) & apb2);
        if (!write_checked(&RCU_APB2EN, NX_GD303_READ(&RCU_APB2EN) | apb2,
                           apb2)) {
            return NX_ERROR_IO;
        }
    }
    if ((selected & NX_GD32F303_PLATFORM_PWM) != 0U) {
        if ((NX_GD303_READ(&RCU_APB1EN) & RCU_APB1EN_TIMER2EN) == 0U) {
            p->owned |= NX_GD32F303_OWN_TIMER2;
        }
        if (!write_checked(&RCU_APB1EN,
                           NX_GD303_READ(&RCU_APB1EN) | RCU_APB1EN_TIMER2EN,
                           RCU_APB1EN_TIMER2EN) ||
            (NX_GD303_READ(&AFIO_PCF0) & AFIO_PCF0_TIMER2_REMAP) != 0U) {
            return NX_ERROR_IO;
        }
    }
    return NX_SUCCESS;
}
nx_result_t
nx_gd32f303_platform_resources_release(nx_gd32f303_platform_resources_t* p) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (p == NULL) {
        return NX_ERROR_INVALID;
    }
    if ((p->owned & NX_GD32F303_OWN_DMA0) != 0U) {
        if (!write_checked(&RCU_AHBEN,
                           NX_GD303_READ(&RCU_AHBEN) &
                               ~(uint32_t)RCU_AHBEN_DMA0EN,
                           RCU_AHBEN_DMA0EN)) {
            return NX_ERROR_IO;
        }
        p->owned &= (uint8_t)~NX_GD32F303_OWN_DMA0;
    }
    nx_result_t pins = nx_gd32f303_pins_release(&p->pins);
    if (pins != NX_SUCCESS) {
        return pins;
    }
    uint32_t apb2 = platform_apb2_gates(p->owned);
    if (apb2 != 0U) {
        if (!write_checked(&RCU_APB2EN, NX_GD303_READ(&RCU_APB2EN) & ~apb2,
                           apb2)) {
            return NX_ERROR_IO;
        }
        p->owned &= (uint8_t) ~(NX_GD32F303_OWN_AFIO | NX_GD32F303_OWN_USART0 |
                                NX_GD32F303_OWN_SPI0 | NX_GD32F303_OWN_ADC0);
    }
    if ((p->owned & NX_GD32F303_OWN_TIMER2) != 0U) {
        if (!write_checked(&RCU_APB1EN,
                           NX_GD303_READ(&RCU_APB1EN) &
                               ~(uint32_t)RCU_APB1EN_TIMER2EN,
                           RCU_APB1EN_TIMER2EN)) {
            return NX_ERROR_IO;
        }
        p->owned &= (uint8_t)~NX_GD32F303_OWN_TIMER2;
    }
    return NX_SUCCESS;
}
