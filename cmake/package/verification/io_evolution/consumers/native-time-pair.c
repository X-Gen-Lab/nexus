/**
 * \file            native-time-pair.c
 * \brief           Public generated Native calendar/counter qualification
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"
#include "nexus/io/native/rtc_lptim_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <inttypes.h>
#include <stdio.h>

/** \brief Every failure stays nonzero even when the source line exceeds 255. */
#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            (void)fprintf(stderr, "Native time qualification failed: %d\n",    \
                          __LINE__);                                           \
            return 1;                                                          \
        }                                                                      \
    } while (0)

/** \brief Query actual Linux loader mappings of the complete object span.
 * \note Qualification-only libc/proc access, never a production I/O dependency.
 *       This proves observed R/non-W mappings after relocation on this Linux
 *       process only. No static ELF-loader, MCU, total-RAM or other-host claim.
 */
static bool readonly_mapping(const void* pointer, size_t size) {
#if defined(__linux__)
    uintptr_t cursor = (uintptr_t)pointer;
    if (pointer == NULL || size == 0U || size > UINTPTR_MAX - cursor) {
        return false;
    }
    uintptr_t limit = cursor + size;
    FILE* maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        return false;
    }
    char line[256];
    bool complete = false;
    while (fgets(line, sizeof(line), maps) != NULL) {
        uintptr_t first = 0U;
        uintptr_t last = 0U;
        char permissions[5] = {0};
        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s", &first, &last,
                   permissions) != 3 ||
            cursor < first || cursor >= last) {
            continue;
        }
        if (permissions[0] != 'r' || permissions[1] == 'w') {
            break;
        }
        if (limit <= last) {
            complete = true;
            break;
        }
        cursor = last;
    }
    (void)fclose(maps);
    return complete;
#else
    (void)pointer;
    (void)size;
    return false;
#endif
}

/** \brief Consume only acquire-published results, never borrowed metadata. */
static bool settled(const nx_request_t* request, nx_result_t expected,
                    size_t expected_count) {
    nx_result_t result = NX_ERROR_IO;
    size_t count = SIZE_MAX;
    return nx_request_state(request) == NX_REQUEST_SETTLED &&
           nx_request_result(request, &result, &count) == NX_SUCCESS &&
           result == expected && count == expected_count;
}

/** \brief Run once per fresh process using only public and generated headers.
 */
/* The Google qualification target renames this public C entry point. */
int main(void);

int main(void) {
    _Static_assert(NX_RTC_ID_COUNT == 2, "Exact pair has two RTC identities");
    _Static_assert(NX_LPTIM_ID_COUNT == 2,
                   "Exact pair has two tick identities");
    const nx_rtc_port_t* a = nx_factory_rtc(NX_RTC_ID_CALENDAR_A);
    const nx_rtc_port_t* b = nx_factory_rtc(NX_RTC_ID_CALENDAR_B);
    const nx_lptim_port_t* x = nx_factory_lptim(NX_LPTIM_ID_COUNTER_A);
    const nx_lptim_port_t* y = nx_factory_lptim(NX_LPTIM_ID_COUNTER_B);
    CHECK(a != NULL && b != NULL && x != NULL && y != NULL);
    CHECK(a != b && a->context != b->context);
    CHECK(x != y && x->context != y->context);
    CHECK(a->context != x->context && a->context != y->context);
    CHECK(b->context != x->context && b->context != y->context);
    CHECK(a->ops == b->ops && x->ops == y->ops);
    CHECK(nx_factory_rtc(NX_RTC_ID_COUNT) == NULL);
    CHECK(nx_factory_lptim(NX_LPTIM_ID_COUNT) == NULL);
    CHECK(readonly_mapping(a, sizeof(*a)) && readonly_mapping(b, sizeof(*b)));
    CHECK(readonly_mapping(x, sizeof(*x)) && readonly_mapping(y, sizeof(*y)));
    CHECK(readonly_mapping(a->ops, sizeof(*a->ops)));
    CHECK(readonly_mapping(x->ops, sizeof(*x->ops)));
    CHECK(nx_native_clock_configure(true, 17) == NX_SUCCESS);
    nx_rtc_snapshot_t first = {0};
    nx_rtc_snapshot_t second = {0};
    nx_lptim_sample_t left = {0};
    nx_lptim_sample_t right = {0};
    CHECK(nx_rtc_port_read(a, &first) == NX_ERROR_STATE);
    CHECK(nx_lptim_port_sample(x, &left) == NX_ERROR_STATE);
    CHECK(nx_native_rtc_model_advance(a, 1) == NX_ERROR_STATE);
    CHECK(nx_time_now_us() == 17U);
    nx_platform_start_result_t start = nx_platform_start();
    CHECK(start.primary == NX_SUCCESS && start.cleanup == NX_SUCCESS &&
          start.remaining_effects == 0U);
    CHECK(nx_rtc_port_read(a, &first) == NX_ERROR_STATE);
    CHECK(nx_rtc_port_read(b, &second) == NX_ERROR_STATE);
    CHECK(nx_lptim_port_sample(x, &left) == NX_SUCCESS);
    CHECK(nx_lptim_port_sample(y, &right) == NX_SUCCESS);
    CHECK(left.tick_hz == 32768U && right.tick_hz == 1000U);
    CHECK(left.counter == 0U && right.counter == 0U && !left.running &&
          !right.running);
    CHECK(left.domain.identity != right.domain.identity);
    const nx_rtc_calendar_t date_a = {24, 2, 28, 7, 23, 59, 59};
    const nx_rtc_calendar_t date_b = {1, 1, 1, 1, 12, 0, 0};
    CHECK(nx_rtc_port_set(a, &date_a) == NX_SUCCESS);
    CHECK(nx_rtc_port_set(b, &date_b) == NX_SUCCESS);
    CHECK(nx_rtc_port_read(a, &first) == NX_SUCCESS);
    CHECK(nx_rtc_port_read(b, &second) == NX_SUCCESS);
    CHECK(first.domain.identity != second.domain.identity);
    nx_rtc_request_t alarm = {0};
    nx_rtc_request_t stale_alarm = {0};
    nx_lptim_request_t full = {0};
    nx_lptim_request_t held = {0};
    nx_lptim_request_t stale_shot = {0};
    nx_request_initialize(&alarm.base);
    nx_request_initialize(&stale_alarm.base);
    nx_request_initialize(&full.base);
    nx_request_initialize(&held.base);
    nx_request_initialize(&stale_shot.base);
    const nx_rtc_alarm_t midnight = {29, 0, 0, 0};
    CHECK(nx_rtc_request_prepare(&alarm, &first.domain, &midnight,
                                 NX_DEADLINE_NEVER) == NX_SUCCESS);
    CHECK(nx_rtc_port_submit(b, &alarm) == NX_ERROR_STATE);
    CHECK(nx_rtc_port_submit(a, &alarm) == NX_SUCCESS);
    CHECK(nx_lptim_request_prepare(&full, &left.domain, 65536,
                                   NX_DEADLINE_NEVER) == NX_SUCCESS);
    CHECK(nx_lptim_request_prepare(&held, &right.domain, 10,
                                   NX_DEADLINE_NEVER) == NX_SUCCESS);
    CHECK(nx_lptim_port_submit(y, &full) == NX_ERROR_STATE);
    CHECK(nx_lptim_port_submit(x, &full) == NX_SUCCESS);
    CHECK(nx_lptim_port_submit(y, &held) == NX_SUCCESS);
    CHECK(nx_native_rtc_model_advance(a, 1) == NX_SUCCESS);
    CHECK(nx_native_lptim_model_advance(x, 65536) == NX_SUCCESS);
    CHECK(nx_lptim_port_sample(x, &left) == NX_SUCCESS && left.counter == 0U &&
          left.running);
    CHECK(nx_rtc_port_service(a) == NX_SUCCESS);
    CHECK(nx_lptim_port_service(x) == NX_SUCCESS);
    CHECK(settled(&alarm.base, NX_SUCCESS, 1));
    CHECK(settled(&full.base, NX_SUCCESS, 65536));
    CHECK(nx_request_state(&held.base) == NX_REQUEST_ACTIVE);
    CHECK(nx_rtc_port_read(a, &first) == NX_SUCCESS &&
          first.calendar.day == 29U);
    CHECK(nx_rtc_port_read(b, &second) == NX_SUCCESS &&
          second.calendar.hour == 12U);
    const nx_rtc_alarm_t later = {29, 0, 0, 1};
    CHECK(nx_rtc_request_prepare(&stale_alarm, &first.domain, &later,
                                 NX_DEADLINE_NEVER) == NX_SUCCESS);
    CHECK(nx_lptim_port_sample(x, &left) == NX_SUCCESS);
    CHECK(nx_lptim_request_prepare(&stale_shot, &left.domain, 1,
                                   NX_DEADLINE_NEVER) == NX_SUCCESS);
    CHECK(nx_native_lptim_model_advance(y, 3) == NX_SUCCESS);
    CHECK(nx_native_lptim_model_hold_withdraw(y, true) == NX_SUCCESS);
    CHECK(nx_platform_stop() == NX_ERROR_BUSY);
    CHECK(nx_request_state(&held.base) == NX_REQUEST_QUARANTINED);
    CHECK(nx_platform_start().primary == NX_ERROR_STATE);
    CHECK(nx_native_lptim_model_advance(y, 4) == NX_SUCCESS);
    CHECK(nx_native_lptim_model_hold_withdraw(y, false) == NX_SUCCESS);
    CHECK(nx_lptim_port_service(y) == NX_SUCCESS);
    CHECK(settled(&held.base, NX_ERROR_CANCELLED, 7));
    CHECK(nx_platform_stop() == NX_SUCCESS);
    start = nx_platform_start();
    CHECK(start.primary == NX_SUCCESS && start.cleanup == NX_SUCCESS &&
          start.remaining_effects == 0U);
    CHECK(nx_factory_rtc(NX_RTC_ID_CALENDAR_A) == a);
    CHECK(nx_factory_lptim(NX_LPTIM_ID_COUNTER_A) == x);
    CHECK(nx_rtc_port_submit(a, &stale_alarm) == NX_ERROR_STATE);
    CHECK(nx_lptim_port_submit(x, &stale_shot) == NX_ERROR_STATE);
    CHECK(nx_rtc_port_read(a, &first) == NX_SUCCESS &&
          first.calendar.day == 29U);
    CHECK(nx_rtc_request_prepare(&alarm, &first.domain, &later,
                                 NX_DEADLINE_NEVER) == NX_SUCCESS);
    CHECK(nx_rtc_port_submit(a, &alarm) == NX_SUCCESS);
    CHECK(nx_native_rtc_model_advance(a, 1) == NX_SUCCESS);
    CHECK(nx_rtc_port_service(a) == NX_SUCCESS &&
          settled(&alarm.base, NX_SUCCESS, 1));
    CHECK(nx_lptim_port_sample(x, &left) == NX_SUCCESS);
    CHECK(nx_lptim_request_prepare(&full, &left.domain, 1, NX_DEADLINE_NEVER) ==
          NX_SUCCESS);
    CHECK(nx_lptim_port_submit(x, &full) == NX_SUCCESS);
    CHECK(nx_native_lptim_model_advance(x, 1) == NX_SUCCESS);
    CHECK(nx_lptim_port_service(x) == NX_SUCCESS &&
          settled(&full.base, NX_SUCCESS, 1));
    CHECK(nx_lptim_port_sample(x, &left) == NX_SUCCESS);
    CHECK(nx_lptim_request_prepare(&full, &left.domain, 8, 22) == NX_SUCCESS);
    CHECK(nx_lptim_port_submit(x, &full) == NX_SUCCESS);
    CHECK(nx_native_clock_advance(5) == NX_SUCCESS);
    CHECK(nx_lptim_port_service(x) == NX_SUCCESS);
    CHECK(settled(&full.base, NX_ERROR_TIMEOUT, 0));
    CHECK(nx_time_now_us() == 22U);
    CHECK(nx_factory_rtc_capability(NX_RTC_ID_CALENDAR_A)->unit_bytes == 0U);
    CHECK(nx_factory_lptim_capability(NX_LPTIM_ID_COUNTER_A)
              ->max_transfer_units == 65536U);
    CHECK(nx_platform_stop() == NX_SUCCESS);
    CHECK(nx_platform_stop() == NX_SUCCESS);
    CHECK(nx_native_clock_configure(false, 0) == NX_SUCCESS);
    return 0;
}
