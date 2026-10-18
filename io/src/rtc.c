/**
 * \file            rtc.c
 * \brief           Checked raw-calendar preparation and typed dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/rtc.h"
#include "rtc_lptim_internal.h"
/** \brief Establish the readable face/table before any callback access. */
static bool face_valid(const nx_rtc_port_t* port) {
    return nx_time_span(port, sizeof(*port), _Alignof(nx_rtc_port_t)) &&
           port->context != NULL &&
           nx_time_span(port->ops, sizeof(*port->ops), _Alignof(nx_rtc_ops_t));
}
/** \brief Protect immutable identity before provider-specific span checks. */
static bool argument_valid(const nx_rtc_port_t* port, const void* argument,
                           size_t size, size_t alignment) {
    return nx_time_span(argument, size, alignment) &&
           !nx_time_overlap(argument, size, port, sizeof(*port)) &&
           !nx_time_overlap(argument, size, port->ops, sizeof(*port->ops));
}
/** \brief Check all ranges before using month as an array index. */
nx_result_t nx_rtc_calendar_validate(const nx_rtc_calendar_t* calendar) {
    if (!nx_time_span(calendar, sizeof(*calendar),
                      _Alignof(nx_rtc_calendar_t)) ||
        calendar->year > 99U || calendar->month < 1U || calendar->month > 12U ||
        calendar->day < 1U || calendar->weekday < 1U ||
        calendar->weekday > 7U || calendar->hour > 23U ||
        calendar->minute > 59U || calendar->second > 59U ||
        calendar->day > nx_time_month_days(calendar->year, calendar->month)) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
/** \brief Match only the raw hardware-like day/time field ranges. */
nx_result_t nx_rtc_alarm_validate(const nx_rtc_alarm_t* alarm) {
    if (!nx_time_span(alarm, sizeof(*alarm), _Alignof(nx_rtc_alarm_t)) ||
        alarm->day < 1U || alarm->day > 31U || alarm->hour > 23U ||
        alarm->minute > 59U || alarm->second > 59U) {
        return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
/** \brief Copy all validated inputs locally before modifying request base. */
nx_result_t nx_rtc_request_prepare(nx_rtc_request_t* request,
                                   const nx_rtc_domain_t* domain,
                                   const nx_rtc_alarm_t* alarm,
                                   nx_time_us_t deadline) {
    if (!nx_time_span(request, sizeof(*request), _Alignof(nx_rtc_request_t)) ||
        !nx_time_span(domain, sizeof(*domain), _Alignof(nx_rtc_domain_t)) ||
        !nx_time_span(alarm, sizeof(*alarm), _Alignof(nx_rtc_alarm_t)) ||
        nx_time_overlap(request, sizeof(*request), domain, sizeof(*domain)) ||
        nx_time_overlap(request, sizeof(*request), alarm, sizeof(*alarm))) {
        return NX_ERROR_INVALID;
    }
    nx_rtc_domain_t copied_domain = *domain;
    nx_rtc_alarm_t copied_alarm = *alarm;
    if (copied_domain.identity == NULL || copied_domain.generation == 0U ||
        nx_rtc_alarm_validate(&copied_alarm) != NX_SUCCESS) {
        return NX_ERROR_INVALID;
    }
    nx_result_t status = nx_request_prepare(&request->base, deadline);
    if (status != NX_SUCCESS) {
        return status;
    }
    request->domain = copied_domain;
    request->alarm = copied_alarm;
    return NX_SUCCESS;
}
/** \brief Dispatch the selected context after complete output validation. */
nx_result_t nx_rtc_port_read(const nx_rtc_port_t* port,
                             nx_rtc_snapshot_t* snapshot) {
    if (!face_valid(port) || !argument_valid(port, snapshot, sizeof(*snapshot),
                                             _Alignof(nx_rtc_snapshot_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->read == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->read(port->context, snapshot);
}
/** \brief Dispatch without retaining the input calendar descriptor. */
nx_result_t nx_rtc_port_set(const nx_rtc_port_t* port,
                            const nx_rtc_calendar_t* calendar) {
    if (!face_valid(port) || !argument_valid(port, calendar, sizeof(*calendar),
                                             _Alignof(nx_rtc_calendar_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->set == NULL ? NX_ERROR_UNSUPPORTED
                                  : port->ops->set(port->context, calendar);
}
/** \brief The provider performs owner/domain checks before admission. */
nx_result_t nx_rtc_port_submit(const nx_rtc_port_t* port,
                               nx_rtc_request_t* request) {
    if (!face_valid(port) || !argument_valid(port, request, sizeof(*request),
                                             _Alignof(nx_rtc_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->submit == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->submit(port->context, request);
}
/** \brief Validate identity numerically; never dereference a foreign request.
 */
nx_result_t nx_rtc_port_cancel(const nx_rtc_port_t* port,
                               nx_rtc_request_t* request) {
    if (!face_valid(port) || !argument_valid(port, request, sizeof(*request),
                                             _Alignof(nx_rtc_request_t))) {
        return NX_ERROR_INVALID;
    }
    return port->ops->cancel == NULL
               ? NX_ERROR_UNSUPPORTED
               : port->ops->cancel(port->context, request);
}
/** \brief No worker or ownership interpretation is added by dispatch. */
nx_result_t nx_rtc_port_service(const nx_rtc_port_t* port) {
    if (!face_valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->service == NULL ? NX_ERROR_UNSUPPORTED
                                      : port->ops->service(port->context);
}
/** \brief Only the exact provider may prove its sources have withdrawn. */
nx_result_t nx_rtc_port_stop(const nx_rtc_port_t* port) {
    if (!face_valid(port)) {
        return NX_ERROR_INVALID;
    }
    return port->ops->stop == NULL ? NX_ERROR_UNSUPPORTED
                                   : port->ops->stop(port->context);
}
