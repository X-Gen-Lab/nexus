/**
 * \file            freertos_stop.c
 * \brief           Selected one-shot FreeRTOS Idle STOP reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "FreeRTOS.h"
#include "clock_domain_impl.h"
#include "drivers/rtc/rtc_idle.h"
#include "nexus/arch/arch.h"
#include "nexus/os/lowpower.h"
#include "nexus/soc/stm32f4_freertos_stop.h"
#include "system/stm32f4_idle_power.h"
#include "task.h"
#include "timebase_lowpower.h"
#include <stddef.h>

#define RTC_DAY_TICKS UINT32_C(22118400)
#define RTC_HZ        UINT32_C(256)
#define PPM_SCALE     UINT32_C(1000000)

_Static_assert(configUSE_TICKLESS_IDLE == 2,
               "STOP requires the sole explicit Idle suppression entry");
_Static_assert(configTICK_RATE_HZ > 0 && PPM_SCALE % configTICK_RATE_HZ == 0,
               "STOP requires an integral microsecond kernel period");

__attribute__((weak)) nx_stm32_freertos_stop_t*
nx_stm32_freertos_stop_context(void) {
    return NULL;
}

/** \brief Bad timing proof never unmasks or returns a borrowed tick. */
static _Noreturn void fail(nx_stm32_freertos_stop_t* state,
                           nx_result_t result) {
    state->result = result;
    nx_freertos_assert_failed(__FILE__, (unsigned)__LINE__);
    for (;;) {
    }
}

static bool span(const void* address, size_t bytes, size_t alignment) {
    return address != NULL && (uintptr_t)address % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)address;
}
static bool overlap(const void* a, size_t a_bytes, const void* b,
                    size_t b_bytes) {
    uintptr_t left = (uintptr_t)a, right = (uintptr_t)b;
    return left <= right ? right - left < a_bytes : left - right < b_bytes;
}
/** \brief Only the product's stable explicit owner can arm this profile. */
static bool valid(nx_stm32_freertos_stop_t* s,
                  const nx_stm32_freertos_stop_config_t* c) {
    return span(s, sizeof(*s), _Alignof(nx_stm32_freertos_stop_t)) &&
           span(c, sizeof(*c), _Alignof(nx_stm32_freertos_stop_config_t)) &&
           span(c->rtc, sizeof(*c->rtc), _Alignof(nx_rtc_port_t)) &&
           !overlap(s, sizeof(*s), c, sizeof(*c)) &&
           !overlap(s, sizeof(*s), c->rtc, sizeof(*c->rtc)) &&
           !overlap(c, sizeof(*c), c->rtc, sizeof(*c->rtc)) &&
           c->poll_limit != 0U && c->max_elapsed_us >= 140000U &&
           c->max_elapsed_us <= PPM_SCALE && c->drift_ppm <= 10000U &&
           nx_clock_domain_span_valid(c->barrier.domain, s, sizeof(*s)) &&
           nx_clock_domain_span_valid(c->barrier.domain, c, sizeof(*c)) &&
           nx_clock_domain_span_valid(c->barrier.domain, c->rtc,
                                      sizeof(*c->rtc));
}
static bool thread(void) {
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           masks.primask == 0U && masks.basepri == 0U && masks.faultmask == 0U;
}

nx_result_t
nx_stm32_freertos_stop_arm(nx_stm32_freertos_stop_t* s,
                           const nx_stm32_freertos_stop_config_t* c) {
    if (!thread())
        return NX_ERROR_CONTEXT;
    if (s == NULL || nx_stm32_freertos_stop_context() != s || !valid(s, c))
        return NX_ERROR_INVALID;
    if (s->armed)
        return NX_ERROR_BUSY;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = nx_clock_domain_quiescent(c->barrier);
    uint32_t sample = 0U;
    if (result == NX_SUCCESS)
        result = nx_stm32_rtc_idle_sample(c->rtc, &sample);
    if (result == NX_SUCCESS)
        *s = (nx_stm32_freertos_stop_t){.config = c, .armed = true};
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Snapshot quantization is one complete SSR tick at each end. */
static void envelope(nx_stm32_freertos_stop_t* s, uint32_t before,
                     uint32_t after, uint64_t pre_min, uint64_t pre_max,
                     uint64_t post_max) {
    if (before >= RTC_DAY_TICKS || after >= RTC_DAY_TICKS)
        fail(s, NX_ERROR_IO);
    uint32_t ticks =
        after >= before ? after - before : RTC_DAY_TICKS - before + after;
    uint64_t nominal_min =
        ticks == 0U ? 0U : (uint64_t)(ticks - 1U) * PPM_SCALE / RTC_HZ;
    uint64_t nominal_max =
        ((uint64_t)(ticks + 1U) * PPM_SCALE + RTC_HZ - 1U) / RTC_HZ;
    uint32_t ppm = s->config->drift_ppm;
    s->elapsed_min_us = nominal_min * PPM_SCALE / (PPM_SCALE + ppm) + pre_min;
    uint32_t denominator = PPM_SCALE - ppm;
    s->elapsed_max_us =
        (nominal_max * PPM_SCALE + denominator - 1U) / denominator + pre_max +
        post_max;
}

/** \brief Constant-time kernel step stays strictly before its next timeout. */
static void restore(nx_stm32_freertos_stop_t* s,
                    const nx_freertos_lowpower_port_t* port,
                    const nx_freertos_tick_snapshot_t* tick, uint32_t expected,
                    uint64_t measured_at, bool handed) {
    uint64_t now = port->now_us(port->context);
    if (now < measured_at)
        fail(s, NX_ERROR_IO);
    uint64_t extra = now - measured_at;
    if (s->elapsed_max_us > s->config->max_elapsed_us ||
        extra > s->config->max_elapsed_us - s->elapsed_max_us)
        fail(s, NX_ERROR_TIMEOUT);
    s->elapsed_min_us += extra;
    s->elapsed_max_us += extra;
    const uint32_t period = PPM_SCALE / configTICK_RATE_HZ;
    uint64_t combined = s->elapsed_min_us + tick->phase_us;
    uint64_t whole = combined / period;
    if (whole >= expected ||
        s->elapsed_max_us + tick->phase_us >= (uint64_t)expected * period)
        fail(s, NX_ERROR_TIMEOUT);
    s->accounted_ticks = whole;
    if (whole != 0U)
        vTaskStepTick((TickType_t)whole);
    uint64_t origin = port->now_us(port->context);
    if (origin < now ||
        origin - now > s->config->max_elapsed_us - s->elapsed_max_us)
        fail(s, NX_ERROR_TIMEOUT);
    /* Step latency and a deliberately discarded remainder make time later,
     * never earlier. Both remain explicit in the caller's uncertainty receipt.
     */
    s->elapsed_max_us += origin - now;
    s->discarded_phase_us = handed ? (uint32_t)(combined % period) : 0U;
    s->uncertainty_us =
        s->elapsed_max_us - s->elapsed_min_us + s->discarded_phase_us + 1U;
    if (origin > UINT64_MAX - period)
        fail(s, NX_ERROR_OVERFLOW);
    if (handed) {
        nx_timebase_stop_resume(port->context, origin + period);
    } else {
        uint64_t next = tick->time_us + (whole + 1U) * period - tick->phase_us;
        port->resume_tick(port->context, next);
    }
}

bool nx_freertos_try_stop_idle(uint32_t expected_idle_ticks) {
    nx_stm32_freertos_stop_t* s = nx_stm32_freertos_stop_context();
    if (!span(s, sizeof(*s), _Alignof(nx_stm32_freertos_stop_t)) || !s->armed)
        return false;
    const nx_stm32_freertos_stop_config_t* c = s->config;
    const uint32_t period = PPM_SCALE / configTICK_RATE_HZ;
    if (!valid(s, c) || !thread())
        return false;
    /* Leave the one-shot armed if the next unblock cannot admit its complete
     * externally declared wake/restore bound and one partial tick. */
    if ((uint64_t)expected_idle_ticks * period <=
        (uint64_t)c->max_elapsed_us + period)
        return false;
    const nx_freertos_lowpower_port_t* port = nx_freertos_lowpower_port();
    if (port == NULL || port->context == NULL || port->now_us == NULL ||
        port->pause_tick == NULL || port->resume_tick == NULL)
        return false;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    s->armed = false;
    s->attempted = true;
    s->result = NX_ERROR_BUSY;
    if (eTaskConfirmSleepModeStatus() == eAbortSleep)
        goto done;
    s->result = nx_clock_domain_quiescent(c->barrier);
    if (s->result != NX_SUCCESS)
        goto done;
    nx_freertos_tick_snapshot_t tick;
    s->result = port->pause_tick(port->context, &tick);
    if (s->result != NX_SUCCESS)
        goto done;
    if (tick.phase_us >= period)
        fail(s, NX_ERROR_IO);
    s->result = nx_timebase_stop_handoff(port->context);
    bool handed = s->result == NX_SUCCESS;
    uint64_t measured_at = port->now_us(port->context);
    if (measured_at < tick.time_us)
        fail(s, NX_ERROR_IO);
    if (!handed)
        goto exact_restore;
    uint32_t before = 0U, after = 0U;
    uint64_t pre_start = measured_at;
    s->result = nx_stm32_rtc_idle_sample(c->rtc, &before);
    uint64_t pre_end = port->now_us(port->context);
    if (pre_end < pre_start || pre_end - pre_start > 100U)
        fail(s, NX_ERROR_IO);
    if (s->result != NX_SUCCESS)
        goto exact_restore;
    const nx_stm32_power_options_t options = {.poll_limit = c->poll_limit,
                                              .wake_events =
                                                  NX_STM32_WAKE_RTC_TIMER,
                                              .low_power_regulator = true};
    s->result =
        nx_stm32_power_enter_idle_masked(c->barrier, &options, &s->power);
    if (s->result != NX_SUCCESS) {
        /* A changed epoch is never an exact old-clock rejection, even if the
         * new numeric counter happens to exceed the old paused instant. */
        if (s->power.old_epoch != 0U || s->power.new_epoch != 0U ||
            s->power.waited)
            fail(s, NX_ERROR_IO);
        goto exact_restore;
    }
    if (!s->power.waited || s->power.old_epoch == 0U ||
        s->power.new_epoch <= s->power.old_epoch)
        fail(s, NX_ERROR_IO);
    uint64_t post_start = port->now_us(port->context);
    nx_result_t sampled = nx_stm32_rtc_idle_sample(c->rtc, &after);
    uint64_t post_end = port->now_us(port->context);
    if (sampled != NX_SUCCESS || post_end < post_start ||
        post_end - post_start > 100U)
        fail(s, sampled == NX_SUCCESS ? NX_ERROR_IO : sampled);
    envelope(s, before, after, pre_start - tick.time_us, pre_end - tick.time_us,
             post_end - post_start);
    measured_at = post_end;
    if (s->elapsed_max_us > c->max_elapsed_us)
        fail(s, NX_ERROR_TIMEOUT);
    s->result = nx_stm32_rtc_idle_ack(c->rtc);
    restore(s, port, &tick, expected_idle_ticks, measured_at, handed);
    goto done;
exact_restore:
    measured_at = port->now_us(port->context);
    if (measured_at < tick.time_us)
        fail(s, NX_ERROR_IO);
    s->elapsed_min_us = measured_at - tick.time_us;
    s->elapsed_max_us = s->elapsed_min_us;
    restore(s, port, &tick, expected_idle_ticks, measured_at, handed);
done:
    nx_arch_irq_restore(saved);
    return true;
}
