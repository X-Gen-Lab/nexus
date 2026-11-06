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
    EFFECT_GPIOC_INSPECTION = 64U
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
                !OVERLAP(&g_nx_stm32_system, sizeof(g_nx_stm32_system)) &&
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
            !nx_time_overlap(p, length, s->active, sizeof(*s->active)));
}
static bool quiet(void) {
    const uint32_t line = 1U << 17U;
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
static bool live(void) {
    return system_live() && quiet() &&
           (NX_F407_RTC_READ(&NX_F407_RTC_RCC->APB1ENR) & RCC_APB1ENR_PWREN) &&
           (NX_F407_RTC_READ(&NX_F407_RTC_PWR->CR) & PWR_CR_DBP) &&
           (NX_F407_RTC_READ(&NX_F407_RTC_RCC->BDCR) & BACKUP_MASK) ==
               BACKUP_LIVE &&
           (NX_F407_RTC_READ(&NX_F407_RTC->CR) & ~(uint32_t)RTC_CR_ALRAE) ==
               RTC_CR_BYPSHAD &&
           NX_F407_RTC_READ(&NX_F407_RTC->PRER) == PROFILE &&
           NX_F407_RTC_READ(&NX_F407_RTC->CALR) == 0U &&
           NX_F407_RTC_READ(&NX_F407_RTC->CALIBR) == 0U &&
           NX_F407_RTC_READ(&NX_F407_RTC->TAFCR) == 0U &&
           (NX_F407_RTC_READ(&NX_F407_RTC->ISR) &
            (FOREIGN_STATUS | RTC_ISR_INIT | RTC_ISR_INITF)) == 0U;
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
    else if (p == &NX_F407_RTC->CR && value == RTC_CR_BYPSHAD) {
        LL_RTC_ALMA_Disable(NX_F407_RTC);
        LL_RTC_EnableShadowRegBypass(NX_F407_RTC);
    } else if (p == &NX_F407_RTC->CR &&
               value == (RTC_CR_BYPSHAD | RTC_CR_ALRAE))
        LL_RTC_ALMA_Enable(NX_F407_RTC);
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
/** \brief Programming starts only after admission and INITF. No rollback date.
 */
static nx_result_t program_calendar(nx_stm32_rtc_state_t* s,
                                    const nx_rtc_calendar_t* c,
                                    nx_time_us_t deadline, bool provision) {
    RTC_TypeDef* p = NX_F407_RTC;
    status_write(true, false);
    nx_result_t r =
        wait_bits(s->config, &p->ISR, RTC_ISR_INITF, RTC_ISR_INITF, deadline);
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
    if (!s->opened || s->closing || s->tainted || !live())
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
    if (!s->opened || s->closing || s->tainted || !live())
        return NX_ERROR_STATE;
    if (!s->config->allow_set)
        return NX_ERROR_UNSUPPORTED;
    if (s->active != NULL)
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
    if (!s->opened || s->closing || s->tainted)
        return NX_ERROR_STATE;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    if (!live() || NX_F407_RTC_READ(&NX_F407_RTC->CR) != RTC_CR_BYPSHAD ||
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
    if (s->tainted || !live()) {
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
            good = checked(&p->CR, RTC_CR_BYPSHAD);
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
            good = checked(&p->CR, RTC_CR_BYPSHAD | RTC_CR_ALRAE);
            if (good)
                s->stage = RUN;
            break;
        case RUN:
            if (NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAF)
                finish(s, NX_SUCCESS);
            break;
        case WITHDRAW:
            if (!checked(&p->CR, RTC_CR_BYPSHAD))
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
                (NX_F407_RTC_READ(&p->ISR) & RTC_ISR_ALRAF) || !live())
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
    s->closing = true;
    if (s->active != NULL) {
        finish(s, NX_ERROR_CANCELLED);
        (void)service(s);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    if (!s->opened)
        return s->tainted ? NX_ERROR_BUSY : NX_SUCCESS;
    if (s->tainted || !live() ||
        NX_F407_RTC_READ(&NX_F407_RTC->CR) != RTC_CR_BYPSHAD ||
        (NX_F407_RTC_READ(&NX_F407_RTC->ISR) & RTC_ISR_ALRAF))
        return NX_ERROR_BUSY;
    s->opened = false;
    return NX_SUCCESS;
}
const nx_rtc_ops_t nx_stm32_rtc_ops = {read_calendar, set_calendar, submit,
                                       cancel,        service,      stop};
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
/** \brief LSE overrides PC14/15: exclude foreign pad owners before BDCR writes.
 * \note Powered-off GPIO cannot prove pad state. The cold owner temporarily
 *       enables GPIOC, observes without changing pads, then restores its gate.
 *       A failed observation/restore retains the exact gate responsibility.
 */
static nx_result_t inspect_oscillator_pads(nx_stm32_rtc_state_t* s) {
    uint32_t current = NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR);
    if (!(current & RCC_AHB1ENR_GPIOCEN)) {
        s->effects |= EFFECT_GPIOC_INSPECTION;
        NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->AHB1ENR,
                          current | RCC_AHB1ENR_GPIOCEN);
        nx_arch_dsb();
        if (!(NX_F407_RTC_READ(&NX_F407_RTC_RCC->AHB1ENR) &
              RCC_AHB1ENR_GPIOCEN))
            return NX_ERROR_IO;
    }
    uint32_t mode = NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->MODER);
    uint32_t pull = NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->PUPDR);
    uint32_t lock_bits = NX_F407_RTC_READ(&NX_F407_RTC_GPIOC->LCKR);
    bool good = (lock_bits & UINT32_C(0xc000)) == 0U &&
                (pull & UINT32_C(0xf0000000)) == 0U;
    for (uint32_t pin = 14U; pin < 16U; ++pin) {
        uint32_t selected = (mode >> (pin * 2U)) & 3U;
        good = good && (selected == 0U || selected == 3U);
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
    if (s->opened || s->active || s->effects || s->tainted)
        return NX_ERROR_BUSY;
    if (s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!system_live())
        return NX_ERROR_STATE;
    if (!quiet())
        return NX_ERROR_BUSY;
    nx_rtc_calendar_t retained;
    if (c->policy == NX_STM32_RTC_ADOPT_RETAINED) {
        if (!live() || NX_F407_RTC_READ(&NX_F407_RTC->CR) != RTC_CR_BYPSHAD ||
            (NX_F407_RTC_READ(&NX_F407_RTC->ISR) & RTC_ISR_ALRAF))
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
    if (c->policy == NX_STM32_RTC_PROVISION_EMPTY) {
        nx_result_t inspection = inspect_oscillator_pads(s);
        if (inspection != NX_SUCCESS)
            return inspection;
    }
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
        NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->BDCR, RCC_BDCR_LSEON);
        r = wait_bits(c, &NX_F407_RTC_RCC->BDCR, RCC_BDCR_LSERDY,
                      RCC_BDCR_LSERDY, deadline);
        if (r != NX_SUCCESS)
            return r;
        s->effects |= EFFECT_SOURCE | EFFECT_RTC;
        NX_F407_RTC_WRITE(&NX_F407_RTC_RCC->BDCR, BACKUP_LIVE);
        if ((NX_F407_RTC_READ(&NX_F407_RTC_RCC->BDCR) & BACKUP_MASK) !=
            BACKUP_LIVE)
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
    if (!restore_inspection(s))
        return NX_ERROR_BUSY;
    return s->effects ? NX_ERROR_BUSY : NX_SUCCESS;
}
uint32_t nx_stm32_rtc_effects(const nx_stm32_rtc_state_t* s) {
    return owner(s) ? s->effects : 0U;
}
