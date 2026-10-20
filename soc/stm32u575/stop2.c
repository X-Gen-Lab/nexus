/**
 * \file            stop2.c
 * \brief           U575 Stop2 owner and bounded RTC wake drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "nexus/components/stm32u575_stop2.h"
#include "stm32u575_stop2_hw.h"
#include "stm32u575_system.h"

_Static_assert(RTC_IRQn == 2 && LSECSSD_IRQn == 125,
               "Stop2 uses the exact U575 interrupt extent and RTC vector");
#define WUT_BITS (RTC_CR_WUCKSEL | RTC_CR_WUTE | RTC_CR_WUTIE)
#define MSI_BITS                                                               \
    (RCC_CR_MSISON | RCC_CR_MSISRDY | RCC_CR_MSIKON | RCC_CR_MSIKRDY)
#define RTC_IRQ_BIT (1U << RTC_IRQn)
static nx_clock_domain_t s_domain;
/** \brief Only this one static owner can retain these hardware effects. */
static struct {
    nx_stm32u575_retained_time_storage_t* clock;
    uint32_t rtc_cr;
    uint32_t wutr;
    uint32_t power;
    uint32_t sleep_gate;
    uint32_t autonomous_gate;
    uint32_t msi;
    uint32_t effects;
} s_owner;

const void* nx_stm32u575_stop2_identity(void) {
    return &s_owner;
}
nx_clock_domain_t* nx_stm32u575_stop2_domain(void) {
    return &s_domain;
}

/** \brief No transition ever changes the incoming special-register masks. */
static bool context(bool masked) {
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U ||
        NX_STOP_CONTROL() != 0U)
        return false;
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    return masks.primask == (masked ? 1U : 0U) && masks.basepri == 0U &&
           masks.faultmask == 0U;
}

nx_result_t
nx_stm32u575_stop2_initialize(size_t capacity,
                              nx_stm32u575_retained_time_storage_t* clock) {
    if (!context(false))
        return NX_ERROR_CONTEXT;
    if (clock == NULL ||
        (uintptr_t)clock % _Alignof(nx_stm32u575_retained_time_storage_t) !=
            0U ||
        capacity == 0U)
        return NX_ERROR_INVALID;
    if (s_owner.clock != NULL)
        return NX_ERROR_STATE;
    nx_stm32u575_retained_time_snapshot_t sample;
    nx_result_t result = nx_stm32u575_retained_time_sample(clock, &sample);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32u575_retained_time_validate_wake(clock, &s_owner,
                                                      sample.generation);
    if (result != NX_SUCCESS)
        return result;
    result = nx_clock_domain_construct(&s_domain, capacity);
    if (result == NX_SUCCESS)
        s_owner.clock = clock;
    return result;
}

/** \brief Bounded observations are not a time or wake-latency estimate. */
static bool wait_bits(volatile const uint32_t* reg, uint32_t mask,
                      uint32_t expected, uint32_t limit) {
    while (limit-- != 0U) {
        if ((NX_STOP_READ(*reg) & mask) == expected)
            return true;
    }
    return false;
}
static bool write_checked(volatile uint32_t* reg, uint32_t value) {
    NX_STOP_WRITE(*reg, value);
    nx_arch_dsb();
    return NX_STOP_READ(*reg) == value;
}
/** \brief MSIS and bus dividers are retained; hardware wakes in Range4. */
static bool run_profile(void) {
    uint32_t selection = NX_STOP_READ(RCC->ICSCR1);
    uint32_t range =
        (selection & RCC_ICSCR1_MSIRGSEL) == 0U
            ? (NX_STOP_READ(RCC->CSR) & RCC_CSR_MSISSRANGE) >>
                  RCC_CSR_MSISSRANGE_Pos
            : (selection & RCC_ICSCR1_MSISRANGE) >> RCC_ICSCR1_MSISRANGE_Pos;
    uint32_t cr = NX_STOP_READ(RCC->CR);
    uint32_t vos = NX_STOP_READ(PWR->VOSR);
    return range == 4U && NX_STOP_READ(RCC->CFGR1) == 0U &&
           NX_STOP_READ(RCC->CFGR2) == 0U && NX_STOP_READ(RCC->CFGR3) == 0U &&
           (cr & ~MSI_BITS) == 0U &&
           (cr & (RCC_CR_MSISON | RCC_CR_MSISRDY)) ==
               (RCC_CR_MSISON | RCC_CR_MSISRDY) &&
           (vos & (PWR_VOSR_VOS | PWR_VOSR_VOSRDY | PWR_VOSR_BOOSTEN)) ==
               PWR_VOSR_VOSRDY;
}
/** \brief Never infer safe RTC access from an unknown wake clock. */
static bool wait_profile(uint32_t limit) {
    while (limit-- != 0U) {
        if (run_profile())
            return true;
    }
    return false;
}
/** \brief Hardware observations supplement the externally joined owners. */
static nx_result_t preflight(void) {
    if (NX_STOP_FLASH_KIB() != 2048U ||
        (NX_STOP_READ(FLASH->OPTR) &
         (FLASH_OPTR_TZEN | FLASH_OPTR_SWAP_BANK)) != 0U ||
        !run_profile() || NX_STOP_READ(PWR->CR1) != 0U ||
        NX_STOP_READ(PWR->CR2) != 0U || NX_STOP_READ(FLASH->ACR) != 0U ||
        NX_STOP_READ(ICACHE->CR) != 0U || NX_STOP_READ(DCACHE1->CR) != 0U ||
        NX_STOP_READ(MPU->CTRL) != 0U ||
        (NX_STOP_READ(FPU->FPCCR) & FPU_FPCCR_LSPACT_Msk) != 0U ||
        NX_STOP_READ(SCB->SCR) != 0U ||
        (NX_STOP_READ(CoreDebug->DHCSR) & 0xfU) != 0U ||
        NX_STOP_READ(CoreDebug->DEMCR) != 0U ||
        NX_STOP_READ(DBGMCU->CR) != 0U ||
        (NX_STOP_READ(RCC->BDCR) & (RCC_BDCR_LSION | RCC_BDCR_LSIRDY)) != 0U)
        return NX_ERROR_UNSUPPORTED;
    if (g_nx_stm32u575_system.started ||
        g_nx_stm32u575_system.remaining_effects != 0U)
        return NX_ERROR_BUSY;
    if (g_nx_stm32u575_system.timer_error != NX_SUCCESS)
        return NX_ERROR_STATE;
    if ((NX_STOP_READ(RCC->AHB1ENR) &
         ~(uint32_t)(RCC_AHB1ENR_FLASHEN | RCC_AHB1ENR_SRAM1EN)) != 0U ||
        (NX_STOP_READ(RCC->AHB2ENR1) &
         ~(uint32_t)(RCC_AHB2ENR1_SRAM2EN | RCC_AHB2ENR1_SRAM3EN)) != 0U ||
        NX_STOP_READ(RCC->AHB2ENR2) != 0U ||
        (NX_STOP_READ(RCC->AHB3ENR) &
         ~(uint32_t)(RCC_AHB3ENR_PWREN | RCC_AHB3ENR_SRAM4EN)) != 0U ||
        (NX_STOP_READ(RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0U ||
        NX_STOP_READ(RCC->APB1ENR1) != 0U ||
        NX_STOP_READ(RCC->APB1ENR2) != 0U || NX_STOP_READ(RCC->APB2ENR) != 0U ||
        NX_STOP_READ(RCC->APB3ENR) != RCC_APB3ENR_RTCAPBEN ||
        (NX_STOP_READ(RCC->SRDAMR) & ~(uint32_t)RCC_SRDAMR_RTCAPBAMEN) != 0U ||
        NX_STOP_READ(RCC->CCIPR1) != 0U || NX_STOP_READ(RCC->CCIPR2) != 0U ||
        NX_STOP_READ(RCC->CCIPR3) != 0U || NX_STOP_READ(RCC->CIER) != 0U ||
        (NX_STOP_READ(PWR->DBPR) & PWR_DBPR_DBP) == 0U ||
        NX_STOP_READ(PWR->WUCR1) != 0U || NX_STOP_READ(PWR->WUCR2) != 0U ||
        NX_STOP_READ(PWR->WUCR3) != 0U || NX_STOP_READ(PWR->WUSR) != 0U ||
        (NX_STOP_READ(FLASH->NSSR) & FLASH_NSSR_BSY) != 0U ||
        NX_STOP_READ(FLASH->NSCR) != FLASH_NSCR_LOCK ||
        (NX_STOP_READ(RTC->CR) & ~(uint32_t)RTC_CR_WUCKSEL) != RTC_CR_BYPSHAD ||
        NX_STOP_READ(RTC->SR) != 0U ||
        (NX_STOP_READ(RTC->ICSR) & RTC_ICSR_WUTWF) == 0U)
        return NX_ERROR_BUSY;
    volatile const uint32_t* const resets[] = {
        &RCC->AHB1RSTR,  &RCC->AHB2RSTR1, &RCC->AHB2RSTR2, &RCC->AHB3RSTR,
        &RCC->APB1RSTR1, &RCC->APB1RSTR2, &RCC->APB2RSTR,  &RCC->APB3RSTR};
    for (size_t i = 0U; i < sizeof(resets) / sizeof(resets[0]); ++i)
        if (NX_STOP_READ(*resets[i]) != 0U)
            return NX_ERROR_BUSY;
    if ((NX_STOP_READ(SysTick->CTRL) &
         (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk)) != 0U ||
        NX_STOP_READ(SysTick->LOAD) != 0U || NX_STOP_READ(SysTick->VAL) != 0U ||
        (NX_STOP_READ(SCB->ICSR) &
         (SCB_ICSR_VECTACTIVE_Msk | SCB_ICSR_VECTPENDING_Msk |
          SCB_ICSR_PENDSTSET_Msk | SCB_ICSR_PENDSVSET_Msk |
          SCB_ICSR_NMIPENDSET_Msk)) != 0U ||
        NX_STOP_READ(SCB->SHCSR) != 0U || NX_STOP_READ(EXTI->IMR1) != 0U ||
        NX_STOP_READ(EXTI->EMR1) != 0U || NX_STOP_READ(EXTI->RPR1) != 0U ||
        NX_STOP_READ(EXTI->FPR1) != 0U || NX_STOP_READ(EXTI->RTSR1) != 0U ||
        NX_STOP_READ(EXTI->FTSR1) != 0U)
        return NX_ERROR_BUSY;
    for (size_t word = 0U; word < 4U; ++word)
        if (NX_STOP_READ(NVIC->ISER[word]) != 0U ||
            NX_STOP_READ(NVIC->ISPR[word]) != 0U ||
            NX_STOP_READ(NVIC->IABR[word]) != 0U)
            return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

/** \brief Always finish the owned WPR interval, including rejected writes. */
static void unlock(void) {
    NX_STOP_WRITE(RTC->WPR, 0xcaU);
    NX_STOP_WRITE(RTC->WPR, 0x53U);
    nx_arch_dsb();
}
static void lock(void) {
    NX_STOP_WRITE(RTC->WPR, 0xffU);
    nx_arch_dsb();
}

/** \brief Program the WUT inside the caller's always-relocked WPR interval. */
static nx_result_t program_wakeup(uint32_t wake_ticks, uint32_t poll_limit) {
    if (!write_checked(&RTC->CR, s_owner.rtc_cr &
                                     ~(uint32_t)(RTC_CR_WUTE | RTC_CR_WUTIE)))
        return NX_ERROR_IO;
    if (!wait_bits(&RTC->ICSR, RTC_ICSR_WUTWF, RTC_ICSR_WUTWF, poll_limit))
        return NX_ERROR_TIMEOUT;
    if (!write_checked(&RTC->WUTR, wake_ticks - 1U))
        return NX_ERROR_IO;
    NX_STOP_WRITE(RTC->SCR, RTC_SCR_CWUTF);
    nx_arch_dsb();
    if ((NX_STOP_READ(RTC->SR) & RTC_SR_WUTF) != 0U ||
        !write_checked(&RTC->CR, (s_owner.rtc_cr & ~(uint32_t)WUT_BITS) |
                                     RTC_CR_WUTE | RTC_CR_WUTIE))
        return NX_ERROR_IO;
    if (!wait_bits(&RTC->ICSR, RTC_ICSR_WUTWF, 0U, poll_limit))
        return NX_ERROR_TIMEOUT;
    return NX_SUCCESS;
}

/** \brief No source/IRQ reference is released on a failed observation. */
static nx_result_t drain(uint32_t limit) {
    nx_result_t result = NX_SUCCESS;
    if (!write_checked(&SCB->SCR, 0U))
        result = NX_ERROR_IO;
    /* Unknown APB frequency cannot authorize any RTC register access.
     * Keep the source and gates owned. Core vector withdrawal is safe;
     * pending is cleared only after a later verified peripheral drain. */
    if (!wait_profile(limit)) {
        NX_STOP_WRITE(NVIC->ICER[0], RTC_IRQ_BIT);
        nx_arch_dsb();
        nx_arch_isb();
        return NX_ERROR_IO;
    }
    unlock();
    uint32_t cr =
        NX_STOP_READ(RTC->CR) & ~(uint32_t)(RTC_CR_WUTE | RTC_CR_WUTIE);
    if (!write_checked(&RTC->CR, cr))
        result = NX_ERROR_IO;
    else if (!wait_bits(&RTC->ICSR, RTC_ICSR_WUTWF, RTC_ICSR_WUTWF, limit))
        result = NX_ERROR_TIMEOUT;
    else {
        NX_STOP_WRITE(RTC->SCR, RTC_SCR_CWUTF);
        nx_arch_dsb();
        if ((NX_STOP_READ(RTC->SR) & RTC_SR_WUTF) != 0U ||
            !write_checked(&RTC->WUTR, s_owner.wutr) ||
            !write_checked(&RTC->CR,
                           (NX_STOP_READ(RTC->CR) & ~(uint32_t)WUT_BITS) |
                               (s_owner.rtc_cr & WUT_BITS)))
            result = NX_ERROR_IO;
    }
    lock();
    /* Withdraw the vector even after a failed source drain. Pending may
     * reassert until the source is quiet; either failure retains ownership. */
    NX_STOP_WRITE(NVIC->ICER[0], RTC_IRQ_BIT);
    NX_STOP_WRITE(NVIC->ICPR[0], RTC_IRQ_BIT);
    nx_arch_dsb();
    nx_arch_isb();
    if ((NX_STOP_READ(NVIC->ISER[0]) & RTC_IRQ_BIT) != 0U ||
        (NX_STOP_READ(NVIC->ISPR[0]) & RTC_IRQ_BIT) != 0U ||
        (NX_STOP_READ(NVIC->IABR[0]) & RTC_IRQ_BIT) != 0U)
        result = NX_ERROR_IO;
    if (result != NX_SUCCESS)
        return result;
    if (!run_profile())
        return NX_ERROR_IO;
    /* STOPKERWUCK=0 automatically enables MSIK. Its old enable state is
     * restored only after proving the retained MSIS4/bus/Range4 profile. */
    uint32_t clock = (NX_STOP_READ(RCC->CR) & ~(uint32_t)RCC_CR_MSIKON) |
                     (s_owner.msi & RCC_CR_MSIKON);
    NX_STOP_WRITE(RCC->CR, clock);
    nx_arch_dsb();
    if (!wait_bits(&RCC->CR, RCC_CR_MSIKON | RCC_CR_MSIKRDY,
                   s_owner.msi & (RCC_CR_MSIKON | RCC_CR_MSIKRDY), limit) ||
        !run_profile())
        return NX_ERROR_IO;
    if (!write_checked(&PWR->CR1, s_owner.power) ||
        !write_checked(&RCC->APB3SMENR, s_owner.sleep_gate) ||
        !write_checked(&RCC->SRDAMR, s_owner.autonomous_gate))
        return NX_ERROR_IO;
    s_owner.effects = 0U;
    return NX_SUCCESS;
}

static nx_result_t admission(nx_clock_barrier_t owners, uint32_t limit) {
    if (owners.domain != &s_domain || limit == 0U)
        return NX_ERROR_INVALID;
    if (!context(true))
        return NX_ERROR_CONTEXT;
    if (s_owner.clock == NULL)
        return NX_ERROR_STATE;
    return nx_clock_domain_quiescent(owners);
}

nx_stm32u575_stop2_result_t nx_stm32u575_stop2_enter(nx_clock_barrier_t owners,
                                                     uint32_t wake_ticks,
                                                     uint32_t poll_limit) {
    nx_stm32u575_stop2_result_t r = {0};
    r.remaining_effects = s_owner.effects;
    if (wake_ticks < 2U || wake_ticks > 65536U) {
        r.primary = NX_ERROR_INVALID;
        return r;
    }
    r.primary = admission(owners, poll_limit);
    if (r.primary != NX_SUCCESS)
        return r;
    if (s_owner.effects != 0U) {
        r.primary = NX_ERROR_STATE;
        r.remaining_effects = s_owner.effects;
        return r;
    }
    r.primary = preflight();
    if (r.primary != NX_SUCCESS)
        return r;
    r.primary = nx_stm32u575_retained_time_sample(s_owner.clock, &r.before);
    if (r.primary != NX_SUCCESS)
        return r;
    r.primary = nx_stm32u575_retained_time_validate_wake(
        s_owner.clock, &s_owner, r.before.generation);
    if (r.primary != NX_SUCCESS)
        return r;
    s_owner.rtc_cr = NX_STOP_READ(RTC->CR);
    s_owner.wutr = NX_STOP_READ(RTC->WUTR);
    s_owner.power = NX_STOP_READ(PWR->CR1);
    s_owner.sleep_gate = NX_STOP_READ(RCC->APB3SMENR);
    s_owner.autonomous_gate = NX_STOP_READ(RCC->SRDAMR);
    s_owner.msi = NX_STOP_READ(RCC->CR);
    s_owner.effects = 1U;
    if (!write_checked(&RCC->APB3SMENR,
                       s_owner.sleep_gate | RCC_APB3SMENR_RTCAPBSMEN) ||
        !write_checked(&RCC->SRDAMR,
                       s_owner.autonomous_gate | RCC_SRDAMR_RTCAPBAMEN)) {
        r.primary = NX_ERROR_IO;
        goto cleanup;
    }
    unlock();
    r.primary = program_wakeup(wake_ticks, poll_limit);
    lock();
    if (r.primary != NX_SUCCESS)
        goto cleanup;
    /* Do not clear NVIC pending after arming: an already asserted WUTF is
     * a legitimate immediate wake and must not be lost in an entry race. */
    NX_STOP_WRITE(NVIC->ISER[0], RTC_IRQ_BIT);
    nx_arch_dsb();
    if ((NX_STOP_READ(NVIC->ISER[0]) & RTC_IRQ_BIT) == 0U ||
        !write_checked(&PWR->CR1, PWR_CR1_LPMS_1) ||
        !write_checked(&SCB->SCR, SCB_SCR_SLEEPDEEP_Msk)) {
        r.primary = NX_ERROR_IO;
        goto cleanup;
    }
    r.wait_executed = true;
    NX_STOP_WAIT();
    bool shallow_restored = write_checked(&SCB->SCR, 0U);
    if (!shallow_restored || !wait_profile(poll_limit)) {
        r.primary = NX_ERROR_IO;
        goto cleanup;
    }
    r.wake_flag = (NX_STOP_READ(RTC->SR) & RTC_SR_WUTF) != 0U;
    r.primary = r.wake_flag && shallow_restored ? NX_SUCCESS : NX_ERROR_IO;
    nx_stm32u575_retained_time_snapshot_t after;
    nx_result_t sample =
        nx_stm32u575_retained_time_sample(s_owner.clock, &after);
    if (sample == NX_SUCCESS)
        sample = nx_stm32u575_retained_time_validate_wake(
            s_owner.clock, &s_owner, r.before.generation);
    if (sample == NX_SUCCESS && after.identity == r.before.identity &&
        after.generation == r.before.generation && after.now >= r.before.now)
        r.after = after;
    else
        r.primary = sample == NX_SUCCESS ? NX_ERROR_STATE : sample;
cleanup:
    r.cleanup = drain(poll_limit);
    r.remaining_effects = s_owner.effects;
    return r;
}

nx_stm32u575_stop2_result_t
nx_stm32u575_stop2_recover(nx_clock_barrier_t owners, uint32_t poll_limit) {
    nx_stm32u575_stop2_result_t r = {0};
    r.primary = admission(owners, poll_limit);
    if (r.primary == NX_SUCCESS && s_owner.effects != 0U)
        r.cleanup = drain(poll_limit);
    r.remaining_effects = s_owner.effects;
    return r;
}
