/**
 * \file            u575-rtc.c
 * \brief           Public-only retained RTC RUN software link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
static nx_rtc_request_t s_request;
/** \brief Retain static loans and platform responsibility on failed drain. */
static void retain(void) {
    for (;;) {
    }
}
/** \brief Actual join is mandatory before this fixture can exit. */
static int close_platform(int result) {
    if (nx_platform_stop() != NX_SUCCESS)
        retain();
    return result;
}
/** \brief External boot policy prepares the retained RTC before entry. */
int main(void) {
    const nx_rtc_port_t* port = nx_factory_rtc(NX_RTC_ID_CALENDAR);
    if (port == NULL)
        return 1;
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS)
        return close_platform(2);
    nx_rtc_snapshot_t snapshot;
    nx_request_initialize(&s_request.base);
    if (nx_rtc_port_read(port, &snapshot) != NX_SUCCESS)
        return close_platform(3);
    /* This is a distinct future day/time pattern, not a translation from
     * monotonic microseconds into a calendar epoch. Product date policy is
     * external; the finite Core deadline independently bounds this loan. */
    nx_rtc_alarm_t alarm = {snapshot.calendar.day, snapshot.calendar.hour,
                            snapshot.calendar.minute,
                            (uint8_t)((snapshot.calendar.second + 1U) % 60U)};
    if (nx_rtc_request_prepare(&s_request, &snapshot.domain, &alarm,
                               nx_deadline_after(nx_time_now_us(), 1000U)) !=
            NX_SUCCESS ||
        nx_rtc_port_submit(port, &s_request) != NX_SUCCESS)
        return close_platform(4);
    if (nx_rtc_port_cancel(port, &s_request) != NX_SUCCESS)
        retain();
    nx_time_us_t join = nx_deadline_after(nx_time_now_us(), 1000U);
    while (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED &&
           !nx_deadline_expired(join, nx_time_now_us()))
        (void)nx_rtc_port_service(port);
    if (nx_request_state(&s_request.base) != NX_REQUEST_SETTLED) {
        (void)nx_platform_stop();
        retain();
    }
    return close_platform(0);
}
