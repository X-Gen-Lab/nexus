/**
 * \file            wait.c
 * \brief           Predicate wait with arm and recheck
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/wait.h"

/** \brief Call-local scan state with no subscriptions or retained borrowers. */
typedef struct {
    const nx_wait_source_t* sources;
    size_t count;
    size_t start;
    size_t selected;
} nx_wait_selection_t;

/** \brief Check byte-range aliasing without constructing end pointers. */
static bool overlaps(const void* left, size_t left_size, const void* right,
                     size_t right_size) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    return left_size != 0 && right_size != 0 &&
           (a <= b ? b - a < left_size : a - b < right_size);
}

/** \brief Reject spans whose last address cannot be represented safely. */
static bool span_overflows(const void* data, size_t bytes) {
    return (uintptr_t)bytes != bytes ||
           (uintptr_t)data > UINTPTR_MAX - (uintptr_t)bytes;
}

/** \brief Inspect at most one cyclic pass of authoritative source state. */
static bool any_ready(void* context) {
    nx_wait_selection_t* selection = context;
    size_t index = selection->start;
    for (size_t visited = 0; visited < selection->count; ++visited) {
        const nx_wait_source_t* source = &selection->sources[index];
        if (source->ready(source->context)) {
            selection->selected = index;
            return true;
        }
        if (++index == selection->count)
            index = 0;
    }
    return false;
}

/** \brief Wait without making notification the result authority. */
nx_result_t nx_wait_until(const nx_wait_port_t* port, bool (*ready)(void*),
                          void* context, uint64_t deadline_us) {
    if (port == NULL || port->arm == NULL || port->wait == NULL ||
        ready == NULL) {
        return NX_ERROR_INVALID;
    }
    for (;;) {
        if (ready(context)) {
            return NX_SUCCESS;
        }
        uint32_t sequence = port->arm(port->context);
        if (ready(context)) {
            return NX_SUCCESS;
        }
        nx_result_t result = port->wait(port->context, sequence, deadline_us);
        if (result != NX_SUCCESS) {
            /* Completion concurrent with a timeout still wins observation. */
            return ready(context) ? NX_SUCCESS : result;
        }
    }
}

/** \brief Validate all sources before reusing the established wait protocol. */
nx_result_t nx_wait_select(const nx_wait_port_t* port,
                           const nx_wait_source_t* sources, size_t count,
                           size_t start, uint64_t deadline_us,
                           size_t* selected) {
    if (!port || !sources || !selected || count == 0 || start >= count)
        return NX_ERROR_INVALID;
    if (count > SIZE_MAX / sizeof(*sources))
        return NX_ERROR_OVERFLOW;
    size_t bytes = count * sizeof(*sources);
    if (span_overflows(sources, bytes) || span_overflows(port, sizeof(*port)) ||
        span_overflows(selected, sizeof(*selected)))
        return NX_ERROR_OVERFLOW;
    nx_wait_selection_t selection = {sources, count, start, 0};
    if (overlaps(selected, sizeof(*selected), sources, bytes) ||
        overlaps(selected, sizeof(*selected), port, sizeof(*port)) ||
        overlaps(selected, sizeof(*selected), &selection, sizeof(selection)) ||
        overlaps(sources, bytes, port, sizeof(*port)))
        return NX_ERROR_INVALID;
    if (!port->arm || !port->wait)
        return NX_ERROR_INVALID;
    for (size_t index = 0; index < count; ++index)
        if (!sources[index].ready)
            return NX_ERROR_INVALID;
    nx_result_t result =
        nx_wait_until(port, any_ready, &selection, deadline_us);
    if (result == NX_SUCCESS)
        *selected = selection.selected;
    return result;
}
