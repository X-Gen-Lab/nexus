/**
 * \file            lifecycle_adapter.c
 *
 * \brief           External C cold lifecycle with a bounded fault port.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-11-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "lifecycle_adapter.h"

enum { NX_REFERENCE_EVENT_CAPACITY = 16 };
static nx_reference_event_t s_events[NX_REFERENCE_EVENT_CAPACITY];
static size_t s_event_count;
static uint32_t s_failed_mask;

/** \brief Cold single-owner observations never allocate or expose storage. */
static void record_event(uint32_t kind, uint32_t mask) {
    if (s_event_count < NX_REFERENCE_EVENT_CAPACITY) {
        s_events[s_event_count++] = (nx_reference_event_t){kind, mask};
    }
}

void nx_reference_reset(void) {
    s_event_count = 0U;
    s_failed_mask = 0U;
}

void nx_reference_fail_prepare(uint32_t mask) {
    s_failed_mask = mask;
}

size_t nx_reference_event_count(void) {
    return s_event_count;
}

nx_reference_event_t nx_reference_event(size_t index) {
    return index < s_event_count ? s_events[index]
                                 : (nx_reference_event_t){0U, 0U};
}

/** \brief Reject the injected failure before any Native port state is touched.
 */
nx_result_t nx_reference_gpio_prepare(nx_native_gpio_state_t* port,
                                      uint32_t mask, uint32_t initial,
                                      bool output) {
    record_event(NX_REFERENCE_PREPARE, mask);
    if (mask == s_failed_mask) {
        return NX_ERROR_IO;
    }
    return nx_native_gpio_configure_instance(port, mask, initial, output);
}

/** \brief Record the actual live view before C stop withdraws its authority. */
nx_result_t nx_reference_gpio_stop(nx_native_gpio_state_t* port,
                                   uint32_t inactive) {
    record_event(NX_REFERENCE_STOP, port == NULL ? 0U : port->mask);
    return nx_native_gpio_stop_instance(port, inactive);
}
