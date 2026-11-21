/**
 * \file            event_fanout.c
 * \brief           Explicit caller-owned event fanout implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/event_fanout.h"

nx_result_t nx_event_fanout_initialize(nx_event_fanout_t* fanout,
                                       const nx_event_sink_t* sinks,
                                       size_t count) {
    if (fanout == NULL || sinks == NULL || count == 0 ||
        count > SIZE_MAX / sizeof(*sinks)) {
        return NX_ERROR_INVALID;
    }
    for (size_t index = 0; index < count; ++index) {
        if (sinks[index].write == NULL) {
            return NX_ERROR_INVALID;
        }
    }
    const nx_event_fanout_t next = {sinks, count, false, true};
    *fanout = next;
    return NX_SUCCESS;
}

nx_result_t nx_event_fanout_emit(nx_event_fanout_t* fanout, const void* data,
                                 size_t length,
                                 nx_event_fanout_report_t* report) {
    if (fanout == NULL || data == NULL || length == 0 || report == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!fanout->initialized || fanout->sinks == NULL || fanout->count == 0) {
        return NX_ERROR_STATE;
    }
    if (fanout->emitting) {
        return NX_ERROR_BUSY;
    }
    nx_event_fanout_report_t next = {0, 0, NX_SUCCESS};
    fanout->emitting = true;
    for (size_t index = 0; index < fanout->count; ++index) {
        const nx_event_sink_t* sink = &fanout->sinks[index];
        const nx_result_t result = sink->write(sink->context, data, length);
        ++next.attempted;
        if (result == NX_SUCCESS) {
            ++next.delivered;
        } else if (next.first_error == NX_SUCCESS) {
            next.first_error = result;
        }
    }
    fanout->emitting = false;
    *report = next;
    return NX_SUCCESS;
}
