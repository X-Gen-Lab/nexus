/**
 * \file            rtc.c
 * \brief           F407 explicit backup calendar and polling Alarm A owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "rtc_lptim_internal.h"
#include "stm32f407_rtc.h"
#include "stm32f4xx_ll_rtc.h"
#ifndef NX_F407_RTC
#define NX_F407_RTC       RTC
#define NX_F407_RTC_RCC   RCC
#define NX_F407_RTC_PWR   PWR
#define NX_F407_RTC_EXTI  EXTI
#define NX_F407_RTC_GPIOC GPIOC
#endif
#ifndef NX_F407_RTC_SRAM
#define NX_F407_RTC_SRAM ((void*)BKPSRAM_BASE)
#endif
#ifndef NX_F407_RTC_READ
#define NX_F407_RTC_READ(p)     READ_REG(*(p))
#define NX_F407_RTC_WRITE(p, v) WRITE_REG(*(p), (v))
#endif
#ifndef NX_F407_RTC_IRQ_QUIET
static bool vector_quiet(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&
           NVIC_GetActive(irq) == 0U;
}
#define NX_F407_RTC_IRQ_QUIET()                                                \
    (vector_quiet(RTC_Alarm_IRQn) && vector_quiet(TAMP_STAMP_IRQn) &&          \
     vector_quiet(RTC_WKUP_IRQn))
#endif
extern nx_stm32_system_t g_nx_stm32_system;
nx_time_us_t nx_stm32_rtc_clock_read(void* context) {
    return context == &g_nx_stm32_system
               ? nx_stm32_system_now(&g_nx_stm32_system)
               : 0U;
}

_Static_assert(offsetof(RTC_TypeDef, ISR) == 0x0cU &&
                   offsetof(RTC_TypeDef, ALRMASSR) == 0x44U &&
                   RTC_Alarm_IRQn == 41 && TAMP_STAMP_IRQn == 2 &&
                   RTC_WKUP_IRQn == 3 && LL_RTC_ALMA_MASK_NONE == 0U,
               "F407 reviewed RTC register profile changed");
enum {
    IDLE,
    SOURCE_OFF,
    OFF_READY,
    PROGRAM,
    ARM,
    RUN,
    WITHDRAW,
    DRAIN_READY,
    CLEAR,
    RESTORE
};
enum {
    EFFECT_POWER = 1U,
    EFFECT_ACCESS = 2U,
    EFFECT_LSE = 4U,
    EFFECT_SOURCE = 8U,
    EFFECT_RTC = 16U,
    EFFECT_CALENDAR = 32U,
    EFFECT_GPIOC_INSPECTION = 64U,
    EFFECT_QUIET_PC13 = 128U
};
#define PROFILE UINT32_C(0x007f00ff)
#define BACKUP_MASK                                                            \
    (RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_LSEBYP | RCC_BDCR_RTCSEL |    \
     RCC_BDCR_RTCEN | RCC_BDCR_BDRST)
#define BACKUP_LIVE                                                            \
    (RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCEN)
#define FOREIGN_STATUS                                                         \
    (RTC_ISR_RECALPF | RTC_ISR_TAMP1F | RTC_ISR_TAMP2F | RTC_ISR_TSOVF |       \
     RTC_ISR_TSF | RTC_ISR_WUTF | RTC_ISR_ALRBF | RTC_ISR_SHPF)
#define AUX_STATUS                                                             \
    (RTC_ISR_ALRBF | RTC_ISR_WUTF | RTC_ISR_TSF | RTC_ISR_TSOVF |              \
     RTC_ISR_TAMP1F | RTC_ISR_TAMP2F)
#define WAKE_LINES ((1U << 17U) | (1U << 21U) | (1U << 22U))
static nx_result_t auxiliary_close(nx_stm32_rtc_state_t* s);
static bool inspection_clock(nx_stm32_rtc_state_t* s);
static bool restore_inspection(nx_stm32_rtc_state_t* s);
static const nx_rtc_aux_ops_t auxiliary_ops;
/** \brief Backup access belongs to one privileged unmasked task executor. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool owner(const nx_stm32_rtc_state_t* s) {
    return nx_time_span(s, sizeof(*s), _Alignof(nx_stm32_rtc_state_t));
}
/** \brief Known private/configuration spans cannot be used as public buffers.
 */
static bool outside_private(const nx_stm32_rtc_state_t* s, const void* p,
                            size_t length, size_t alignment) {
    if (!nx_time_span(p, length, alignment))
        return false;
#define OVERLAP(base, size) nx_time_overlap(p, length, (base), (size))
    bool good = !OVERLAP(s, sizeof(*s)) &&
                !OVERLAP(NX_F407_RTC, sizeof(RTC_TypeDef)) &&
                !OVERLAP(NX_F407_RTC_RCC, sizeof(RCC_TypeDef)) &&
                !OVERLAP(NX_F407_RTC_PWR, sizeof(PWR_TypeDef)) &&
                !OVERLAP(NX_F407_RTC_EXTI, sizeof(EXTI_TypeDef)) &&
                !OVERLAP(NX_F407_RTC_GPIOC, sizeof(GPIO_TypeDef)) &&
                !OVERLAP(NX_F407_RTC_SRAM, 4096U) &&
                !OVERLAP(&g_nx_stm32_system, sizeof(g_nx_stm32_system)) &&
                !OVERLAP(&auxiliary_ops, sizeof(auxiliary_ops)) &&
                !OVERLAP(&nx_stm32_rtc_ops, sizeof(nx_stm32_rtc_ops));
#undef OVERLAP
    return good;
}
static bool external(const nx_stm32_rtc_state_t* s, const void* p,
                     size_t length, size_t alignment, bool protect_active) {
    return outside_private(s, p, length, alignment) &&
           (s->config == NULL ||
            !nx_time_overlap(p, length, s->config, sizeof(*s->config))) &&
           (!protect_active || s->active == NULL ||
            !nx_time_overlap(p, length, s->active, sizeof(*s->active))) &&
           (s->sram_lease == NULL ||
            !nx_time_overlap(p, length, s->sram_lease, sizeof(*s->sram_lease)));
}
static bool quiet(void) {
    const uint32_t line = WAKE_LINES;
    return NX_F407_RTC_IRQ_QUIET() &&
           ((NX_F407_RTC_READ(&NX_F407_RTC_EXTI->IMR) |
             NX_F407_RTC_READ(&NX_F407_RTC_EXTI->EMR) |
             NX_F407_RTC_READ(&NX_F407_RTC_EXTI->RTSR) |
             NX_F407_RTC_READ(&NX_F407_RTC_EXTI->FTSR) |
             NX_F407_RTC_READ(&NX_F407_RTC_EXTI->PR)) &
            line) == 0U;
}
static bool system_live(void) {
    return g_nx_stm32_system.started &&
           g_nx_stm32_system.timer_error == NX_SUCCESS;
}
/** \brief INITS depends on year != 00; it is never a validity oracle. */
static bool live_source(const nx_stm32_rtc_state_t* s, bool bypass) {
    uint32_t events = s->wake_lines;
    return system_live() && NX_F407_RTC_IRQ_QUIET() &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->IMR) & WAKE_LINES) == 0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->FTSR) & WAKE_LINES) == 0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->EMR) & WAKE_LINES) == events &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->RTSR) & WAKE_LINES) == events &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->PR) & (WAKE_LINES & ~events)) ==
               0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC_RCC->APB1ENR) & RCC_APB1ENR_PWREN) &&
           (NX_F407_RTC_READ(&NX_F407_RTC_PWR->CR) & PWR_CR_DBP) &&
           (NX_F407_RTC_READ(&NX_F407_RTC_RCC->BDCR) & BACKUP_MASK) ==
               (BACKUP_LIVE | (bypass ? RCC_BDCR_LSEBYP : 0U)) &&
           (NX_F407_RTC_READ(&NX_F407_RTC->CR) & ~(uint32_t)RTC_CR_ALRAE) ==
               (RTC_CR_BYPSHAD | s->auxiliary_cr) &&
           NX_F407_RTC_READ(&NX_F407_RTC->PRER) == PROFILE &&
           NX_F407_RTC_READ(&NX_F407_RTC->CALR) == s->auxiliary_calr &&
           NX_F407_RTC_READ(&NX_F407_RTC->CALIBR) == s->auxiliary_calibr &&
           NX_F407_RTC_READ(&NX_F407_RTC->TAFCR) == s->auxiliary_tafcr &&
           (NX_F407_RTC_READ(&NX_F407_RTC->ISR) &
            (RTC_ISR_SHPF | RTC_ISR_INIT | RTC_ISR_INITF)) == 0U;
}
/** \brief A live owner retains its exact authored oscillator personality. */
static bool live(const nx_stm32_rtc_state_t* s) {
    return live_source(s, s->config != NULL && s->config->lse_bypass);
}
static bool bcd(uint32_t value, uint8_t* out) {
    if ((value & 15U) > 9U || ((value >> 4U) & 15U) > 9U)
        return false;
    *out = (uint8_t)((value >> 4U) * 10U + (value & 15U));
    return true;
}
/** \brief BYPSHAD requires two equal complete SSR/TR/DR observations. */
static nx_result_t calendar(nx_rtc_calendar_t* out) {
    for (unsigned i = 0; i < 8U; ++i) {
        uint32_t sub = NX_F407_RTC_READ(&NX_F407_RTC->SSR);
        uint32_t time = NX_F407_RTC_READ(&NX_F407_RTC->TR);
        uint32_t date = NX_F407_RTC_READ(&NX_F407_RTC->DR);
        uint32_t sub2 = NX_F407_RTC_READ(&NX_F407_RTC->SSR);
        uint32_t time2 = NX_F407_RTC_READ(&NX_F407_RTC->TR);
        uint32_t date2 = NX_F407_RTC_READ(&NX_F407_RTC->DR);
        if (sub != sub2 || time != time2 || date != date2)
            continue;
        nx_rtc_calendar_t c = {0};
        c.weekday = (uint8_t)((date >> 13U) & 7U);
        if (sub > 255U || (time & ~UINT32_C(0x003f7f7f)) ||
            (date & ~UINT32_C(0x00ffff3f)) ||
            !bcd((time >> 16U) & 0x3fU, &c.hour) ||
            !bcd((time >> 8U) & 0x7fU, &c.minute) ||
            !bcd(time & 0x7fU, &c.second) ||
            !bcd((date >> 16U) & 0xffU, &c.year) ||
            !bcd((date >> 8U) & 0x1fU, &c.month) ||
            !bcd(date & 0x3fU, &c.day) ||
            nx_rtc_calendar_validate(&c) != NX_SUCCESS)
            return NX_ERROR_STATE;
        *out = c;
        return NX_SUCCESS;
    }
    return NX_ERROR_BUSY;
}
/** \brief Official WRITE_REG primitive with mandatory full ownership readback.
 * \note LL/HAL blocking initialization uses independent timeout assumptions.
 *       Nexus keeps bounded INIT and CDC control here; no register script or
 *       HAL tick is used. WPR is unreadable, so locked handoff is required.
 */
static void unlock(void) {
    LL_RTC_DisableWriteProtection(NX_F407_RTC);
    nx_arch_dsb();
}
static void lock(void) {
    LL_RTC_EnableWriteProtection(NX_F407_RTC);
    nx_arch_dsb();
}
static bool checked(volatile uint32_t* p, uint32_t value) {
    unlock();
    if (p == &NX_F407_RTC->TR)
        LL_RTC_TIME_Config(NX_F407_RTC, LL_RTC_TIME_FORMAT_AM_OR_24,
                           (value >> 16U) & 0x3fU, (value >> 8U) & 0x7fU,
                           value & 0x7fU);
    else if (p == &NX_F407_RTC->DR)
        LL_RTC_DATE_Config(NX_F407_RTC, (value >> 13U) & 7U, value & 0x3fU,
                           (value >> 8U) & 0x1fU, (value >> 16U) & 0xffU);
    else
        NX_F407_RTC_WRITE(p, value);
    nx_arch_dsb();
    bool good = NX_F407_RTC_READ(p) == value;
    lock();
    return good;
}
/** \brief F407 ISR is W0C; preserve every unrelated flag and INIT state. */
static void status_write(bool init, bool clear_alarm) {
    unlock();
    if (clear_alarm)
        LL_RTC_ClearFlag_ALRA(NX_F407_RTC);
    else if (init)
        LL_RTC_EnableInitMode(NX_F407_RTC);
    else
        LL_RTC_DisableInitMode(NX_F407_RTC);
    lock();
}
static nx_result_t wait_bits(const nx_stm32_rtc_config_t* c,
                             volatile uint32_t* p, uint32_t mask,
                             uint32_t expected, nx_time_us_t deadline) {
    for (uint32_t i = 0; i < c->poll_limit; ++i) {
        if ((NX_F407_RTC_READ(p) & mask) == expected)
            return NX_SUCCESS;
        if (!system_live())
            return NX_ERROR_STATE;
        if (nx_deadline_expired(deadline, c->clock.read(c->clock.context)))
            break;
    }
    return NX_ERROR_TIMEOUT;
}
/** \brief ES0182 2.9.4 requires actual shadow synchronization before INIT.
 * \note Every path uses the same finite budget, including coarse calibration.
 *       Restore the original bypass personality even when RSF never arrives.
 */
static nx_result_t synchronize_init(nx_stm32_rtc_state_t* s,
                                    nx_time_us_t deadline) {
    uint32_t controls = NX_F407_RTC_READ(&NX_F407_RTC->CR);
    if (!checked(&NX_F407_RTC->CR, controls & ~(uint32_t)RTC_CR_BYPSHAD))
        return NX_ERROR_IO;
    unlock();
    LL_RTC_ClearFlag_RS(NX_F407_RTC);
    nx_arch_dsb();
    lock();
    nx_result_t result = wait_bits(s->config, &NX_F407_RTC->ISR, RTC_ISR_RSF,
                                   RTC_ISR_RSF, deadline);
    return checked(&NX_F407_RTC->CR, controls) ? result : NX_ERROR_IO;
}
/** \brief Programming starts only after admission and INITF. No rollback date.
 */
static nx_result_t program_calendar(nx_stm32_rtc_state_t* s,
                                    const nx_rtc_calendar_t* c,
                                    nx_time_us_t deadline, bool provision) {
    RTC_TypeDef* p = NX_F407_RTC;
    nx_result_t r = synchronize_init(s, deadline);
    if (r != NX_SUCCESS)
        return r;
    status_write(true, false);
    r = wait_bits(s->config, &p->ISR, RTC_ISR_INITF, RTC_ISR_INITF, deadline);
    if (r != NX_SUCCESS) {
        /* A bounded readiness failure must not deliberately leave INIT set.
         * The caller still retains a tainted ownership receipt; this best
         * effort abort is not proof that a partial calendar can be reused. */
        status_write(false, false);
        return r;
    }
    bool good = true;
    if (provision) {
        /* RM0090 initialization requires synchronous then asynchronous fields
         * in separate writes, even when reset values already match. */
        unlock();
        LL_RTC_SetSynchPrescaler(p, 255U);
        nx_arch_dsb();
        good = (NX_F407_RTC_READ(&p->PRER) & RTC_PRER_PREDIV_S) == 255U;
        if (good) {
            LL_RTC_SetAsynchPrescaler(p, 127U);
            nx_arch_dsb();
            good = NX_F407_RTC_READ(&p->PRER) == PROFILE;
        }
        lock();
    }
    uint32_t time = __LL_RTC_CONVERT_BIN2BCD(c->hour) << 16U |
                    __LL_RTC_CONVERT_BIN2BCD(c->minute) << 8U |
                    __LL_RTC_CONVERT_BIN2BCD(c->second);
    uint32_t date = __LL_RTC_CONVERT_BIN2BCD(c->year) << 16U |
                    (uint32_t)c->weekday << 13U |
                    __LL_RTC_CONVERT_BIN2BCD(c->month) << 8U |
                    __LL_RTC_CONVERT_BIN2BCD(c->day);
    good = good && checked(&p->TR, time) && checked(&p->DR, date);
    if (provision)
        good = good && checked(&p->CR, RTC_CR_BYPSHAD);
    status_write(false, false);
    r = wait_bits(s->config, &p->ISR, RTC_ISR_INITF | RTC_ISR_INIT, 0U,
                  deadline);
    if (!good)
        return NX_ERROR_IO;
    if (r != NX_SUCCESS)
        return r;
    /* RM0090 Rev22 26.3.5: counters reload after four RTCCLK cycles. INITF
     * clearing alone does not prove that BYPSHAD reads see the new calendar.
     * Observe the complete value within the same finite operation budget. */
    for (uint32_t i = 0U; i < s->config->poll_limit; ++i) {
        nx_rtc_calendar_t observed;
        r = calendar(&observed);
        if (r == NX_SUCCESS && observed.year == c->year &&
            observed.month == c->month && observed.day == c->day &&
            observed.weekday == c->weekday && observed.hour == c->hour &&
            observed.minute == c->minute && observed.second == c->second)
            return NX_SUCCESS;
        if (!system_live())
            return NX_ERROR_STATE;
        if (nx_deadline_expired(
                deadline, s->config->clock.read(s->config->clock.context)))
            break;
    }
    return NX_ERROR_TIMEOUT;
}
static nx_result_t read_calendar(const void* raw, nx_rtc_snapshot_t* out) {
    nx_stm32_rtc_state_t* s = (nx_stm32_rtc_state_t*)raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s) ||
        !external(s, out, sizeof(*out), _Alignof(nx_rtc_snapshot_t), true))
        return NX_ERROR_INVALID;
    if (!s->opened || s->closing || s->suspended || s->tainted || !live(s))
        return NX_ERROR_STATE;
    nx_rtc_snapshot_t value = {.domain = {s, s->generation}};
    nx_result_t result = calendar(&value.calendar);
    if (result == NX_SUCCESS)
        *out = value;
    return result;
}
static nx_result_t set_calendar(void* raw, const nx_rtc_calendar_t* in) {
    nx_stm32_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s) ||
        !external(s, in, sizeof(*in), _Alignof(nx_rtc_calendar_t), true) ||
        nx_rtc_calendar_validate(in) != NX_SUCCESS)
        return NX_ERROR_INVALID;
    if (!s->opened || s->closing || s->suspended || s->tainted || !live(s))
        return NX_ERROR_STATE;
    if (!s->config->allow_set)
        return NX_ERROR_UNSUPPORTED;
    if (s->active != NULL || s->auxiliary_cr != 0U || s->auxiliary_tafcr != 0U)
        return NX_ERROR_BUSY;
    if (s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    nx_time_us_t deadline;
    nx_result_t r = nx_deadline_finite_after(
        s->config->clock.read(s->config->clock.context), s->config->timeout_us,
        &deadline);
    if (r != NX_SUCCESS)
        return r;
    nx_rtc_calendar_t value = *in;
    r = program_calendar(s, &value, deadline, false);
    if (r == NX_SUCCESS)
        ++s->generation;
    else
        s->tainted = true;
    return r;
}
static void finish(nx_stm32_rtc_state_t* s, nx_result_t result) {
    if (!s->terminal_set) {
        s->terminal = result;
        s->terminal_set = true;
    }
    if (s->stage < WITHDRAW)
        s->stage = WITHDRAW;
    if (s->active != NULL)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
}
static nx_result_t held(nx_stm32_rtc_state_t* s) {
    if (s->active != NULL)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
    return NX_ERROR_BUSY;
}
static nx_result_t submit(void* raw, nx_rtc_request_t* request) {
    nx_stm32_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s) || !external(s, request, sizeof(*request),
                               _Alignof(nx_rtc_request_t), false))
        return NX_ERROR_INVALID;
    if (!s->opened || s->closing || s->suspended || s->tainted)
        return NX_ERROR_STATE;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    if (!live(s) || (NX_F407_RTC_READ(&NX_F407_RTC->CR) & RTC_CR_ALRAE) ||
        (NX_F407_RTC_READ(&NX_F407_RTC->ISR) & RTC_ISR_ALRAF))
        return NX_ERROR_STATE;
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        request->domain.identity != s ||
        request->domain.generation != s->generation)
        return NX_ERROR_STATE;
    if (nx_rtc_alarm_validate(&request->alarm) != NX_SUCCESS)
        return NX_ERROR_INVALID;
    nx_rtc_calendar_t c;
    nx_result_t r = calendar(&c);
    if (r != NX_SUCCESS)
        return r;
    if (c.day == request->alarm.day && c.hour == request->alarm.hour &&
        c.minute == request->alarm.minute && c.second == request->alarm.second)
        return NX_ERROR_STATE;
    if (request->base.deadline != NX_DEADLINE_NEVER &&
        nx_deadline_expired(request->base.deadline,
                            s->config->clock.read(s->config->clock.context)))
        return NX_ERROR_TIMEOUT;
    r = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (r == NX_SUCCESS) {
        s->active = request;
        s->stage = SOURCE_OFF;
        s->terminal_set = false;
    }
    return r;
}
static nx_result_t cancel(void* raw, nx_rtc_request_t* request) {
    nx_stm32_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s))
        return NX_ERROR_INVALID;
    if (request == NULL || request != s->active)
        return NX_ERROR_STATE;
    finish(s, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
/** \brief Each call advances one hardware phase; ALRAWF proves real quiet. */
static nx_result_t service(void* raw) {
    nx_stm32_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s))
        return NX_ERROR_INVALID;
    if (s->active == NULL)
        return NX_SUCCESS;
    if (s->auxiliary_faulted) {
        finish(s, NX_ERROR_IO);
        return held(s);
    }
    if (s->tainted || !live(s)) {
        s->tainted = true;
        finish(s, NX_ERROR_STATE);
        return held(s);
    }
    if (!s->terminal_set && s->active->base.deadline != NX_DEADLINE_NEVER &&
        nx_deadline_expired(s->active->base.deadline,
                            s->config->clock.read(s->config->clock.context)))
        finish(s, NX_ERROR_TIMEOUT);
    RTC_TypeDef* p = NX_F407_RTC;
    bool good = true;
    switch (s->stage) {
        case SOURCE_OFF:
            good = checked(&p->CR, RTC_CR_BYPSHAD | s->auxiliary_cr);
            if (good)
                s->stage = OFF_READY;
            break;
        case OFF_READY:
            if (NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAWF)
                s->stage = PROGRAM;
            break;
        case PROGRAM: {
            const nx_rtc_alarm_t* a = &s->active->alarm;
            uint32_t value = __LL_RTC_CONVERT_BIN2BCD(a->day) << 24U |
                             __LL_RTC_CONVERT_BIN2BCD(a->hour) << 16U |
                             __LL_RTC_CONVERT_BIN2BCD(a->minute) << 8U |
                             __LL_RTC_CONVERT_BIN2BCD(a->second);
            status_write(false, true);
            good = !(NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAF) &&
                   checked(&p->ALRMAR, value) && checked(&p->ALRMASSR, 0U);
            if (good)
                s->stage = ARM;
            break;
        }
        case ARM:
            good = checked(&p->CR,
                           RTC_CR_BYPSHAD | s->auxiliary_cr | RTC_CR_ALRAE);
            if (good)
                s->stage = RUN;
            break;
        case RUN:
            if (NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAF)
                finish(s, NX_SUCCESS);
            break;
        case WITHDRAW:
            if (!checked(&p->CR, RTC_CR_BYPSHAD | s->auxiliary_cr))
                return held(s);
            s->stage = DRAIN_READY;
            break;
        case DRAIN_READY:
            if (!(NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAWF))
                return held(s);
            s->stage = CLEAR;
            break;
        case CLEAR:
            status_write(false, true);
            if (NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAF)
                return held(s);
            s->stage = RESTORE;
            break;
        case RESTORE:
            if (!checked(&p->ALRMAR, s->saved_alarm) ||
                !checked(&p->ALRMASSR, s->saved_subsecond) ||
                !(NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAWF) ||
                (NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAF) || !live(s))
                return held(s);
            {
                nx_rtc_request_t* request = s->active;
                nx_result_t result = s->terminal;
                s->active = NULL;
                s->stage = IDLE;
                s->terminal_set = false;
                nx_request_settle(&request->base, result,
                                  result == NX_SUCCESS ? 1U : 0U);
            }
            return NX_SUCCESS;
        default:
            good = false;
            break;
    }
    if (!good)
        finish(s, NX_ERROR_IO);
    return NX_ERROR_BUSY;
}
static nx_result_t stop(void* raw) {
    nx_stm32_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s))
        return NX_ERROR_INVALID;
    if (s->suspended)
        return NX_ERROR_BUSY;
    s->closing = true;
    if (s->auxiliary_faulted && auxiliary_close(s) != NX_SUCCESS)
        return NX_ERROR_BUSY;
    if (s->active != NULL) {
        finish(s, NX_ERROR_CANCELLED);
        (void)service(s);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    if (!s->opened)
        return s->tainted ? NX_ERROR_BUSY : NX_SUCCESS;
    if (auxiliary_close(s) != NX_SUCCESS)
        return NX_ERROR_BUSY;
    if (s->tainted || !live(s) ||
        NX_F407_RTC_READ(&NX_F407_RTC->CR) != RTC_CR_BYPSHAD ||
        (NX_F407_RTC_READ(&NX_F407_RTC->ISR) & RTC_ISR_ALRAF))
        return NX_ERROR_BUSY;
    s->opened = false;
    return NX_SUCCESS;
}
/** \brief Auxiliary operations share calendar admission and one task owner. */
static nx_result_t auxiliary_admit(nx_stm32_rtc_state_t* s) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s))
        return NX_ERROR_INVALID;
    if (!s->opened || s->closing || s->suspended || s->tainted ||
        s->auxiliary_faulted || !live(s))
        return NX_ERROR_STATE;
    return NX_SUCCESS;
}
/** \brief Every clock crossing has both a finite Core and poll-count budget. */
static nx_result_t auxiliary_deadline(nx_stm32_rtc_state_t* s,
                                      nx_time_us_t* deadline) {
    return nx_deadline_finite_after(
        s->config->clock.read(s->config->clock.context), s->config->timeout_us,
        deadline);
}
/** \brief Only selected flags are W0C, with INIT preserved atomically. */
static bool clear_status(uint32_t flags) {
    unlock();
    uint32_t init = NX_F407_RTC_READ(&NX_F407_RTC->ISR) & RTC_ISR_INIT;
    NX_F407_RTC_WRITE(&NX_F407_RTC->ISR,
                      ~(flags | (uint32_t)RTC_ISR_INIT) | init);
    nx_arch_dsb();
    lock();
    return (NX_F407_RTC_READ(&NX_F407_RTC->ISR) & flags) == 0U;
}
/** \brief Retain expected controls even if a physical write is refused. */
static nx_result_t auxiliary_control(nx_stm32_rtc_state_t* s,
                                     uint32_t controls) {
    s->auxiliary_dirty = true;
    s->auxiliary_cr = controls;
    uint32_t alarm_a = NX_F407_RTC_READ(&NX_F407_RTC->CR) & RTC_CR_ALRAE;
    if (!checked(&NX_F407_RTC->CR, RTC_CR_BYPSHAD | controls | alarm_a)) {
        s->auxiliary_faulted = true;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
/** \brief A partial operation retains sources until explicit stop retries. */
static nx_result_t auxiliary_failure(nx_stm32_rtc_state_t* s,
                                     nx_result_t result) {
    if (result != NX_SUCCESS)
        s->auxiliary_faulted = true;
    return result;
}
/** \brief Alarm B has its own write-ready handshake, independent of Alarm A. */
static nx_result_t alarm_b(void* raw, bool enabled,
                           const nx_rtc_alarm_t* alarm) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (!external(s, alarm, sizeof(*alarm), _Alignof(nx_rtc_alarm_t), true) ||
        nx_rtc_alarm_validate(alarm) != NX_SUCCESS)
        return NX_ERROR_INVALID;
    nx_time_us_t deadline;
    r = auxiliary_deadline(s, &deadline);
    if (r != NX_SUCCESS)
        return r;
    nx_rtc_alarm_t copied = *alarm;
    r = auxiliary_control(s, s->auxiliary_cr & ~(uint32_t)RTC_CR_ALRBE);
    if (r == NX_SUCCESS)
        r = wait_bits(s->config, &NX_F407_RTC->ISR, RTC_ISR_ALRBWF,
                      RTC_ISR_ALRBWF, deadline);
    if (r != NX_SUCCESS)
        return auxiliary_failure(s, r);
    uint32_t value = __LL_RTC_CONVERT_BIN2BCD(copied.day) << 24U |
                     __LL_RTC_CONVERT_BIN2BCD(copied.hour) << 16U |
                     __LL_RTC_CONVERT_BIN2BCD(copied.minute) << 8U |
                     __LL_RTC_CONVERT_BIN2BCD(copied.second);
    if (!clear_status(RTC_ISR_ALRBF) || !checked(&NX_F407_RTC->ALRMBR, value) ||
        !checked(&NX_F407_RTC->ALRMBSSR, 0U))
        return auxiliary_failure(s, NX_ERROR_IO);
    return auxiliary_control(s,
                             s->auxiliary_cr | (enabled ? RTC_CR_ALRBE : 0U));
}
/** \brief WUTR and WUCKSEL are writable only after actual WUTWF. */
static nx_result_t wakeup(void* raw, const nx_rtc_wakeup_t* config) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (!external(s, config, sizeof(*config), _Alignof(nx_rtc_wakeup_t),
                  true) ||
        (unsigned)config->clock > NX_RTC_WAKE_SECOND_EXTENDED ||
        (config->clock == NX_RTC_WAKE_DIV2 && config->reload == 0U))
        return NX_ERROR_INVALID;
    nx_rtc_wakeup_t copied = *config;
    nx_time_us_t deadline;
    r = auxiliary_deadline(s, &deadline);
    if (r != NX_SUCCESS)
        return r;
    r = auxiliary_control(s, s->auxiliary_cr & ~(uint32_t)RTC_CR_WUTE);
    if (r == NX_SUCCESS)
        r = wait_bits(s->config, &NX_F407_RTC->ISR, RTC_ISR_WUTWF,
                      RTC_ISR_WUTWF, deadline);
    if (r != NX_SUCCESS)
        return auxiliary_failure(s, r);
    if (!clear_status(RTC_ISR_WUTF) ||
        !checked(&NX_F407_RTC->WUTR, copied.reload))
        return auxiliary_failure(s, NX_ERROR_IO);
    uint32_t clock = copied.clock == NX_RTC_WAKE_SECOND_EXTENDED
                         ? 6U
                         : (uint32_t)copied.clock;
    uint32_t controls = (s->auxiliary_cr & ~(uint32_t)RTC_CR_WUCKSEL) | clock;
    if (copied.enabled)
        controls |= RTC_CR_WUTE;
    return auxiliary_control(s, controls);
}
/** \brief ES0182 2.9.6 applies every time an edge source is enabled again. */
static nx_result_t capture_input(nx_stm32_rtc_state_t* s, uint32_t tafcr) {
    if ((tafcr & (RTC_TAFCR_TAMP1E | RTC_TAFCR_TAMPFLT)) != RTC_TAFCR_TAMP1E)
        return NX_SUCCESS;
    /* The caller handed off this input. Observe it after enabling detection,
     * through a live gate, before backup writes may be treated as protected.
     * Failure retains the armed source and any refused inspection gate. */
    if (!inspection_clock(s))
        return NX_ERROR_IO;
    /* Analog mode disconnects the digital input buffer; IDR zero then cannot
     * establish an inactive tamper input. Never configure the handed-off pad.
     */
    bool active =
        ((NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->MODER) >> 26U) & 3U) == 3U ||
        (NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->IDR) & (1U << 13U)) != 0U;
    bool restored = restore_inspection(s);
    return active || !restored ? NX_ERROR_IO : NX_SUCCESS;
}
/** \brief Inputs are explicit pad handoffs, not inferred PCB capabilities. */
static nx_result_t capture_configure(void* raw,
                                     const nx_rtc_capture_config_t* config) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (!external(s, config, sizeof(*config), _Alignof(nx_rtc_capture_config_t),
                  true) ||
        config->timestamp_pin < 1U || config->timestamp_pin > 2U ||
        config->tamper1_pin < 1U || config->tamper1_pin > 2U ||
        config->tamper_mask > 3U || config->trigger_mask > 3U ||
        config->filter > 3U || config->sample_rate > 7U ||
        config->precharge > 3U ||
        (config->timestamp_pin == 2U && config->filter != 0U &&
         (config->tamper_mask & 1U) && config->tamper1_pin == 1U) ||
        (config->filter != 0U && config->timestamp_enabled &&
         !config->timestamp_on_tamper &&
         (((config->tamper_mask & 1U) &&
           config->timestamp_pin == config->tamper1_pin) ||
          ((config->tamper_mask & 2U) && config->timestamp_pin == 2U))))
        return NX_ERROR_INVALID;
    /* PI8/RTC_AF2 is absent from both maintained F407VE100 and ZG144
     * packages. The family SDK macro is not package capability evidence. */
    if (config->timestamp_pin == 2U || config->tamper1_pin == 2U ||
        (config->tamper_mask & 2U))
        return NX_ERROR_UNSUPPORTED;
    /* ES0182 2.2.18: digital PC13 transitions disturb a crystal LSE. The
     * separately authored external-clock bypass input is not affected. */
    if (!s->config->lse_bypass &&
        (config->timestamp_enabled || config->tamper_mask != 0U))
        return NX_ERROR_UNSUPPORTED;
    /* ES0182 2.9.1 has no workaround for false backup erasure on disabling
     * unfiltered falling-edge tamper. Filtered level detection is separate. */
    if (config->filter == 0U &&
        (config->tamper_mask & config->trigger_mask) != 0U)
        return NX_ERROR_UNSUPPORTED;
    if (NX_F407_RTC_READ(&NX_F407_RTC->ISR) &
        (RTC_ISR_TSF | RTC_ISR_TSOVF | RTC_ISR_TAMP1F | RTC_ISR_TAMP2F))
        return NX_ERROR_BUSY;
    nx_rtc_capture_config_t copied = *config;
    uint32_t cr = s->auxiliary_cr & ~(uint32_t)(RTC_CR_TSE | RTC_CR_TSEDGE);
    r = auxiliary_control(s, cr);
    if (r != NX_SUCCESS)
        return r;
    /* Disable tamper before changing edge polarity; otherwise a control
     * write can itself be interpreted as a tamper event and erase data. */
    uint32_t irq = s->auxiliary_tafcr & RTC_TAFCR_TAMPIE;
    if (!checked(&NX_F407_RTC->TAFCR, irq))
        return auxiliary_failure(s, NX_ERROR_IO);
    uint32_t tafcr = irq | ((uint32_t)copied.filter << 11U) |
                     ((uint32_t)copied.sample_rate << 8U) |
                     ((uint32_t)copied.precharge << 13U) |
                     (copied.pullup ? 0U : RTC_TAFCR_TAMPPUDIS) |
                     (copied.timestamp_on_tamper ? RTC_TAFCR_TAMPTS : 0U) |
                     ((copied.trigger_mask & 1U) ? RTC_TAFCR_TAMP1TRG : 0U) |
                     ((copied.tamper_mask & 1U) ? RTC_TAFCR_TAMP1E : 0U);
    s->auxiliary_tafcr = tafcr;
    if (!checked(&NX_F407_RTC->TAFCR, tafcr))
        return auxiliary_failure(s, NX_ERROR_IO);
    r = capture_input(s, tafcr);
    if (r != NX_SUCCESS)
        return auxiliary_failure(s, r);
    cr |= (copied.timestamp_enabled ? RTC_CR_TSE : 0U) |
          (copied.timestamp_falling ? RTC_CR_TSEDGE : 0U);
    return auxiliary_control(s, cr);
}
/** \brief Copy raw captured fields; the RTC never captured a year. */
static nx_result_t capture_read(void* raw, nx_rtc_capture_t* out) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (!external(s, out, sizeof(*out), _Alignof(nx_rtc_capture_t), true))
        return NX_ERROR_INVALID;
    uint32_t flags = NX_F407_RTC_READ(&NX_F407_RTC->ISR);
    if (!(flags & RTC_ISR_TSF))
        return NX_ERROR_BUSY;
    uint32_t sub = NX_F407_RTC_READ(&NX_F407_RTC->TSSSR);
    uint32_t time = NX_F407_RTC_READ(&NX_F407_RTC->TSTR);
    uint32_t date = NX_F407_RTC_READ(&NX_F407_RTC->TSDR);
    nx_rtc_capture_t value = {.domain = {s, s->generation},
                              .weekday = (uint8_t)((date >> 13U) & 7U),
                              .subseconds = (uint16_t)sub,
                              .subsecond_divisor = 256U};
    nx_rtc_calendar_t valid = {.year = 0U};
    if (sub > 255U || (time & ~UINT32_C(0x003f7f7f)) ||
        (date & ~UINT32_C(0x0000ff3f)) ||
        !bcd((time >> 16U) & 0x3fU, &value.hour) ||
        !bcd((time >> 8U) & 0x7fU, &value.minute) ||
        !bcd(time & 0x7fU, &value.second) ||
        !bcd((date >> 8U) & 0x1fU, &value.month) ||
        !bcd(date & 0x3fU, &value.day))
        return NX_ERROR_STATE;
    valid.month = value.month;
    valid.day = value.day;
    valid.weekday = value.weekday;
    valid.hour = value.hour;
    valid.minute = value.minute;
    valid.second = value.second;
    if (nx_rtc_calendar_validate(&valid) != NX_SUCCESS)
        return NX_ERROR_STATE;
    value.overflow =
        ((flags | NX_F407_RTC_READ(&NX_F407_RTC->ISR)) & RTC_ISR_TSOVF) != 0U;
    *out = value;
    return NX_SUCCESS;
}
/** \brief Stable explicit mapping; status bits are not public ABI. */
static uint32_t event_flags(uint32_t selected) {
    return ((selected & NX_RTC_EVENT_ALARM_B) ? RTC_ISR_ALRBF : 0U) |
           ((selected & NX_RTC_EVENT_WAKEUP) ? RTC_ISR_WUTF : 0U) |
           ((selected & NX_RTC_EVENT_TIMESTAMP) ? RTC_ISR_TSF : 0U) |
           ((selected & NX_RTC_EVENT_TIMESTAMP_OVERFLOW) ? RTC_ISR_TSOVF : 0U) |
           ((selected & NX_RTC_EVENT_TAMPER1) ? RTC_ISR_TAMP1F : 0U) |
           ((selected & NX_RTC_EVENT_TAMPER2) ? RTC_ISR_TAMP2F : 0U);
}
/** \brief Observing auxiliary flags never consumes a capture or request. */
static nx_result_t events(void* raw, uint32_t* out) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (!external(s, out, sizeof(*out), _Alignof(uint32_t), true))
        return NX_ERROR_INVALID;
    uint32_t flags = NX_F407_RTC_READ(&NX_F407_RTC->ISR), value = 0U;
    for (uint32_t bit = 1U; bit <= NX_RTC_EVENT_TAMPER2; bit <<= 1U)
        if (flags & event_flags(bit))
            value |= bit;
    *out = value;
    return NX_SUCCESS;
}
/** \brief Never clear an unobserved TSF: it may be crossing clock domains. */
static nx_result_t acknowledge(void* raw, uint32_t selected) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (selected & ~UINT32_C(63))
        return NX_ERROR_INVALID;
    uint32_t status = NX_F407_RTC_READ(&NX_F407_RTC->ISR);
    uint32_t mask = event_flags(selected) & status;
    if ((mask & RTC_ISR_TSOVF) && !(status & RTC_ISR_TSF))
        return NX_ERROR_BUSY;
    return clear_status(mask) ? NX_SUCCESS : NX_ERROR_BUSY;
}
/** \brief Coarse calibration requires the actual INITF handshake. */
static nx_result_t coarse_write(nx_stm32_rtc_state_t* s, uint32_t value,
                                nx_time_us_t deadline) {
    nx_result_t r = synchronize_init(s, deadline);
    if (r != NX_SUCCESS)
        return r;
    status_write(true, false);
    r = wait_bits(s->config, &NX_F407_RTC->ISR, RTC_ISR_INITF, RTC_ISR_INITF,
                  deadline);
    if (r == NX_SUCCESS && !checked(&NX_F407_RTC->CALIBR, value))
        r = NX_ERROR_IO;
    status_write(false, false);
    if (r != NX_SUCCESS)
        return r;
    return wait_bits(s->config, &NX_F407_RTC->ISR, RTC_ISR_INIT | RTC_ISR_INITF,
                     0U, deadline);
}
/** \brief Calibration changes only the RTC oscillator/divider domain. */
static nx_result_t calibrate(void* raw, const nx_rtc_calibration_t* c) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (!external(s, c, sizeof(*c), _Alignof(nx_rtc_calibration_t), true) ||
        (c->kind != NX_RTC_CALIBRATION_SMOOTH &&
         c->kind != NX_RTC_CALIBRATION_COARSE))
        return NX_ERROR_INVALID;
    bool smooth = c->kind == NX_RTC_CALIBRATION_SMOOTH;
    if ((smooth && (c->pulses > 511U || c->subtract ||
                    (c->window_seconds != 8U && c->window_seconds != 16U &&
                     c->window_seconds != 32U) ||
                    (c->window_seconds == 8U && (c->pulses & 3U)) ||
                    (c->window_seconds == 16U && (c->pulses & 1U)))) ||
        (!smooth && (c->pulses > 31U || c->add512 || c->window_seconds)))
        return NX_ERROR_INVALID;
    bool resets_divider = !smooth || s->auxiliary_calibr != 0U;
    if (resets_divider && !s->config->allow_set)
        return NX_ERROR_UNSUPPORTED;
    if (s->active != NULL ||
        (resets_divider && (s->auxiliary_cr != 0U || s->auxiliary_tafcr != 0U)))
        return NX_ERROR_BUSY;
    if (resets_divider && s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    nx_time_us_t deadline;
    r = auxiliary_deadline(s, &deadline);
    if (r != NX_SUCCESS)
        return r;
    if (NX_F407_RTC_READ(&NX_F407_RTC->ISR) & RTC_ISR_RECALPF)
        return NX_ERROR_BUSY;
    s->auxiliary_dirty = true;
    if (smooth) {
        if (s->auxiliary_calibr != 0U) {
            r = coarse_write(s, 0U, deadline);
            if (r != NX_SUCCESS)
                return auxiliary_failure(s, r);
            s->auxiliary_calibr = 0U;
            ++s->generation;
        }
        uint32_t value = c->pulses | (c->add512 ? RTC_CALR_CALP : 0U) |
                         (c->window_seconds == 8U ? RTC_CALR_CALW8 : 0U) |
                         (c->window_seconds == 16U ? RTC_CALR_CALW16 : 0U);
        if (!checked(&NX_F407_RTC->CALR, value))
            return auxiliary_failure(s, NX_ERROR_IO);
        s->auxiliary_calr = value;
    } else {
        if (!checked(&NX_F407_RTC->CALR, 0U))
            return auxiliary_failure(s, NX_ERROR_IO);
        s->auxiliary_calr = 0U;
        uint32_t value = c->pulses | (c->subtract ? RTC_CALIBR_DCS : 0U);
        r = wait_bits(s->config, &NX_F407_RTC->ISR, RTC_ISR_RECALPF, 0U,
                      deadline);
        if (r != NX_SUCCESS)
            return auxiliary_failure(s, r);
        r = coarse_write(s, value, deadline);
        if (r != NX_SUCCESS)
            return auxiliary_failure(s, r);
        s->auxiliary_calibr = value;
        ++s->generation;
    }
    return NX_SUCCESS;
}
/** \brief EXTI is an explicit event source, with no hidden IRQ publisher. */
static nx_result_t wake_route(void* raw, uint32_t selected) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (selected & ~UINT32_C(119))
        return NX_ERROR_INVALID;
    uint32_t lines = 0U;
    uint32_t cr = s->auxiliary_cr & ~(uint32_t)(RTC_CR_ALRAIE | RTC_CR_ALRBIE |
                                                RTC_CR_WUTIE | RTC_CR_TSIE);
    uint32_t tafcr = s->auxiliary_tafcr & ~(uint32_t)RTC_TAFCR_TAMPIE;
    if (selected & NX_RTC_EVENT_ALARM_A)
        cr |= RTC_CR_ALRAIE;
    if (selected & NX_RTC_EVENT_ALARM_B)
        cr |= RTC_CR_ALRBIE;
    if (selected & (NX_RTC_EVENT_ALARM_A | NX_RTC_EVENT_ALARM_B))
        lines |= 1U << 17U;
    if (selected & NX_RTC_EVENT_WAKEUP) {
        cr |= RTC_CR_WUTIE;
        lines |= 1U << 22U;
    }
    if (selected & NX_RTC_EVENT_TIMESTAMP) {
        cr |= RTC_CR_TSIE;
        lines |= 1U << 21U;
    }
    if (selected & (NX_RTC_EVENT_TAMPER1 | NX_RTC_EVENT_TAMPER2)) {
        tafcr |= RTC_TAFCR_TAMPIE;
        lines |= 1U << 21U;
    }
    s->auxiliary_dirty = true;
    /* Withdraw event routing before clearing pending EXTI state. Peripheral
     * latches remain intact and need explicit acknowledgement. */
    NX_F407_RTC_WRITE(&NX_F407_RTC_EXTI->EMR,
                      NX_F407_RTC_READ(&NX_F407_RTC_EXTI->EMR) & ~WAKE_LINES);
    NX_F407_RTC_WRITE(
        &NX_F407_RTC_EXTI->RTSR,
        (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->RTSR) & ~WAKE_LINES) | lines);
    NX_F407_RTC_WRITE(&NX_F407_RTC_EXTI->PR, WAKE_LINES);
    s->wake_lines = lines;
    s->auxiliary_tafcr = tafcr;
    if (!checked(&NX_F407_RTC->TAFCR, tafcr))
        return auxiliary_failure(s, NX_ERROR_IO);
    r = auxiliary_control(s, cr);
    if (r != NX_SUCCESS)
        return r;
    NX_F407_RTC_WRITE(&NX_F407_RTC_EXTI->EMR,
                      NX_F407_RTC_READ(&NX_F407_RTC_EXTI->EMR) | lines);
    nx_arch_dsb();
    return live(s) ? NX_SUCCESS : auxiliary_failure(s, NX_ERROR_IO);
}
/** \brief Exact F407 BKP0R..BKP19R span, without indexing through structs. */
static volatile uint32_t* backup_word(uint8_t index) {
    return (volatile uint32_t*)((uintptr_t)NX_F407_RTC + 0x50U +
                                (uintptr_t)index * 4U);
}
/** \brief Backup register reads never imply calendar provenance. */
static nx_result_t backup_read(void* raw, uint8_t index, uint32_t* value) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (index >= 20U ||
        !external(s, value, sizeof(*value), _Alignof(uint32_t), true))
        return NX_ERROR_INVALID;
    *value = NX_F407_RTC_READ(backup_word(index));
    return NX_SUCCESS;
}
/** \brief Write/readback touches one word; tamper may independently erase it.
 */
static nx_result_t backup_write(void* raw, uint8_t index, uint32_t value) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (index >= 20U)
        return NX_ERROR_INVALID;
    return checked(backup_word(index), value) ? NX_SUCCESS : NX_ERROR_IO;
}
/** \brief SRAM gate and regulator are explicit reversible owned effects. */
static nx_result_t sram_prepare(void* raw, bool retained) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (s->sram_ready || s->sram_effects != 0U)
        return NX_ERROR_BUSY;
    nx_time_us_t deadline;
    r = auxiliary_deadline(s, &deadline);
    if (r != NX_SUCCESS)
        return r;
    if (!(NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) &
          RCC_AHB1ENR_BKPSRAMEN)) {
        s->sram_effects |= 1U;
        NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->AHB1ENR,
                          NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) |
                              RCC_AHB1ENR_BKPSRAMEN);
        nx_arch_dsb();
        if (!(NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) &
              RCC_AHB1ENR_BKPSRAMEN))
            return NX_ERROR_IO;
    }
    if (retained) {
        if (!(NX_F407_RTC_READ(&NX_F407_RTC_PWR->CSR) & PWR_CSR_BRE)) {
            s->sram_effects |= 2U;
            NX_F407_RTC_WRITE(&NX_F407_RTC_PWR->CSR,
                              NX_F407_RTC_READ(&NX_F407_RTC_PWR->CSR) |
                                  PWR_CSR_BRE);
        }
        r = wait_bits(s->config, &NX_F407_RTC_PWR->CSR,
                      PWR_CSR_BRE | PWR_CSR_BRR, PWR_CSR_BRE | PWR_CSR_BRR,
                      deadline);
        if (r != NX_SUCCESS)
            return r;
    }
    s->sram_ready = true;
    s->sram_retention = retained;
    return NX_SUCCESS;
}
/** \brief Admission creates exactly one stable direct-access lease. */
static nx_result_t sram_acquire(void* raw, nx_memory_window_lease_t* lease) {
    nx_stm32_rtc_state_t* s = raw;
    nx_result_t r = auxiliary_admit(s);
    if (r != NX_SUCCESS)
        return r;
    if (!external(s, lease, sizeof(*lease), _Alignof(nx_memory_window_lease_t),
                  true))
        return NX_ERROR_INVALID;
    if (!s->sram_ready ||
        !(NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) & RCC_AHB1ENR_BKPSRAMEN))
        return NX_ERROR_STATE;
    if (s->sram_retention &&
        (NX_F407_RTC_READ(&NX_F407_RTC_PWR->CSR) &
         (PWR_CSR_BRE | PWR_CSR_BRR)) != (PWR_CSR_BRE | PWR_CSR_BRR))
        return NX_ERROR_STATE;
    if (s->sram_lease != NULL)
        return NX_ERROR_BUSY;
    if (lease->owner != NULL || lease->address != 0U || lease->bytes != 0U ||
        lease->epoch != 0U)
        return NX_ERROR_STATE;
    if (s->sram_epoch == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    ++s->sram_epoch;
    s->sram_lease = lease;
    *lease = (nx_memory_window_lease_t){s, (uintptr_t)NX_F407_RTC_SRAM, 4096U,
                                        s->sram_epoch};
    return NX_SUCCESS;
}
/** \brief Final lease access precedes detachment and any clock withdrawal. */
static nx_result_t sram_release(void* raw, nx_memory_window_lease_t* lease,
                                uint64_t epoch) {
    nx_stm32_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s))
        return NX_ERROR_INVALID;
    if (s->sram_lease != lease || lease == NULL)
        return NX_ERROR_STATE;
    if (epoch != s->sram_epoch || lease->epoch != epoch || lease->owner != s ||
        lease->address != (uintptr_t)NX_F407_RTC_SRAM || lease->bytes != 4096U)
        return NX_ERROR_STATE;
    nx_arch_dsb();
    *lease = (nx_memory_window_lease_t){0};
    s->sram_lease = NULL;
    return NX_SUCCESS;
}
/** \brief Restore sources and owned power only after every user exits. */
static nx_result_t auxiliary_close(nx_stm32_rtc_state_t* s) {
    if (s->sram_lease != NULL)
        return NX_ERROR_BUSY;
    if (s->auxiliary_dirty) {
        nx_time_us_t deadline;
        if (auxiliary_deadline(s, &deadline) != NX_SUCCESS)
            return NX_ERROR_BUSY;
        NX_F407_RTC_WRITE(&NX_F407_RTC_EXTI->EMR,
                          NX_F407_RTC_READ(&NX_F407_RTC_EXTI->EMR) &
                              ~WAKE_LINES);
        NX_F407_RTC_WRITE(&NX_F407_RTC_EXTI->RTSR,
                          NX_F407_RTC_READ(&NX_F407_RTC_EXTI->RTSR) &
                              ~WAKE_LINES);
        s->wake_lines = 0U;
        if (auxiliary_control(s, 0U) != NX_SUCCESS ||
            wait_bits(s->config, &NX_F407_RTC->ISR,
                      RTC_ISR_ALRBWF | RTC_ISR_WUTWF,
                      RTC_ISR_ALRBWF | RTC_ISR_WUTWF, deadline) != NX_SUCCESS)
            return NX_ERROR_BUSY;
        if (!checked(&NX_F407_RTC->TAFCR, 0U))
            return NX_ERROR_BUSY;
        s->auxiliary_tafcr = 0U;
        if (!clear_status(NX_F407_RTC_READ(&NX_F407_RTC->ISR) & AUX_STATUS) ||
            !checked(&NX_F407_RTC->ALRMBR, s->saved_alarm_b) ||
            !checked(&NX_F407_RTC->ALRMBSSR, s->saved_subsecond_b) ||
            !checked(&NX_F407_RTC->WUTR, s->saved_wakeup))
            return NX_ERROR_BUSY;
        if (wait_bits(s->config, &NX_F407_RTC->ISR, RTC_ISR_RECALPF, 0U,
                      deadline) != NX_SUCCESS ||
            !checked(&NX_F407_RTC->CALR, 0U))
            return NX_ERROR_BUSY;
        s->auxiliary_calr = 0U;
        if (NX_F407_RTC_READ(&NX_F407_RTC->CALIBR) != 0U &&
            (wait_bits(s->config, &NX_F407_RTC->ISR, RTC_ISR_RECALPF, 0U,
                       deadline) != NX_SUCCESS ||
             coarse_write(s, 0U, deadline) != NX_SUCCESS))
            return NX_ERROR_BUSY;
        s->auxiliary_calibr = 0U;
        NX_F407_RTC_WRITE(&NX_F407_RTC_EXTI->PR, WAKE_LINES);
        if (!quiet())
            return NX_ERROR_BUSY;
        s->auxiliary_dirty = false;
        s->auxiliary_faulted = false;
    }
    s->sram_ready = false;
    s->sram_retention = false;
    if (s->sram_effects & 2U) {
        NX_F407_RTC_WRITE(&NX_F407_RTC_PWR->CSR,
                          NX_F407_RTC_READ(&NX_F407_RTC_PWR->CSR) &
                              ~(uint32_t)PWR_CSR_BRE);
        nx_arch_dsb();
        if (NX_F407_RTC_READ(&NX_F407_RTC_PWR->CSR) & PWR_CSR_BRE)
            return NX_ERROR_BUSY;
        s->sram_effects &= (uint8_t)~2U;
    }
    if (s->sram_effects & 1U) {
        NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->AHB1ENR,
                          NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) &
                              ~(uint32_t)RCC_AHB1ENR_BKPSRAMEN);
        nx_arch_dsb();
        if (NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) & RCC_AHB1ENR_BKPSRAMEN)
            return NX_ERROR_BUSY;
        s->sram_effects &= (uint8_t)~1U;
    }
    return NX_SUCCESS;
}
/** \brief Transfer only proved autonomous provisioning, never failed cleanup.
 */
static nx_result_t handoff(void* raw, nx_rtc_backup_handoff_t* out) {
    nx_stm32_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s) || !external(s, out, sizeof(*out),
                               _Alignof(nx_rtc_backup_handoff_t), true))
        return NX_ERROR_INVALID;
    if (s->opened || !s->closing || s->active != NULL ||
        s->sram_lease != NULL || s->auxiliary_dirty || s->sram_effects != 0U ||
        s->tainted || (s->effects & ~UINT32_C(0xbf)) != 0U)
        return NX_ERROR_BUSY;
    if (s->effects == 0U || !live(s))
        return NX_ERROR_STATE;
    nx_rtc_backup_handoff_t value = {.resources = s->effects,
                                     .oscillator_hz = 32768U};
    nx_result_t r = calendar(&value.calendar);
    if (r != NX_SUCCESS)
        return r;
    s->effects = 0U;
    *out = value;
    return NX_SUCCESS;
}
#define STANDBY_CR                                                             \
    ((uint32_t)(RTC_CR_ALRAE | RTC_CR_ALRBE | RTC_CR_WUTE | RTC_CR_TSE |       \
                RTC_CR_ALRAIE | RTC_CR_ALRBIE | RTC_CR_WUTIE | RTC_CR_TSIE))
#define STANDBY_TAFCR                                                          \
    ((uint32_t)(RTC_TAFCR_TAMP1E | RTC_TAFCR_TAMP2E | RTC_TAFCR_TAMPIE))
enum {
    STANDBY_PREPARING = 1,
    STANDBY_DISABLED,
    STANDBY_COMMITTING,
    STANDBY_COMMITTED
};
/** \brief Cleanup and Power commit never depend on the old Core time domain. */
static bool standby_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr();
}
/** \brief EXTI route configuration remains in its existing RTC owner. */
static bool standby_hardware(const nx_stm32_rtc_state_t* s) {
    return NX_F407_RTC_IRQ_QUIET() &&
           (NX_F407_RTC_READ(&NX_F407_RTC_RCC->APB1ENR) & RCC_APB1ENR_PWREN) !=
               0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC_PWR->CR) & PWR_CR_DBP) != 0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC_RCC->BDCR) & BACKUP_MASK) ==
               (BACKUP_LIVE | (s->config->lse_bypass ? RCC_BDCR_LSEBYP : 0U)) &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->IMR) & WAKE_LINES) == 0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->FTSR) & WAKE_LINES) == 0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->EMR) & WAKE_LINES) ==
               s->wake_lines &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->RTSR) & WAKE_LINES) ==
               s->wake_lines &&
           (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->PR) &
            (WAKE_LINES & ~s->wake_lines)) == 0U &&
           NX_F407_RTC_READ(&NX_F407_RTC->PRER) == PROFILE &&
           NX_F407_RTC_READ(&NX_F407_RTC->CALR) == s->auxiliary_calr &&
           NX_F407_RTC_READ(&NX_F407_RTC->CALIBR) == s->auxiliary_calibr &&
           (NX_F407_RTC_READ(&NX_F407_RTC->CR) & ~STANDBY_CR) ==
               (s->standby_cr & ~STANDBY_CR) &&
           (NX_F407_RTC_READ(&NX_F407_RTC->TAFCR) & ~STANDBY_TAFCR) ==
               (s->standby_tafcr & ~STANDBY_TAFCR) &&
           (NX_F407_RTC_READ(&NX_F407_RTC->ISR) &
            (RTC_ISR_INIT | RTC_ISR_INITF | RTC_ISR_SHPF)) == 0U;
}
/** \brief Exact receipt storage and monotonic identity, without pointer
 * registry. */
static nx_result_t standby_identity(nx_stm32_rtc_state_t* s,
                                    const nx_rtc_standby_t* receipt) {
    if (!owner(s) || !external(s, receipt, sizeof(*receipt),
                               _Alignof(nx_rtc_standby_t), true))
        return NX_ERROR_INVALID;
    if (!s->opened || s->closing || !s->suspended || s->tainted ||
        s->auxiliary_faulted || s->active != NULL || s->sram_lease != NULL ||
        s->standby_receipt != receipt || receipt->owner != s ||
        receipt->epoch == 0U || receipt->epoch != s->standby_epoch)
        return NX_ERROR_STATE;
    return standby_hardware(s) ? NX_SUCCESS : NX_ERROR_STATE;
}
static uint32_t standby_flags(uint32_t selected) {
    return event_flags(selected) |
           ((selected & NX_RTC_EVENT_ALARM_A) != 0U ? (uint32_t)RTC_ISR_ALRAF
                                                    : 0U);
}
/** \brief A shared pending EXTI line needs authorization for its routed
 * sources. */
static uint32_t standby_pending_events(uint32_t pr, uint32_t cr,
                                       uint32_t tafcr) {
    uint32_t events = 0U;
    if ((pr & (1U << 17U)) != 0U) {
        if ((cr & RTC_CR_ALRAIE) != 0U)
            events |= NX_RTC_EVENT_ALARM_A;
        if ((cr & RTC_CR_ALRBIE) != 0U)
            events |= NX_RTC_EVENT_ALARM_B;
    }
    if ((pr & (1U << 22U)) != 0U)
        events |= NX_RTC_EVENT_WAKEUP;
    if ((pr & (1U << 21U)) != 0U) {
        if ((cr & RTC_CR_TSIE) != 0U)
            events |= NX_RTC_EVENT_TIMESTAMP;
        if ((tafcr & RTC_TAFCR_TAMPIE) != 0U) {
            uint32_t tampers = 0U;
            if ((tafcr & RTC_TAFCR_TAMP1E) != 0U)
                tampers |= NX_RTC_EVENT_TAMPER1;
            if ((tafcr & RTC_TAFCR_TAMP2E) != 0U)
                tampers |= NX_RTC_EVENT_TAMPER2;
            events |= tampers != 0U
                          ? tampers
                          : NX_RTC_EVENT_TAMPER1 | NX_RTC_EVENT_TAMPER2;
        }
    }
    return events;
}
/** \brief Disable -> bounded WF -> explicit observed flags/PR clear
 * (ES0182 2.2.6). */
static nx_result_t standby_prepare(void* raw, uint32_t discard, uint32_t limit,
                                   nx_rtc_standby_t* receipt) {
    nx_stm32_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s) ||
        !external(s, receipt, sizeof(*receipt), _Alignof(nx_rtc_standby_t),
                  true) ||
        (discard & ~UINT32_C(127)) != 0U || limit == 0U)
        return NX_ERROR_INVALID;
    if (s->standby_receipt != NULL || receipt->owner != NULL ||
        receipt->epoch != 0U)
        return NX_ERROR_BUSY;
    if (!s->opened || s->closing || !s->suspended || s->tainted ||
        s->auxiliary_faulted || s->active != NULL || s->sram_lease != NULL)
        return NX_ERROR_STATE;
    if (s->standby_epoch == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!live(s))
        return NX_ERROR_STATE;
    uint32_t cr = NX_F407_RTC_READ(&NX_F407_RTC->CR);
    uint32_t tafcr = NX_F407_RTC_READ(&NX_F407_RTC->TAFCR);
    /* An enabled Alarm A without its joined finite owner is foreign. */
    if ((cr & RTC_CR_ALRAE) != 0U)
        return NX_ERROR_BUSY;
    uint32_t allowed = standby_flags(discard);
    uint32_t flags =
        NX_F407_RTC_READ(&NX_F407_RTC->ISR) & (AUX_STATUS | RTC_ISR_ALRAF);
    uint32_t pr = NX_F407_RTC_READ(&NX_F407_RTC_EXTI->PR) & s->wake_lines;
    if ((flags & ~allowed) != 0U ||
        (standby_pending_events(pr, cr, tafcr) & ~discard) != 0U)
        return NX_ERROR_BUSY;
    s->standby_cr = cr;
    s->standby_tafcr = tafcr;
    ++s->standby_epoch;
    s->standby_receipt = receipt;
    s->standby_phase = STANDBY_PREPARING;
    *receipt = (nx_rtc_standby_t){s, s->standby_epoch};
    if (!checked(&NX_F407_RTC->CR, cr & ~STANDBY_CR) ||
        !checked(&NX_F407_RTC->TAFCR, tafcr & ~STANDBY_TAFCR))
        return NX_ERROR_IO;
    const uint32_t ready = RTC_ISR_ALRAWF | RTC_ISR_ALRBWF | RTC_ISR_WUTWF;
    bool observed = false;
    for (uint32_t poll = 0U; poll < limit; ++poll) {
        if ((NX_F407_RTC_READ(&NX_F407_RTC->ISR) & ready) == ready) {
            observed = true;
            break;
        }
    }
    if (!observed)
        return NX_ERROR_TIMEOUT;
    if (!standby_hardware(s))
        return NX_ERROR_STATE;
    flags = NX_F407_RTC_READ(&NX_F407_RTC->ISR) & (AUX_STATUS | RTC_ISR_ALRAF);
    pr = NX_F407_RTC_READ(&NX_F407_RTC_EXTI->PR) & s->wake_lines;
    if ((flags & ~allowed) != 0U ||
        (standby_pending_events(pr, cr, tafcr) & ~discard) != 0U)
        return NX_ERROR_BUSY;
    if (flags != 0U && !clear_status(flags))
        return NX_ERROR_BUSY;
    if (pr != 0U) {
        NX_F407_RTC_WRITE(&NX_F407_RTC_EXTI->PR, pr);
        nx_arch_dsb();
    }
    /* Never loop to consume a newly crossing event, even when its class was
     * authorized earlier. Caller restores and chooses its next disposition. */
    if ((NX_F407_RTC_READ(&NX_F407_RTC->ISR) & (AUX_STATUS | RTC_ISR_ALRAF)) !=
            0U ||
        (NX_F407_RTC_READ(&NX_F407_RTC_EXTI->PR) & s->wake_lines) != 0U)
        return NX_ERROR_BUSY;
    if (NX_F407_RTC_READ(&NX_F407_RTC->CR) != (cr & ~STANDBY_CR) ||
        NX_F407_RTC_READ(&NX_F407_RTC->TAFCR) != (tafcr & ~STANDBY_TAFCR))
        return NX_ERROR_IO;
    s->standby_phase = STANDBY_DISABLED;
    return NX_SUCCESS;
}
/** \brief Prove disabled sources/routes/independent LSE without touching flags.
 */
static nx_result_t standby_validate(void* raw,
                                    const nx_rtc_standby_t* receipt) {
    nx_stm32_rtc_state_t* s = raw;
    if (!standby_context())
        return NX_ERROR_CONTEXT;
    nx_result_t result = standby_identity(s, receipt);
    if (result != NX_SUCCESS)
        return result;
    return s->standby_phase == STANDBY_DISABLED &&
                   NX_F407_RTC_READ(&NX_F407_RTC->CR) ==
                       (s->standby_cr & ~STANDBY_CR) &&
                   NX_F407_RTC_READ(&NX_F407_RTC->TAFCR) ==
                       (s->standby_tafcr & ~STANDBY_TAFCR)
               ? NX_SUCCESS
               : NX_ERROR_STATE;
}
/** \brief Restore original controls only; caller owns PWR and all flag policy.
 */
static nx_result_t standby_sources_restore(nx_stm32_rtc_state_t* s) {
    if (!checked(&NX_F407_RTC->TAFCR, s->standby_tafcr) ||
        !checked(&NX_F407_RTC->CR, s->standby_cr))
        return NX_ERROR_IO;
    nx_result_t result = capture_input(s, s->standby_tafcr);
    if (result != NX_SUCCESS)
        return result;
    return standby_hardware(s) ? NX_SUCCESS : NX_ERROR_STATE;
}
static nx_result_t standby_commit(void* raw, nx_rtc_standby_t* receipt) {
    nx_stm32_rtc_state_t* s = raw;
    if (!standby_context())
        return NX_ERROR_CONTEXT;
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask != 1U || masks.basepri != 0U || masks.faultmask != 0U)
        return NX_ERROR_CONTEXT;
    nx_result_t result = standby_validate(raw, receipt);
    if (result != NX_SUCCESS)
        return result;
    if ((NX_F407_RTC_READ(&NX_F407_RTC_PWR->CSR) & PWR_CSR_WUF) != 0U)
        return NX_ERROR_BUSY;
    s->standby_phase = STANDBY_COMMITTING;
    result = standby_sources_restore(s);
    if (result == NX_SUCCESS)
        s->standby_phase = STANDBY_COMMITTED;
    return result;
}
/** \brief Failed prepare/commit remains retryable across the System epoch. */
static nx_result_t standby_restore(void* raw, nx_rtc_standby_t* receipt) {
    nx_stm32_rtc_state_t* s = raw;
    if (!standby_context())
        return NX_ERROR_CONTEXT;
    if (!owner(s) || !external(s, receipt, sizeof(*receipt),
                               _Alignof(nx_rtc_standby_t), true))
        return NX_ERROR_INVALID;
    if (s->standby_receipt == NULL && receipt->owner == NULL &&
        receipt->epoch == 0U)
        return NX_SUCCESS;
    nx_result_t result = standby_identity(s, receipt);
    if (result != NX_SUCCESS)
        return result;
    result = standby_sources_restore(s);
    if (result != NX_SUCCESS)
        return result;
    s->standby_receipt = NULL;
    s->standby_phase = 0U;
    nx_arch_dsb();
    *receipt = (nx_rtc_standby_t){0};
    return NX_SUCCESS;
}
static const nx_rtc_aux_ops_t auxiliary_ops = {
    alarm_b,        wakeup,         capture_configure, capture_read,
    events,         acknowledge,    calibrate,         wake_route,
    backup_read,    backup_write,   sram_prepare,      sram_acquire,
    sram_release,   handoff,        standby_prepare,   standby_validate,
    standby_commit, standby_restore};
const nx_rtc_ops_t nx_stm32_rtc_ops = {
    read_calendar, set_calendar, submit, cancel, service, stop, &auxiliary_ops};
/** \brief Empty means reset-like calendar/control, not INITS == 0 alone. */
static bool empty(void) {
    return NX_F407_RTC_READ(&NX_F407_RTC_RCC->BDCR) == 0U &&
           NX_F407_RTC_READ(&NX_F407_RTC->TR) == 0U &&
           NX_F407_RTC_READ(&NX_F407_RTC->DR) == 0x2101U &&
           NX_F407_RTC_READ(&NX_F407_RTC->CR) == 0U &&
           NX_F407_RTC_READ(&NX_F407_RTC->PRER) == PROFILE &&
           NX_F407_RTC_READ(&NX_F407_RTC->CALR) == 0U &&
           NX_F407_RTC_READ(&NX_F407_RTC->CALIBR) == 0U &&
           NX_F407_RTC_READ(&NX_F407_RTC->TAFCR) == 0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC->ISR) &
            (FOREIGN_STATUS | RTC_ISR_ALRAF | RTC_ISR_INIT | RTC_ISR_INITF |
             RTC_ISR_INITS)) == 0U;
}
/** \brief Restore only the temporary inspection gate, preserving other users.
 */
static bool restore_inspection(nx_stm32_rtc_state_t* s) {
    if (!(s->effects & EFFECT_GPIOC_INSPECTION))
        return true;
    uint32_t current = NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR);
    NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->AHB1ENR,
                      current & ~(uint32_t)RCC_AHB1ENR_GPIOCEN);
    nx_arch_dsb();
    if (NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) & RCC_AHB1ENR_GPIOCEN)
        return false;
    s->effects &= ~(uint32_t)EFFECT_GPIOC_INSPECTION;
    return true;
}
/** \brief Temporarily observe oscillator pads through their real GPIO gate. */
static bool inspection_clock(nx_stm32_rtc_state_t* s) {
    uint32_t current = NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR);
    if (!(current & RCC_AHB1ENR_GPIOCEN)) {
        s->effects |= EFFECT_GPIOC_INSPECTION;
        NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->AHB1ENR,
                          current | RCC_AHB1ENR_GPIOCEN);
        nx_arch_dsb();
        return (NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) &
                RCC_AHB1ENR_GPIOCEN) != 0U;
    }
    return true;
}
/** \brief Return the quiet pad only before any crystal LSE could own it. */
static bool restore_quiet_pad(nx_stm32_rtc_state_t* s) {
    if (!(s->effects & EFFECT_QUIET_PC13) ||
        (NX_F407_RTC_READ(&NX_F407_RTC_RCC->BDCR) &
         (RCC_BDCR_LSEON | RCC_BDCR_LSERDY)) != 0U)
        return true;
    if (!inspection_clock(s))
        return false;
    const uint32_t mask = UINT32_C(3) << 26U;
    uint32_t mode = NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->MODER);
    NX_F407_RTC_WRITE(&NX_F407_RTC_GPIOC->MODER,
                      (mode & ~mask) | ((uint32_t)s->saved_pc13 << 26U));
    nx_arch_dsb();
    if ((NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->MODER) & mask) !=
        ((uint32_t)s->saved_pc13 << 26U))
        return false;
    s->effects &= ~(uint32_t)EFFECT_QUIET_PC13;
    return true;
}
/** \brief Enforce crystal quietness before BDCR effects or retained adoption.
 * \note ES0182 2.2.18 requires digital PC13 silence while crystal LSE runs.
 *       Provision can acquire a free input as analog; adoption requires an
 *       already quiet pad. The persistent pad follows oscillator handoff.
 */
static nx_result_t inspect_oscillator_pads(nx_stm32_rtc_state_t* s,
                                           const nx_stm32_rtc_config_t* c) {
    if (!inspection_clock(s))
        return NX_ERROR_IO;
    uint32_t mode = NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->MODER);
    uint32_t pull = NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->PUPDR);
    uint32_t lock_bits = NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->LCKR);
    uint32_t oscillator = c->lse_bypass ? UINT32_C(0x4000) : UINT32_C(0xc000);
    uint32_t pull_mask =
        c->lse_bypass ? UINT32_C(0x30000000) : UINT32_C(0xf0000000);
    bool good = (lock_bits & oscillator) == 0U && (pull & pull_mask) == 0U;
    for (uint32_t pin = 14U; pin < (c->lse_bypass ? 15U : 16U); ++pin) {
        uint32_t selected = (mode >> (pin * 2U)) & 3U;
        good = good && (selected == 0U || selected == 3U);
    }
    if (!c->lse_bypass) {
        uint32_t pc13 = (mode >> 26U) & 3U;
        good = good && (lock_bits & (1U << 13U)) == 0U &&
               (pull & (UINT32_C(3) << 26U)) == 0U &&
               (pc13 == 3U ||
                (pc13 == 0U && c->policy == NX_STM32_RTC_PROVISION_EMPTY));
        if (good && c->policy == NX_STM32_RTC_PROVISION_EMPTY) {
            s->saved_pc13 = (uint8_t)pc13;
            s->effects |= EFFECT_QUIET_PC13;
            NX_F407_RTC_WRITE(&NX_F407_RTC_GPIOC->MODER,
                              mode | (UINT32_C(3) << 26U));
            nx_arch_dsb();
            if (((NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->MODER) >> 26U) & 3U) !=
                3U)
                return NX_ERROR_IO;
        }
    }
    if (!restore_inspection(s))
        return NX_ERROR_IO;
    return good ? NX_SUCCESS : NX_ERROR_BUSY;
}

nx_result_t nx_stm32_rtc_prepare(nx_stm32_rtc_state_t* s,
                                 const nx_stm32_rtc_config_t* c) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s) ||
        !outside_private(s, c, sizeof(*c), _Alignof(nx_stm32_rtc_config_t)) ||
        c->clock.read == NULL || c->timeout_us == 0U || c->poll_limit == 0U ||
        (c->policy != NX_STM32_RTC_ADOPT_RETAINED &&
         c->policy != NX_STM32_RTC_PROVISION_EMPTY) ||
        (c->policy == NX_STM32_RTC_PROVISION_EMPTY &&
         (!c->has_initial ||
          nx_rtc_calendar_validate(&c->initial) != NX_SUCCESS)) ||
        (c->policy == NX_STM32_RTC_ADOPT_RETAINED && c->has_initial))
        return NX_ERROR_INVALID;
    if (s->opened || s->active || s->effects || s->tainted ||
        s->auxiliary_dirty || s->sram_lease || s->sram_effects)
        return NX_ERROR_BUSY;
    if (s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!system_live())
        return NX_ERROR_STATE;
    if (!quiet())
        return NX_ERROR_BUSY;
    nx_rtc_calendar_t retained;
    if (c->policy == NX_STM32_RTC_ADOPT_RETAINED) {
        if (!live_source(s, c->lse_bypass) ||
            (NX_F407_RTC_READ(&NX_F407_RTC->CR) & RTC_CR_ALRAE) ||
            (NX_F407_RTC_READ(&NX_F407_RTC->ISR) &
             (FOREIGN_STATUS | RTC_ISR_ALRAF)))
            return NX_ERROR_STATE;
        nx_result_t r = calendar(&retained);
        if (r != NX_SUCCESS)
            return r;
    } else {
        if (!empty())
            return NX_ERROR_STATE;
    }
    nx_time_us_t deadline;
    nx_result_t r = nx_deadline_finite_after(c->clock.read(c->clock.context),
                                             c->timeout_us, &deadline);
    if (r != NX_SUCCESS)
        return r;
    nx_result_t inspection = inspect_oscillator_pads(s, c);
    if (inspection != NX_SUCCESS)
        return inspection;
    s->config = c;
    if (c->policy == NX_STM32_RTC_PROVISION_EMPTY) {
        uint32_t power = NX_F407_RTC_READ(&NX_F407_RTC_RCC->APB1ENR);
        if (!(power & RCC_APB1ENR_PWREN)) {
            s->effects |= EFFECT_POWER;
            NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->APB1ENR,
                              power | RCC_APB1ENR_PWREN);
            if (!(NX_F407_RTC_READ(&NX_F407_RTC_RCC->APB1ENR) &
                  RCC_APB1ENR_PWREN))
                return NX_ERROR_IO;
        }
        power = NX_F407_RTC_READ(&NX_F407_RTC_PWR->CR);
        if (!(power & PWR_CR_DBP)) {
            s->effects |= EFFECT_ACCESS;
            NX_F407_RTC_WRITE(&NX_F407_RTC_PWR->CR, power | PWR_CR_DBP);
            if (!(NX_F407_RTC_READ(&NX_F407_RTC_PWR->CR) & PWR_CR_DBP))
                return NX_ERROR_IO;
        }
        s->effects |= EFFECT_LSE;
        uint32_t source = c->lse_bypass ? RCC_BDCR_LSEBYP : 0U;
        NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->BDCR, source | RCC_BDCR_LSEON);
        r = wait_bits(c, &NX_F407_RTC_RCC->BDCR, RCC_BDCR_LSERDY,
                      RCC_BDCR_LSERDY, deadline);
        if (r != NX_SUCCESS)
            return r;
        s->effects |= EFFECT_SOURCE | EFFECT_RTC;
        NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->BDCR, BACKUP_LIVE | source);
        if ((NX_F407_RTC_READ(&NX_F407_RTC_RCC->BDCR) & BACKUP_MASK) !=
            (BACKUP_LIVE | source))
            return NX_ERROR_IO;
        s->effects |= EFFECT_CALENDAR;
        r = program_calendar(s, &c->initial, deadline, true);
        if (r != NX_SUCCESS) {
            s->tainted = true;
            return r;
        }
    }
    s->saved_alarm = NX_F407_RTC_READ(&NX_F407_RTC->ALRMAR);
    s->saved_subsecond = NX_F407_RTC_READ(&NX_F407_RTC->ALRMASSR);
    s->saved_alarm_b = NX_F407_RTC_READ(&NX_F407_RTC->ALRMBR);
    s->saved_subsecond_b = NX_F407_RTC_READ(&NX_F407_RTC->ALRMBSSR);
    s->saved_wakeup = NX_F407_RTC_READ(&NX_F407_RTC->WUTR);
    s->opened = true;
    s->closing = false;
    s->terminal_set = false;
    s->stage = IDLE;
    ++s->generation;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_rtc_close(nx_stm32_rtc_state_t* s) {
    nx_result_t r = stop(s);
    if (r != NX_SUCCESS)
        return r;
    if (!restore_quiet_pad(s) || !restore_inspection(s))
        return NX_ERROR_BUSY;
    return s->effects ? NX_ERROR_BUSY : NX_SUCCESS;
}
uint32_t nx_stm32_rtc_effects(const nx_stm32_rtc_state_t* s) {
    return owner(s) ? s->effects | ((uint32_t)s->sram_effects << 8U) |
                          (s->auxiliary_dirty ? UINT32_C(0x400) : 0U) |
                          (s->sram_lease != NULL ? UINT32_C(0x800) : 0U) |
                          (s->suspended ? UINT32_C(0x1000) : 0U) |
                          (s->standby_receipt != NULL ? UINT32_C(0x2000) : 0U)
                    : 0U;
}
/** \brief No asynchronous software user may span the Core epoch boundary. */
nx_result_t nx_stm32_rtc_suspend(nx_stm32_rtc_state_t* s) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s))
        return NX_ERROR_INVALID;
    if (s->active != NULL || s->sram_lease != NULL)
        return NX_ERROR_BUSY;
    if (!s->opened || s->closing || s->suspended || s->tainted ||
        s->auxiliary_faulted)
        return NX_ERROR_STATE;
    if (s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!live(s))
        return NX_ERROR_STATE;
    s->suspended = true;
    return NX_SUCCESS;
}
/** \brief A fresh admission identity invalidates prepared old Core deadlines.
 */
nx_result_t nx_stm32_rtc_resume(nx_stm32_rtc_state_t* s) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!owner(s))
        return NX_ERROR_INVALID;
    if (s->standby_receipt != NULL)
        return NX_ERROR_BUSY;
    if (!s->opened || s->closing || !s->suspended || s->tainted ||
        s->auxiliary_faulted || !live(s))
        return NX_ERROR_STATE;
    if (s->sram_ready &&
        (!(NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) &
           RCC_AHB1ENR_BKPSRAMEN) ||
         (s->sram_retention &&
          (NX_F407_RTC_READ(&NX_F407_RTC_PWR->CSR) &
           (PWR_CSR_BRE | PWR_CSR_BRR)) != (PWR_CSR_BRE | PWR_CSR_BRR))))
        return NX_ERROR_STATE;
    if (s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    ++s->generation;
    s->suspended = false;
    return NX_SUCCESS;
}
