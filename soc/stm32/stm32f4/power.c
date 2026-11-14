/**
 * \file            power.c
 * \brief           F4 deep power with exact traits, drain and explicit timer
 * epochs \author          Nexus Team \version         1.0.0 \date 2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "nexus/core/fault.h"
#include "nexus/soc/stm32f4_power.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
#include "stm32f4_power_traits.h"

extern nx_stm32_system_t g_nx_stm32_system;

#ifndef NX_POWER_SCR
#define NX_POWER_SCR                SCB->SCR
#define NX_POWER_ICSR               SCB->ICSR
#define NX_POWER_TICK               SysTick->CTRL
#define NX_POWER_DMA1               DMA1_BASE
#define NX_POWER_DMA2               DMA2_BASE
#define NX_POWER_IRQ_ENABLED(index) NVIC->ISER[index]
#define NX_POWER_IRQ_ACTIVE(index)  NVIC->IABR[index]
#define NX_POWER_WAIT()             __WFI()
#define NX_POWER_WAIT_EVENT()       __WFE()
#define NX_POWER_EMR                EXTI->EMR
#define NX_POWER_RTSR               EXTI->RTSR
#define NX_POWER_IMR                EXTI->IMR
#define NX_POWER_FTSR               EXTI->FTSR
#endif
#ifndef NX_POWER_DEBUG
#define NX_POWER_DEBUG DBGMCU->CR
#endif
#ifndef NX_STM32_WRITE
#define NX_STM32_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif

/** \brief A replacement time domain cannot resume interrupted old borrowers.
 */
static _Noreturn void power_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}

/** \brief Reject MMIO, wrapping and misaligned call-owned storage. */
static bool storage(const void* value, size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)value;
    if (value == NULL || begin % alignment != 0U || bytes > UINTPTR_MAX - begin)
        return false;
    uintptr_t end = begin + bytes;
    return !(begin < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(begin < UINT32_C(0xf0000000) && end > UINT32_C(0xe0000000));
}

/** \brief All participating complete objects remain disjoint through return.
 */
static bool overlaps(const void* a, size_t a_bytes, const void* b,
                     size_t b_bytes) {
    return (uintptr_t)a < (uintptr_t)b + b_bytes &&
           (uintptr_t)b < (uintptr_t)a + a_bytes;
}

/** \brief The retained PWR observations belong to the live selected owner. */
static bool system_power_live(void) {
    const nx_stm32_system_t* s = &g_nx_stm32_system;
    return s->started && s->rcc != NULL && s->power != NULL &&
           (s->rcc->APB1ENR & RCC_APB1ENR_PWREN) != 0U;
}

/** \brief Even read-only observations use the unmasked privileged executor. */
static bool power_context(void) {
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           masks.primask == 0U && masks.basepri == 0U && masks.faultmask == 0U;
}

nx_result_t nx_stm32_power_wake_status(nx_stm32_power_wake_status_t* status) {
    if (!power_context())
        return NX_ERROR_CONTEXT;
    if (!storage(status, sizeof(*status),
                 _Alignof(nx_stm32_power_wake_status_t)) ||
        overlaps(status, sizeof(*status), &g_nx_stm32_system,
                 sizeof(g_nx_stm32_system)))
        return NX_ERROR_INVALID;
    if (!system_power_live())
        return NX_ERROR_STATE;
    uint32_t observed = g_nx_stm32_system.power->CSR;
    const nx_stm32_power_wake_status_t value = {
        .standby_observed = (observed & PWR_CSR_SBF) != 0U,
        .wake_pending = (observed & PWR_CSR_WUF) != 0U,
#if defined(STM32F479xx)
        .rtc_reset_required = (observed & (PWR_CSR_SBF | PWR_CSR_WUF)) != 0U
#else
        .rtc_reset_required = false
#endif
    };
    *status = value;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_power_standby_reset_prepare(uint32_t poll_limit) {
    if (!power_context())
        return NX_ERROR_CONTEXT;
    if (poll_limit == 0U)
        return NX_ERROR_INVALID;
#if defined(STM32F479xx)
    if (!system_power_live())
        return NX_ERROR_STATE;
    PWR_TypeDef* power = g_nx_stm32_system.power;
    uint32_t value = power->CR & ~(uint32_t)(PWR_CR_CSBF | PWR_CR_CWUF);
    NX_STM32_WRITE(&power->CR, value | PWR_CR_CSBF);
    nx_arch_dsb();
    for (uint32_t poll = 0U; poll < poll_limit; ++poll)
        if ((power->CSR & PWR_CSR_SBF) == 0U)
            return NX_SUCCESS;
    return NX_ERROR_IO;
#else
    return NX_ERROR_UNSUPPORTED;
#endif
}

/** \brief Inspect every hardware stream enable bit while its clock is live.
 */
static bool dma_idle(uintptr_t base) {
    for (uint32_t stream = 0U; stream < 8U; ++stream) {
        volatile const uint32_t* control =
            (volatile const uint32_t*)(base + 0x10U +
                                       (uintptr_t)stream * 0x18U);
        if ((*control & DMA_SxCR_EN) != 0U)
            return false;
    }
    return true;
}

/** \brief The fixed request storage never admits nonexistent IRQs or pads. */
static nx_result_t wake_selection(const nx_stm32_power_options_t* options) {
    for (uint32_t bank = 0U; bank < 4U; ++bank) {
        uint32_t first = bank * 32U;
        uint32_t valid = first >= NX_POWER_IRQ_COUNT ? 0U : UINT32_MAX;
        if (first < NX_POWER_IRQ_COUNT && NX_POWER_IRQ_COUNT - first < 32U)
            valid = (UINT32_C(1) << (NX_POWER_IRQ_COUNT - first)) - 1U;
        if ((options->wake_irqs[bank] & ~valid) != 0U)
            return NX_ERROR_INVALID;
    }
    if ((options->wake_pins &
         ~(uint8_t)(NX_STM32_WAKE_PIN_PA0 | NX_STM32_WAKE_PIN_PC13)) != 0U)
        return NX_ERROR_INVALID;
    if ((options->wake_pins & NX_STM32_WAKE_PIN_PC13) != 0U &&
        NX_POWER_WAKE_PC13 == 0U)
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}

/** \brief Only the selected normal-regulator STOP path is reviewed. */
static bool normal_stop_power(const PWR_TypeDef* power) {
#if defined(STM32F411xE) || defined(STM32F446xx) || defined(STM32F479xx)
    uint32_t foreign = PWR_CR_FPDS | PWR_CR_LPLVDS | PWR_CR_MRLVDS;
#if defined(PWR_CR_UDEN)
    foreign |= PWR_CR_UDEN;
#endif
#if defined(PWR_CR_FISSR)
    foreign |= PWR_CR_FISSR | PWR_CR_FMSSR;
#endif
#if defined(PWR_CR_ODEN)
    foreign |= PWR_CR_ODEN | PWR_CR_ODSWEN;
#endif
    return (power->CR & foreign) == 0U;
#else
    (void)power;
    return true;
#endif
}

/** \brief F446ZE PC13 edges must not disturb a live crystal LSE owner. */
static bool wake_pad_clock_safe(const nx_stm32_power_options_t* options) {
#if defined(STM32F446xx)
    const nx_stm32_system_t* system = &g_nx_stm32_system;
    bool pc13 = (options->wake_pins & NX_STM32_WAKE_PIN_PC13) != 0U ||
                (system->power->CSR & NX_POWER_WAKE_PC13) != 0U;
    return !options->standby || !pc13 ||
           (system->rcc->BDCR & (RCC_BDCR_LSEON | RCC_BDCR_LSEBYP)) !=
               RCC_BDCR_LSEON;
#else
    (void)options;
    return true;
#endif
}

/** \brief A declared autonomous RTC route is independent of the Core PLL. */
static bool rtc_wake_requested(const nx_stm32_power_options_t* options) {
    return options->wake_events != 0U || (options->wake_irqs[0] & 12U) != 0U ||
           (options->wake_irqs[1] & (UINT32_C(1) << 9U)) != 0U;
}

/** \brief NVIC enable alone cannot establish an armed EXTI wake source. */
static bool interrupt_routes_ready(const nx_stm32_power_options_t* options) {
    uint32_t edges = NX_POWER_IMR & (NX_POWER_RTSR | NX_POWER_FTSR);
    for (uint32_t line = 0U; line < 5U; ++line)
        if ((options->wake_irqs[0] & (UINT32_C(1) << (line + 6U))) != 0U &&
            (edges & (UINT32_C(1) << line)) == 0U)
            return false;
    if (((options->wake_irqs[0] & (UINT32_C(1) << 23U)) != 0U &&
         (edges & UINT32_C(0x3e0)) == 0U) ||
        ((options->wake_irqs[1] & (UINT32_C(1) << 8U)) != 0U &&
         (edges & UINT32_C(0xfc00)) == 0U))
        return false;
    uint32_t rtc =
        ((options->wake_irqs[0] & 4U) != 0U ? NX_STM32_WAKE_RTC_TAMPER : 0U) |
        ((options->wake_irqs[0] & 8U) != 0U ? NX_STM32_WAKE_RTC_TIMER : 0U) |
        ((options->wake_irqs[1] & (UINT32_C(1) << 9U)) != 0U
             ? NX_STM32_WAKE_RTC_ALARM
             : 0U);
    return (NX_POWER_IMR & NX_POWER_RTSR & rtc) == rtc;
}

/** \brief Check hardware and exact wake publishers before ending the epoch.
 */
static bool hardware_idle(const nx_stm32_power_options_t* options) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
#if defined(STM32F411xE) || defined(STM32F446xx) || defined(STM32F479xx)
    /* WFE low-power debug needs a different instruction sequence. The
     * selected release-mode mechanism never silently enables that mode. */
    uint32_t debug =
        options->standby ? DBGMCU_CR_DBG_STANDBY : DBGMCU_CR_DBG_STOP;
    if ((NX_POWER_DEBUG & debug) != 0U)
        return false;
#endif
    uint32_t allowed_ahb1 = NX_POWER_GPIO_GATES | NX_POWER_CCM_GATE |
                            NX_POWER_BACKUP_GATE | RCC_AHB1ENR_DMA1EN |
                            RCC_AHB1ENR_DMA2EN;
    if ((NX_POWER_SCR & SCB_SCR_SLEEPDEEP_Msk) != 0U ||
        (NX_POWER_TICK &
         (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk)) != 0U ||
        (NX_POWER_ICSR & SCB_ICSR_PENDSTSET_Msk) != 0U ||
        (rcc->APB1ENR &
         ~(uint32_t)(NEXUS_STM32_TIMEBASE_GATE | RCC_APB1ENR_PWREN)) != 0U ||
        rcc->APB2ENR != 0U || rcc->AHB2ENR != 0U || rcc->AHB3ENR != 0U ||
        (rcc->AHB1ENR & ~allowed_ahb1) != 0U ||
        ((rcc->AHB1ENR & RCC_AHB1ENR_DMA1EN) != 0U &&
         !dma_idle(NX_POWER_DMA1)) ||
        ((rcc->AHB1ENR & RCC_AHB1ENR_DMA2EN) != 0U && !dma_idle(NX_POWER_DMA2)))
        return false;
    uint32_t timer_index = (uint32_t)NEXUS_STM32_TIMEBASE_IRQ / 32U;
    uint32_t timer_bit = UINT32_C(1)
                         << ((uint32_t)NEXUS_STM32_TIMEBASE_IRQ % 32U);
    if ((options->wake_irqs[timer_index] & timer_bit) != 0U ||
        ((options->wake_irqs[0] | options->wake_irqs[1] |
          options->wake_irqs[2] | options->wake_irqs[3]) == 0U &&
         options->wake_events == 0U &&
         !(options->standby &&
           (options->wake_pins ||
            (g_nx_stm32_system.power->CSR & NX_POWER_WAKE_BITS) != 0U))))
        return false;
    /* Only EXTI0..15 and the independent RTC routes remain operational in
     * this drained STOP profile. A merely enabled peripheral IRQ is not a
     * wake source once its peripheral bus clocks have stopped. */
    const uint32_t wake_allowed[4] = {
        (UINT32_C(1) << 2U) | (UINT32_C(1) << 3U) | (UINT32_C(0x1f) << 6U) |
            (UINT32_C(1) << 23U),
        (UINT32_C(1) << 8U) | (UINT32_C(1) << 9U), 0U, 0U};
    for (uint32_t bank = 0U; bank < NX_POWER_IRQ_BANKS; ++bank)
        if ((options->wake_irqs[bank] & ~wake_allowed[bank]) != 0U)
            return false;
    if (!interrupt_routes_ready(options))
        return false;
    uint32_t events = NX_STM32_WAKE_RTC_ALARM | NX_STM32_WAKE_RTC_TAMPER |
                      NX_STM32_WAKE_RTC_TIMER;
    if ((options->wake_events & ~events) != 0U ||
        (NX_POWER_EMR & options->wake_events) != options->wake_events ||
        (NX_POWER_RTSR & options->wake_events) != options->wake_events)
        return false;
    if (rtc_wake_requested(options)) {
        uint32_t source = rcc->BDCR & RCC_BDCR_RTCSEL;
        if ((rcc->BDCR & RCC_BDCR_RTCEN) == 0U ||
            (source != RCC_BDCR_RTCSEL_0 && source != RCC_BDCR_RTCSEL_1) ||
            (source == RCC_BDCR_RTCSEL_0 &&
             (rcc->BDCR & (RCC_BDCR_LSEON | RCC_BDCR_LSERDY)) !=
                 (RCC_BDCR_LSEON | RCC_BDCR_LSERDY)) ||
            (source == RCC_BDCR_RTCSEL_1 &&
             (rcc->CSR & (RCC_CSR_LSION | RCC_CSR_LSIRDY)) !=
                 (RCC_CSR_LSION | RCC_CSR_LSIRDY)))
            return false;
    }
    for (uint32_t bank = 0U; bank < NX_POWER_IRQ_BANKS; ++bank) {
        uint32_t enabled = NX_POWER_IRQ_ENABLED(bank);
        if (bank == timer_index)
            enabled &= ~timer_bit;
        if (enabled != options->wake_irqs[bank] || NX_POWER_IRQ_ACTIVE(bank))
            return false;
    }
    return true;
}

/** \brief Establish each reversible deep-power register write by readback. */
static bool write_checked(volatile uint32_t* address, uint32_t value,
                          uint32_t mask) {
    NX_STM32_WRITE(address, value);
    nx_arch_dsb();
    return (*address & mask) == (value & mask);
}

/** \brief Replace only wake controls, preserving unrelated rc_w1 status. */
static bool write_wake(PWR_TypeDef* power, uint32_t selected) {
    uint32_t value = (power->CSR & ~(uint32_t)NX_POWER_WAKE_BITS) | selected;
    return write_checked(&power->CSR, nx_stm32_power_csr_control(value),
                         NX_POWER_WAKE_BITS);
}

/** \brief One power transaction consumes only explicit static owners. */
static nx_result_t enter(nx_clock_barrier_t barrier,
                         const nx_stm32_power_options_t* options,
                         const nx_rtc_port_t* rtc,
                         nx_rtc_standby_t* rtc_receipt,
                         nx_stm32_stop_receipt_t* receipt, bool require_rtc) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr())
        return NX_ERROR_CONTEXT;
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask || masks.basepri || masks.faultmask)
        return NX_ERROR_CONTEXT;
    nx_stm32_system_t* system = &g_nx_stm32_system;
    const void* rtc_storage = (const void*)(uintptr_t)rtc;
    bool has_rtc = rtc != NULL && rtc_receipt != NULL;
    if (!storage(options, sizeof(*options),
                 _Alignof(nx_stm32_power_options_t)) ||
        !storage(receipt, sizeof(*receipt),
                 _Alignof(nx_stm32_stop_receipt_t)) ||
        overlaps(options, sizeof(*options), receipt, sizeof(*receipt)) ||
        overlaps(options, sizeof(*options), system, sizeof(*system)) ||
        overlaps(receipt, sizeof(*receipt), system, sizeof(*system)) ||
        options->poll_limit == 0U ||
        (require_rtc && (!has_rtc || !options->standby)) ||
        ((rtc != NULL || rtc_receipt != NULL) && !has_rtc) ||
        (!options->standby && (options->discard_wakeup || options->wake_pins)))
        return NX_ERROR_INVALID;
    if (has_rtc &&
        (!storage(rtc_storage, sizeof(*rtc), _Alignof(nx_rtc_port_t)) ||
         !storage(rtc_receipt, sizeof(*rtc_receipt),
                  _Alignof(nx_rtc_standby_t)) ||
         overlaps(rtc_receipt, sizeof(*rtc_receipt), options,
                  sizeof(*options)) ||
         overlaps(rtc_receipt, sizeof(*rtc_receipt), receipt,
                  sizeof(*receipt)) ||
         overlaps(rtc_receipt, sizeof(*rtc_receipt), system, sizeof(*system)) ||
         overlaps(rtc_receipt, sizeof(*rtc_receipt), rtc_storage,
                  sizeof(*rtc)) ||
         overlaps(rtc_storage, sizeof(*rtc), receipt, sizeof(*receipt))))
        return NX_ERROR_INVALID;
    nx_result_t result = wake_selection(options);
    if (result != NX_SUCCESS)
        return result;
#if defined(NEXUS_CLOCK_SDIO_HS375) && NEXUS_CLOCK_SDIO_HS375
    /* Restore primitives do not qualify deep sleep for the closed RUN mode. */
    return NX_ERROR_UNSUPPORTED;
#endif
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    result = NX_ERROR_INVALID;
    if (!nx_clock_domain_span_valid(barrier.domain, options,
                                    sizeof(*options)) ||
        !nx_clock_domain_span_valid(barrier.domain, receipt,
                                    sizeof(*receipt)) ||
        (has_rtc && (!nx_clock_domain_span_valid(barrier.domain, rtc_storage,
                                                 sizeof(*rtc)) ||
                     !nx_clock_domain_span_valid(barrier.domain, rtc_receipt,
                                                 sizeof(*rtc_receipt)))))
        goto done;
    result = nx_clock_domain_quiescent(barrier);
    if (result != NX_SUCCESS)
        goto done;
    if (!system->started || system->time_epoch == UINT64_MAX ||
        !nx_stm32_clock_profile_matches(system->rcc, system->flash)) {
        result = NX_ERROR_STATE;
        goto done;
    }
    if (options->standby && !options->wake_pins &&
        (system->power->CSR & NX_POWER_WAKE_BITS) == 0U &&
        !rtc_wake_requested(options)) {
        result = NX_ERROR_UNSUPPORTED;
        goto done;
    }
    if (options->standby && !has_rtc &&
        ((system->rcc->BDCR & RCC_BDCR_RTCEN) != 0U ||
         rtc_wake_requested(options))) {
        result = NX_ERROR_STATE;
        goto done;
    }
#if defined(STM32F479xx)
    /* ES0321 2.2.13 requires product-authorized clear-SBF plus reset before
     * another RTC Standby. This call must never perform either implicitly. */
    if (options->standby && rtc_wake_requested(options) &&
        (system->power->CSR & (PWR_CSR_SBF | PWR_CSR_WUF)) != 0U) {
        result = NX_ERROR_UNSUPPORTED;
        goto done;
    }
#endif
    if (has_rtc) {
        result = nx_rtc_port_standby_validate(rtc, rtc_receipt);
        if (result != NX_SUCCESS)
            goto done;
    }
    if (!normal_stop_power(system->power) || !wake_pad_clock_safe(options) ||
        !hardware_idle(options) ||
        (options->standby && !options->discard_wakeup &&
         (system->power->CSR & PWR_CSR_WUF) != 0U)) {
        result = NX_ERROR_BUSY;
        goto done;
    }
    uint32_t saved_wakeup = system->power->CSR & NX_POWER_WAKE_BITS;
    uint32_t selected_wakeup =
        saved_wakeup | (((options->wake_pins & NX_STM32_WAKE_PIN_PA0) != 0U)
                            ? NX_POWER_WAKE_PA0
                            : 0U);
    if ((options->wake_pins & NX_STM32_WAKE_PIN_PC13) != 0U)
        selected_wakeup |= NX_POWER_WAKE_PC13;
    if (options->standby && saved_wakeup != 0U &&
        !write_wake(system->power, 0U)) {
        if (!write_wake(system->power, saved_wakeup))
            power_park();
        result = NX_ERROR_IO;
        goto done;
    }
    /* A wake arriving before the final source was disabled remains business
     * evidence unless this exact call explicitly authorizes its discard. */
    if (options->standby && !options->discard_wakeup &&
        (system->power->CSR & PWR_CSR_WUF) != 0U) {
        if (!write_wake(system->power, saved_wakeup))
            power_park();
        result = NX_ERROR_BUSY;
        goto done;
    }
    nx_stm32_stop_receipt_t observed = {.old_epoch = system->time_epoch,
                                        .before_us =
                                            nx_stm32_system_now(system)};
    if (nx_stm32_system_stop(system, options->poll_limit) != 0)
        power_park();
    bool owned_power = (system->rcc->APB1ENR & RCC_APB1ENR_PWREN) == 0U;
    if (!write_checked(&system->rcc->APB1ENR,
                       system->rcc->APB1ENR | RCC_APB1ENR_PWREN,
                       RCC_APB1ENR_PWREN))
        power_park();
    uint32_t saved_power = system->power->CR;
    uint32_t saved_scr = NX_POWER_SCR;
    uint32_t power = saved_power & ~(uint32_t)(PWR_CR_PDDS | PWR_CR_LPDS);
    if (options->standby)
        power |= PWR_CR_PDDS;
    else if (options->low_power_regulator)
        power |= PWR_CR_LPDS;
    if (!write_checked(&system->power->CR, power, PWR_CR_PDDS | PWR_CR_LPDS) ||
        !write_checked(&NX_POWER_SCR, saved_scr | SCB_SCR_SLEEPDEEP_Msk,
                       SCB_SCR_SLEEPDEEP_Msk))
        power_park();
    result = options->standby ? NX_ERROR_IO : NX_SUCCESS;
    if (options->standby) {
        /* ES0182 2.2.6: both RTC front ends and EWUP are disabled. WUF is
         * acknowledged before either owner rearms its original wake source. */
        NX_STM32_WRITE(&system->power->CR, power | PWR_CR_CWUF);
        nx_arch_dsb();
        if ((system->power->CSR & PWR_CSR_WUF) != 0U)
            goto restore;
        if (has_rtc) {
            result = nx_rtc_port_standby_commit(rtc, rtc_receipt);
            if (result != NX_SUCCESS)
                goto restore;
            result = NX_ERROR_IO;
        }
        if (selected_wakeup != 0U &&
            !write_wake(system->power, selected_wakeup))
            goto restore;
    }
    nx_arch_dsb();
    if (!options->standby && options->wake_events != 0U)
        NX_POWER_WAIT_EVENT();
    else
        NX_POWER_WAIT();
    observed.waited = true;
restore:
    /* No RTC/EXTI or newly asserted WUF is cleared after rearming. A spurious
     * return, including pre-WFI failure, still creates a new Core epoch. */
    if (!write_checked(&NX_POWER_SCR, saved_scr, UINT32_MAX) ||
        !write_checked(&system->power->CR, saved_power,
                       PWR_CR_PDDS | PWR_CR_LPDS))
        power_park();
    if (options->standby &&
        (system->power->CSR & NX_POWER_WAKE_BITS) != saved_wakeup &&
        !write_wake(system->power, saved_wakeup))
        power_park();
    if (owned_power &&
        !write_checked(&system->rcc->APB1ENR,
                       system->rcc->APB1ENR & ~(uint32_t)RCC_APB1ENR_PWREN,
                       RCC_APB1ENR_PWREN))
        power_park();
    nx_stm32_start_result_t started =
        nx_stm32_system_restore(system, options->poll_limit);
    if (started.primary || started.cleanup || !system->started)
        power_park();
    observed.new_epoch = system->time_epoch;
    observed.after_us = nx_stm32_system_now(system);
    if (has_rtc) {
        nx_result_t restored = nx_rtc_port_standby_restore(rtc, rtc_receipt);
        if (restored != NX_SUCCESS)
            result = restored;
    }
    *receipt = observed;
done:
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_stm32_power_enter(nx_clock_barrier_t barrier,
                                 const nx_stm32_power_options_t* options,
                                 nx_stm32_stop_receipt_t* receipt) {
    return enter(barrier, options, NULL, NULL, receipt, false);
}

nx_result_t nx_stm32_standby_enter(nx_clock_barrier_t barrier,
                                   const nx_stm32_power_options_t* options,
                                   const nx_rtc_port_t* rtc,
                                   nx_rtc_standby_t* rtc_receipt,
                                   nx_stm32_stop_receipt_t* receipt) {
    return enter(barrier, options, rtc, rtc_receipt, receipt, true);
}

nx_result_t nx_stm32_deadline_bridge(const nx_stm32_stop_receipt_t* receipt,
                                     uint64_t old_epoch, uint64_t deadline_us,
                                     uint64_t elapsed_min_us,
                                     uint64_t elapsed_max_us,
                                     nx_stm32_deadline_bridge_t* output) {
    if (!storage(receipt, sizeof(*receipt),
                 _Alignof(nx_stm32_stop_receipt_t)) ||
        !storage(output, sizeof(*output),
                 _Alignof(nx_stm32_deadline_bridge_t)) ||
        overlaps(receipt, sizeof(*receipt), output, sizeof(*output)) ||
        elapsed_min_us > elapsed_max_us)
        return NX_ERROR_INVALID;
    if (!receipt->waited || old_epoch == 0U ||
        old_epoch != receipt->old_epoch || receipt->new_epoch <= old_epoch)
        return NX_ERROR_STATE;
    nx_stm32_deadline_bridge_t translated = {.epoch = receipt->new_epoch};
    if (deadline_us == UINT64_MAX) {
        translated.earliest_us = translated.latest_us = UINT64_MAX;
    } else {
        uint64_t remaining = deadline_us > receipt->before_us
                                 ? deadline_us - receipt->before_us
                                 : 0U;
        uint64_t earliest =
            remaining > elapsed_max_us ? remaining - elapsed_max_us : 0U;
        uint64_t latest =
            remaining > elapsed_min_us ? remaining - elapsed_min_us : 0U;
        if (latest >= UINT64_MAX - receipt->after_us)
            return NX_ERROR_OVERFLOW;
        translated.earliest_us = receipt->after_us + earliest;
        translated.latest_us = receipt->after_us + latest;
    }
    *output = translated;
    return NX_SUCCESS;
}
