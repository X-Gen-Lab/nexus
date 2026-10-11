/**
 * \file            diagnostic.c
 * \brief           Bounded copied-record diagnostics without hidden storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/diagnostic.h"
#include "nexus/arch/arch.h"
#include <stdbool.h>

/** \brief Admit only the reviewed same-CPU publication domain. */
static bool diagnostic_context(void) {
    if (!nx_arch_is_privileged()) {
        return false;
    }
    uint32_t exception = nx_arch_exception_number();
    return exception == 0 || exception >= 4;
}

/** \brief Check startup state before touching interrupt masks. */
static bool diagnostic_valid(const nx_diagnostic_ring_t* ring) {
    return ring != NULL && ring->events != NULL && ring->capacity != 0;
}

nx_result_t nx_diagnostic_ring_init(nx_diagnostic_ring_t* ring,
                                    nx_diagnostic_event_t* events,
                                    size_t capacity) {
    if (ring == NULL || events == NULL || capacity == 0 ||
        capacity > UINT32_MAX ||
        (uintptr_t)events % _Alignof(nx_diagnostic_event_t) != 0) {
        return NX_ERROR_INVALID;
    }
    nx_diagnostic_ring_t initialized = {events, (uint32_t)capacity, 0, 0, 0, 0};
    *ring = initialized;
    return NX_SUCCESS;
}

nx_result_t nx_diagnostic_ring_write(nx_diagnostic_ring_t* ring,
                                     const nx_diagnostic_event_t* event) {
    if (!diagnostic_valid(ring) || event == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!diagnostic_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (ring->count == ring->capacity) {
        if (ring->dropped != UINT32_MAX) {
            ++ring->dropped;
        }
        result = NX_ERROR_BUSY;
    } else {
        ring->events[ring->write_index] = *event;
        if (++ring->write_index == ring->capacity) {
            ring->write_index = 0;
        }
        ++ring->count;
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_diagnostic_ring_read(nx_diagnostic_ring_t* ring,
                                    nx_diagnostic_event_t* event) {
    if (!diagnostic_valid(ring) || event == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!diagnostic_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_ERROR_BUSY;
    if (ring->count != 0) {
        *event = ring->events[ring->read_index];
        if (++ring->read_index == ring->capacity) {
            ring->read_index = 0;
        }
        --ring->count;
        result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}

nx_result_t nx_diagnostic_ring_dropped(const nx_diagnostic_ring_t* ring,
                                       uint32_t* dropped) {
    if (!diagnostic_valid(ring) || dropped == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!diagnostic_context()) {
        return NX_ERROR_CONTEXT;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    *dropped = ring->dropped;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
