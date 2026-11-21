/**
 * \file            health_freertos.c
 * \brief           Existing FreeRTOS static-task observation consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/health_freertos.h"
#include "nexus/arch/arch.h"

/** \brief Preserve original task/ring storage rather than merely local output.
 */
static bool overlaps(const void* first, size_t bytes, const void* second,
                     size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Avoid a 64-bit type-limits shortcut while checking 32-bit builds too.
 */
static bool ring_span_fits(size_t count) {
    return count <= SIZE_MAX / sizeof(nx_diagnostic_event_t);
}

/** \brief The existing static-task observer remains the sole kernel boundary.
 */
nx_result_t
nx_health_freertos_resources(const nx_freertos_stack_binding_t* binding,
                             const nx_diagnostic_ring_t* ring,
                             nx_health_resources_t* output) {
    if (output == NULL ||
        (binding != NULL &&
         (binding->words > SIZE_MAX / sizeof(*binding->stack) ||
          overlaps(binding, sizeof(*binding), output, sizeof(*output)) ||
          overlaps(binding->control, sizeof(*binding->control), output,
                   sizeof(*output)) ||
          overlaps(binding->stack, binding->words * sizeof(*binding->stack),
                   output, sizeof(*output)))) ||
        (ring != NULL &&
         (overlaps(ring, sizeof(*ring), output, sizeof(*output)) ||
          !ring_span_fits(ring->capacity) ||
          overlaps(ring->events, (size_t)ring->capacity * sizeof(*ring->events),
                   output, sizeof(*output))))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    nx_health_resources_t result = {0};
    if (binding != NULL) {
        nx_result_t observed =
            nx_freertos_stack_observe(binding, &result.stack);
        if (observed != NX_SUCCESS) {
            return observed;
        }
        result.known |= NX_HEALTH_STACK_KNOWN;
    }
    if (ring != NULL) {
        nx_result_t observed =
            nx_diagnostic_ring_dropped(ring, &result.ring_dropped);
        if (observed != NX_SUCCESS) {
            return observed;
        }
        result.known |= NX_HEALTH_RING_KNOWN;
    }
    *output = result;
    return NX_SUCCESS;
}
