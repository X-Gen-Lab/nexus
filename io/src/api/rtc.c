/**
 * \file            rtc.c
 * \brief           Checked raw-calendar preparation and typed dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/rtc_aux.h"
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
           !nx_time_overlap(argument, size, port->ops, sizeof(*port->ops)) &&
           (port->ops->auxiliary == NULL ||
            !nx_time_overlap(argument, size, port->ops->auxiliary,
                             sizeof(nx_rtc_aux_ops_t)));
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
/** \brief Optional controls reuse the stable RTC face and exact context. */
static const nx_rtc_aux_ops_t* auxiliary(const nx_rtc_port_t* port) {
    if (!face_valid(port) ||
        !nx_time_span(port->ops->auxiliary, sizeof(nx_rtc_aux_ops_t),
                      _Alignof(nx_rtc_aux_ops_t)))
        return NULL;
    return port->ops->auxiliary;
}
/** \brief Validate the copied pattern before selecting the optional method. */
nx_result_t nx_rtc_port_alarm_b(const nx_rtc_port_t* port, bool enabled,
                                const nx_rtc_alarm_t* alarm) {
    if (!face_valid(port) ||
        !argument_valid(port, alarm, sizeof(*alarm),
                        _Alignof(nx_rtc_alarm_t)) ||
        nx_rtc_alarm_validate(alarm) != NX_SUCCESS)
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->alarm_b == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->alarm_b(port->context, enabled, alarm);
}
/** \brief Public spans remain disjoint from face and both immutable tables. */
static bool aux_argument(const nx_rtc_port_t* port, const void* value,
                         size_t size, size_t alignment) {
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return face_valid(port) && argument_valid(port, value, size, alignment) &&
           (ops == NULL || !nx_time_overlap(value, size, ops, sizeof(*ops)));
}
/** \brief Dispatch only a complete wake counter description. */
nx_result_t nx_rtc_port_wakeup(const nx_rtc_port_t* port,
                               const nx_rtc_wakeup_t* config) {
    if (!aux_argument(port, config, sizeof(*config), _Alignof(nx_rtc_wakeup_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->wakeup == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->wakeup(port->context, config);
}
/** \brief Duration selection remains in the selected hardware provider. */
nx_result_t nx_rtc_port_periodic_wakeup(const nx_rtc_port_t* port,
                                        uint64_t period_us,
                                        uint32_t tolerance_us,
                                        nx_rtc_period_t* actual) {
    if (!aux_argument(port, actual, sizeof(*actual), _Alignof(nx_rtc_period_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->periodic_wakeup == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->periodic_wakeup(port->context, period_us, tolerance_us,
                                      actual);
}
/** \brief Dispatch capture configuration without retaining caller storage. */
nx_result_t
nx_rtc_port_capture_configure(const nx_rtc_port_t* port,
                              const nx_rtc_capture_config_t* config) {
    if (!aux_argument(port, config, sizeof(*config),
                      _Alignof(nx_rtc_capture_config_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->capture_configure == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->capture_configure(port->context, config);
}
/** \brief Validate output before any capture register access. */
nx_result_t nx_rtc_port_capture_read(const nx_rtc_port_t* port,
                                     nx_rtc_capture_t* capture) {
    if (!aux_argument(port, capture, sizeof(*capture),
                      _Alignof(nx_rtc_capture_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->capture_read == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->capture_read(port->context, capture);
}
/** \brief No acknowledgement is hidden in the observation dispatch. */
nx_result_t nx_rtc_port_events(const nx_rtc_port_t* port, uint32_t* events) {
    if (!aux_argument(port, events, sizeof(*events), _Alignof(uint32_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->events == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->events(port->context, events);
}
/** \brief The provider preserves unselected hardware flags. */
nx_result_t nx_rtc_port_acknowledge(const nx_rtc_port_t* port,
                                    uint32_t events) {
    if (!face_valid(port))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->acknowledge == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->acknowledge(port->context, events);
}
/** \brief No implicit calibration method or oscillator correction is chosen. */
nx_result_t nx_rtc_port_calibrate(const nx_rtc_port_t* port,
                                  const nx_rtc_calibration_t* calibration) {
    if (!aux_argument(port, calibration, sizeof(*calibration),
                      _Alignof(nx_rtc_calibration_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->calibrate == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->calibrate(port->context, calibration);
}
/** \brief Route ownership is an explicit provider action. */
nx_result_t nx_rtc_port_wake_route(const nx_rtc_port_t* port, uint32_t events) {
    if (!face_valid(port))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->wake_route == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->wake_route(port->context, events);
}
/** \brief Protect the output before observing one hardware backup word. */
nx_result_t nx_rtc_port_backup_read(const nx_rtc_port_t* port, uint8_t index,
                                    uint32_t* value) {
    if (!aux_argument(port, value, sizeof(*value), _Alignof(uint32_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->backup_read == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->backup_read(port->context, index, value);
}
/** \brief The literal write creates no storage layout or persistence policy. */
nx_result_t nx_rtc_port_backup_write(const nx_rtc_port_t* port, uint8_t index,
                                     uint32_t value) {
    if (!face_valid(port))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->backup_write == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->backup_write(port->context, index, value);
}
/** \brief SRAM power acquisition is separate from direct pointer admission. */
nx_result_t nx_rtc_port_sram_prepare(const nx_rtc_port_t* port, bool retain) {
    if (!face_valid(port))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->sram_prepare == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->sram_prepare(port->context, retain);
}
/** \brief The provider retains only an accepted original stable lease. */
nx_result_t nx_rtc_port_sram_acquire(const nx_rtc_port_t* port,
                                     nx_memory_window_lease_t* lease) {
    if (!aux_argument(port, lease, sizeof(*lease),
                      _Alignof(nx_memory_window_lease_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->sram_acquire == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->sram_acquire(port->context, lease);
}
/** \brief Foreign release identities are left for exact provider checks. */
nx_result_t nx_rtc_port_sram_release(const nx_rtc_port_t* port,
                                     nx_memory_window_lease_t* lease,
                                     uint64_t epoch) {
    if (!aux_argument(port, lease, sizeof(*lease),
                      _Alignof(nx_memory_window_lease_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->sram_release == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->sram_release(port->context, lease, epoch);
}
/** \brief Provisioned autonomous resources need explicit accepted transfer. */
nx_result_t nx_rtc_port_backup_handoff(const nx_rtc_port_t* port,
                                       nx_rtc_backup_handoff_t* handoff) {
    if (!aux_argument(port, handoff, sizeof(*handoff),
                      _Alignof(nx_rtc_backup_handoff_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->handoff == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->handoff(port->context, handoff);
}

nx_result_t nx_rtc_port_standby_prepare(const nx_rtc_port_t* port,
                                        uint32_t discard, uint32_t limit,
                                        nx_rtc_standby_t* receipt) {
    if (!aux_argument(port, receipt, sizeof(*receipt),
                      _Alignof(nx_rtc_standby_t)) ||
        (discard & ~UINT32_C(127)) != 0U || limit == 0U)
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->standby_prepare == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->standby_prepare(port->context, discard, limit, receipt);
}
nx_result_t nx_rtc_port_standby_validate(const nx_rtc_port_t* port,
                                         const nx_rtc_standby_t* receipt) {
    if (!aux_argument(port, receipt, sizeof(*receipt),
                      _Alignof(nx_rtc_standby_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->standby_validate == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->standby_validate(port->context, receipt);
}
nx_result_t nx_rtc_port_standby_commit(const nx_rtc_port_t* port,
                                       nx_rtc_standby_t* receipt) {
    if (!aux_argument(port, receipt, sizeof(*receipt),
                      _Alignof(nx_rtc_standby_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->standby_commit == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->standby_commit(port->context, receipt);
}
nx_result_t nx_rtc_port_standby_restore(const nx_rtc_port_t* port,
                                        nx_rtc_standby_t* receipt) {
    if (!aux_argument(port, receipt, sizeof(*receipt),
                      _Alignof(nx_rtc_standby_t)))
        return NX_ERROR_INVALID;
    const nx_rtc_aux_ops_t* ops = auxiliary(port);
    return ops == NULL || ops->standby_restore == NULL
               ? NX_ERROR_UNSUPPORTED
               : ops->standby_restore(port->context, receipt);
}
