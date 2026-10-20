/**
 * \file            rtc.c
 * \brief           Retained U575 calendar and finite polling Alarm A
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "rtc_lptim_internal.h"
#include "stm32u575_rtc.h"
#include "stm32u575_rtc_calendar.h"
#include "stm32u5xx_ll_rtc.h"
_Static_assert(RTC_IRQn == 2 && offsetof(RTC_TypeDef, ALRMAR) == 0x40U &&
                   offsetof(RTC_TypeDef, SCR) == 0x5cU &&
                   LL_RTC_ALMA_MASK_NONE == 0U,
               "U575 retained RTC register generation changed");
#ifndef NX_U575_RTC
#define NX_U575_RTC RTC
#endif
#ifndef NX_U575_RTC_PWR
#define NX_U575_RTC_PWR PWR
#endif
#ifndef NX_U575_RTC_IRQ_QUIET
#define NX_U575_RTC_IRQ_QUIET()                                                \
    (NVIC_GetEnableIRQ(RTC_IRQn) == 0U &&                                      \
     NVIC_GetPendingIRQ(RTC_IRQn) == 0U && NVIC_GetActive(RTC_IRQn) == 0U)
#endif
enum {
    IDLE,
    SOURCE_OFF,
    SOURCE_OFF_WAIT,
    PROGRAM,
    ARM,
    ARM_WAIT,
    RUN,
    WITHDRAW,
    OFF_WAIT,
    CLEAR,
    RESTORE
};
/** \brief The owner is single-task and never publishes an IRQ callback. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Complete private spans precede state or hardware access. */
static bool valid(const nx_stm32u575_rtc_state_t* s) {
    return nx_time_span(s, sizeof(*s), _Alignof(nx_stm32u575_rtc_state_t)) &&
           s->registers == NX_U575_RTC && s->power == NX_U575_RTC_PWR &&
           g_nx_stm32u575_system.rcc != NULL;
}
/** \brief Opaque clock context extents remain an assembly obligation. */
static bool external(const nx_stm32u575_rtc_state_t* s, const void* pointer,
                     size_t size, size_t alignment, bool protect_active) {
    if (!nx_time_span(pointer, size, alignment))
        return false;
#define OVERLAP(base, length) nx_time_overlap(pointer, size, (base), (length))
    bool good =
        !OVERLAP(s, sizeof(*s)) &&
        !OVERLAP(s->registers, sizeof(*s->registers)) &&
        !OVERLAP(s->power, sizeof(*s->power)) &&
        !OVERLAP(&g_nx_stm32u575_system, sizeof(g_nx_stm32u575_system)) &&
        !OVERLAP(g_nx_stm32u575_system.rcc, sizeof(RCC_TypeDef)) &&
        (!protect_active || s->active == NULL ||
         !OVERLAP(s->active, sizeof(*s->active)));
#undef OVERLAP
    return good;
}
/** \brief No backup clock, write-access or foreign configuration is adopted. */
static bool prepared(const nx_stm32u575_rtc_state_t* s) {
    const nx_stm32u575_system_t* system = &g_nx_stm32u575_system;
    RCC_TypeDef* r = system->rcc;
    uint32_t backup = NX_U575_READ(&r->BDCR);
    return (NX_U575_READ(&r->AHB3ENR) & RCC_AHB3ENR_PWREN) != 0U &&
           nx_stm32u575_rtc_access(r) &&
           /* ES0499 Rev12 2.2.3/16 affect revision X. This reference does
            * not qualify a revision, so admit only drive codes 10/11 in crystal
            * mode. */
           (backup & RCC_BDCR_LSEDRV_1) != 0U &&
           (backup & (RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL |
                      RCC_BDCR_RTCEN | RCC_BDCR_BDRST | RCC_BDCR_LSEBYP)) ==
               (RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL_0 |
                RCC_BDCR_RTCEN) &&
           (NX_U575_READ(&s->power->DBPR) & PWR_DBPR_DBP) != 0U;
}
/** \brief Observe the exact prepared mode throughout every accepted loan. */
static bool live(const nx_stm32u575_rtc_state_t* s) {
    RTC_TypeDef* p = s->registers;
    return g_nx_stm32u575_system.started &&
           g_nx_stm32u575_system.timer_error == NX_SUCCESS && prepared(s) &&
           (NX_U575_READ(&p->CR) & ~(uint32_t)RTC_CR_ALRAE) == RTC_CR_BYPSHAD &&
           NX_U575_READ(&p->PRER) == 0x007f00ffU &&
           NX_U575_READ(&p->CALR) == 0U &&
           (NX_U575_READ(&p->SR) & ~(uint32_t)RTC_SR_ALRAF) == 0U &&
           (NX_U575_READ(&p->ICSR) &
            (RTC_ICSR_INIT | RTC_ICSR_INITF | RTC_ICSR_BIN | RTC_ICSR_BCDU |
             RTC_ICSR_SHPF | RTC_ICSR_RECALPF)) == 0U &&
           NX_U575_RTC_IRQ_QUIET();
}
/** \brief Reuse the same validated tuple as the independent clock owner. */
static nx_result_t calendar(const nx_stm32u575_rtc_state_t* s,
                            nx_rtc_calendar_t* out) {
    nx_stm32u575_rtc_tuple_t tuple;
    nx_result_t result = nx_stm32u575_rtc_tuple_read(
        s->registers, g_nx_stm32u575_system.rcc, &tuple);
    if (result == NX_SUCCESS)
        *out = tuple.calendar;
    return result;
}
nx_result_t nx_stm32u575_rtc_preflight(void) {
    if (!task())
        return NX_ERROR_CONTEXT;
    nx_stm32u575_rtc_state_t s = {.registers = NX_U575_RTC,
                                  .power = NX_U575_RTC_PWR};
    if (!valid(&s))
        return NX_ERROR_INVALID;
    if (!prepared(&s))
        return NX_ERROR_STATE;
    if (!NX_U575_RTC_IRQ_QUIET() ||
        NX_U575_READ(&s.registers->CR) != RTC_CR_BYPSHAD ||
        NX_U575_READ(&s.registers->SR) != 0U)
        return NX_ERROR_BUSY;
    RTC_TypeDef* p = s.registers;
    if (NX_U575_READ(&p->PRER) != 0x007f00ffU || NX_U575_READ(&p->CALR) != 0U ||
        (NX_U575_READ(&p->ICSR) &
         (RTC_ICSR_INIT | RTC_ICSR_INITF | RTC_ICSR_BIN | RTC_ICSR_BCDU |
          RTC_ICSR_SHPF | RTC_ICSR_RECALPF)) != 0U)
        return NX_ERROR_STATE;
    nx_rtc_calendar_t retained;
    return calendar(&s, &retained);
}
/** \brief The first terminal cause remains stable throughout real drain. */
static void finish(nx_stm32u575_rtc_state_t* s, nx_result_t result) {
    if (!s->terminal_set) {
        s->terminal = result;
        s->terminal_set = true;
    }
    if (s->stage < WITHDRAW)
        s->stage = WITHDRAW;
    if (s->active != NULL)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
}
/** \brief A failed hardware observation retains the request and owner. */
static nx_result_t held(nx_stm32u575_rtc_state_t* s) {
    if (s->active != NULL)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
    return NX_ERROR_BUSY;
}
/** \brief Foreign clock or register mutation cannot be repaired by restore. */
static void taint(nx_stm32u575_rtc_state_t* s) {
    s->tainted = true;
    finish(s, NX_ERROR_STATE);
}
/** \brief Lease WPR only for one observed write, and always restore locked. */
static bool checked(nx_stm32u575_rtc_state_t* s, volatile uint32_t* reg,
                    uint32_t value) {
    /* WPR reads always return zero. Exact admission therefore requires the
     * external owner to hand off locked WPR with no other writer; it cannot
     * silently adopt or reconstruct an unknown protection state. */
    NX_U575_WRITE(&s->registers->WPR, 0xcaU);
    NX_U575_WRITE(&s->registers->WPR, 0x53U);
    nx_arch_dsb();
    NX_U575_WRITE(reg, value);
    nx_arch_dsb();
    bool good = NX_U575_READ(reg) == value;
    NX_U575_WRITE(&s->registers->WPR, 0xffU);
    nx_arch_dsb();
    return good;
}
static nx_result_t read_calendar(const void* raw, nx_rtc_snapshot_t* out) {
    nx_stm32u575_rtc_state_t* s = (nx_stm32u575_rtc_state_t*)raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s) ||
        !external(s, out, sizeof(*out), _Alignof(nx_rtc_snapshot_t), true))
        return NX_ERROR_INVALID;
    if (!s->opened || s->closing || s->tainted)
        return NX_ERROR_STATE;
    if (!live(s)) {
        if (s->active != NULL)
            taint(s);
        return NX_ERROR_STATE;
    }
    nx_rtc_snapshot_t value = {.domain = {s, s->generation}};
    nx_result_t result = calendar(s, &value.calendar);
    if (result == NX_SUCCESS)
        *out = value;
    return result;
}
/** \brief All rejection checks precede request admission and hardware effects.
 */
static nx_result_t submit(void* raw, nx_rtc_request_t* request) {
    nx_stm32u575_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s) || !external(s, request, sizeof(*request),
                               _Alignof(nx_rtc_request_t), false))
        return NX_ERROR_INVALID;
    if (!s->opened || s->closing || s->tainted)
        return NX_ERROR_STATE;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    if (!live(s) || NX_U575_READ(&s->registers->CR) != RTC_CR_BYPSHAD ||
        NX_U575_READ(&s->registers->SR) != 0U)
        return NX_ERROR_STATE;
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        request->domain.identity != s ||
        request->domain.generation != s->generation)
        return NX_ERROR_STATE;
    if (nx_rtc_alarm_validate(&request->alarm) != NX_SUCCESS)
        return NX_ERROR_INVALID;
    nx_rtc_calendar_t current;
    nx_result_t result = calendar(s, &current);
    if (result != NX_SUCCESS)
        return result;
    if (current.day == request->alarm.day &&
        current.hour == request->alarm.hour &&
        current.minute == request->alarm.minute &&
        current.second == request->alarm.second)
        return NX_ERROR_STATE;
    if (request->base.deadline != NX_DEADLINE_NEVER &&
        nx_deadline_expired(request->base.deadline,
                            s->clock.read(s->clock.context)))
        return NX_ERROR_TIMEOUT;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        s->active = request;
        s->stage = SOURCE_OFF;
        s->terminal_set = false;
    }
    return result;
}
static nx_result_t cancel(void* raw, nx_rtc_request_t* request) {
    nx_stm32u575_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s))
        return NX_ERROR_INVALID;
    if (request == NULL || request != s->active)
        return NX_ERROR_STATE;
    finish(s, NX_ERROR_CANCELLED);
    return NX_SUCCESS;
}
/** \brief One phase per call, with no polling loop or hidden wait. */
static nx_result_t service(void* raw) {
    nx_stm32u575_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s))
        return NX_ERROR_INVALID;
    if (s->active == NULL)
        return NX_SUCCESS;
    if (s->tainted || !live(s)) {
        taint(s);
        return held(s);
    }
    if (!s->terminal_set && s->active->base.deadline != NX_DEADLINE_NEVER &&
        nx_deadline_expired(s->active->base.deadline,
                            s->clock.read(s->clock.context)))
        finish(s, NX_ERROR_TIMEOUT);
    RTC_TypeDef* p = s->registers;
    bool good = true;
    switch (s->stage) {
        case SOURCE_OFF:
            good = checked(s, &p->CR, RTC_CR_BYPSHAD);
            if (good) {
                s->ready_at =
                    nx_deadline_after(s->clock.read(s->clock.context), 100U);
                if (s->ready_at == NX_DEADLINE_NEVER) {
                    finish(s, NX_ERROR_IO);
                    return held(s);
                }
                s->stage = SOURCE_OFF_WAIT;
            }
            break;
        case SOURCE_OFF_WAIT:
            if (nx_deadline_expired(s->ready_at,
                                    s->clock.read(s->clock.context)))
                s->stage = PROGRAM;
            break;
        case PROGRAM: {
            const nx_rtc_alarm_t* alarm = &s->active->alarm;
            uint32_t encoded = __LL_RTC_CONVERT_BIN2BCD(alarm->day) << 24U |
                               __LL_RTC_CONVERT_BIN2BCD(alarm->hour) << 16U |
                               __LL_RTC_CONVERT_BIN2BCD(alarm->minute) << 8U |
                               __LL_RTC_CONVERT_BIN2BCD(alarm->second);
            NX_U575_WRITE(&p->SCR, RTC_SCR_CALRAF);
            nx_arch_dsb();
            good = (NX_U575_READ(&p->SR) & RTC_SR_ALRAF) == 0U &&
                   checked(s, &p->ALRMAR, encoded) &&
                   checked(s, &p->ALRMASSR, 0U);
            if (good)
                s->stage = ARM;
            break;
        }
        case ARM:
            good = checked(s, &p->CR, RTC_CR_BYPSHAD | RTC_CR_ALRAE);
            if (good) {
                s->ready_at =
                    nx_deadline_after(s->clock.read(s->clock.context), 100U);
                if (s->ready_at == NX_DEADLINE_NEVER) {
                    finish(s, NX_ERROR_IO);
                    return held(s);
                }
                s->stage = ARM_WAIT;
            }
            break;
        case ARM_WAIT:
            if (nx_deadline_expired(s->ready_at,
                                    s->clock.read(s->clock.context)))
                s->stage = RUN;
            break;
        case RUN:
            if ((NX_U575_READ(&p->SR) & RTC_SR_ALRAF) != 0U)
                finish(s, NX_SUCCESS);
            break;
        case WITHDRAW:
            if (!checked(s, &p->CR, RTC_CR_BYPSHAD))
                return held(s);
            /* RM0456 63.3.11: CR changes cross approximately two RTCCLK
             * cycles.100us covers three nominal LSE cycles and timer phase.
             * Readback alone never proves that the alarm source has stopped. */
            s->ready_at =
                nx_deadline_after(s->clock.read(s->clock.context), 100U);
            if (s->ready_at == NX_DEADLINE_NEVER)
                return held(s);
            s->stage = OFF_WAIT;
            break;
        case OFF_WAIT:
            if (nx_deadline_expired(s->ready_at,
                                    s->clock.read(s->clock.context)))
                s->stage = CLEAR;
            break;
        case CLEAR:
            NX_U575_WRITE(&p->SCR, RTC_SCR_CALRAF);
            nx_arch_dsb();
            if ((NX_U575_READ(&p->SR) & RTC_SR_ALRAF) != 0U)
                return held(s);
            s->stage = RESTORE;
            break;
        case RESTORE:
            if (!checked(s, &p->ALRMAR, s->saved_alarm) ||
                !checked(s, &p->ALRMASSR, s->saved_subsecond) ||
                NX_U575_READ(&p->CR) != RTC_CR_BYPSHAD ||
                NX_U575_READ(&p->SR) != 0U || !NX_U575_RTC_IRQ_QUIET())
                return held(s);
            {
                nx_rtc_request_t* request = s->active;
                nx_result_t result = s->terminal;
                s->active = NULL;
                s->stage = IDLE;
                s->terminal_set = false;
                /* Detach every reference before release-publication. */
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
    nx_stm32u575_rtc_state_t* s = raw;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s))
        return NX_ERROR_INVALID;
    s->closing = true;
    if (s->active != NULL) {
        finish(s, NX_ERROR_CANCELLED);
        (void)service(s);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    if (!s->retained)
        return NX_SUCCESS;
    if (s->tainted || !live(s) ||
        NX_U575_READ(&s->registers->CR) != RTC_CR_BYPSHAD ||
        NX_U575_READ(&s->registers->SR) != 0U)
        return NX_ERROR_BUSY;
    s->opened = s->retained = false;
    return NX_SUCCESS;
}
const nx_rtc_ops_t nx_stm32u575_rtc_ops = {read_calendar, NULL,    submit,
                                           cancel,        service, stop};
nx_result_t nx_stm32u575_rtc_initialize(nx_stm32u575_rtc_state_t* s,
                                        const nx_clock_t* clock) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(s) ||
        !external(s, clock, sizeof(*clock), _Alignof(nx_clock_t), true) ||
        clock->read == NULL)
        return NX_ERROR_INVALID;
    if (s->active != NULL || s->retained)
        return s->opened && !s->closing ? NX_ERROR_STATE : NX_ERROR_BUSY;
    if (s->generation == UINT64_MAX)
        return NX_ERROR_EXHAUSTED;
    if (!prepared(s))
        return NX_ERROR_STATE;
    if (!NX_U575_RTC_IRQ_QUIET())
        return NX_ERROR_BUSY;
    if (NX_U575_READ(&s->registers->CR) != RTC_CR_BYPSHAD ||
        NX_U575_READ(&s->registers->SR) != 0U)
        return NX_ERROR_BUSY;
    if (!live(s))
        return NX_ERROR_STATE;
    nx_rtc_calendar_t retained;
    nx_result_t result = calendar(s, &retained);
    if (result != NX_SUCCESS)
        return result;
    s->clock = *clock;
    s->saved_alarm = NX_U575_READ(&s->registers->ALRMAR);
    s->saved_subsecond = NX_U575_READ(&s->registers->ALRMASSR);
    ++s->generation;
    s->stage = IDLE;
    s->terminal_set = s->closing = s->tainted = false;
    s->retained = s->opened = true;
    return NX_SUCCESS;
}
