/**
 * \file            health_resources.c
 * \brief           Resource observations reuse existing bounded mechanisms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/health_resources.h"
#include "nexus/arch/arch.h"

/** \brief Reject aliases before a copied observation can overwrite its source.
 */
static bool overlaps(const void* first, size_t bytes, const void* second,
                     size_t other_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return bytes > UINTPTR_MAX - a || other_bytes > UINTPTR_MAX - b ||
           (a < b + other_bytes && b < a + bytes);
}

/** \brief Keep count arithmetic valid on both Native and 32-bit firmware ABIs.
 */
static bool ring_span_fits(size_t count) {
    return count <= SIZE_MAX / sizeof(nx_diagnostic_event_t);
}

/** \brief Preserve both independent observations and unknown resource groups.
 */
nx_result_t nx_health_stack_resources(const void* stack, size_t bytes,
                                      uint8_t paint,
                                      const nx_diagnostic_ring_t* ring,
                                      nx_health_resources_t* output) {
    if (output == NULL || (stack == NULL && bytes != 0U) ||
        (stack != NULL && overlaps(stack, bytes, output, sizeof(*output))) ||
        (ring != NULL &&
         (overlaps(ring, sizeof(*ring), output, sizeof(*output)) ||
          !ring_span_fits(ring->capacity) ||
          overlaps(ring->events, (size_t)ring->capacity * sizeof(*ring->events),
                   output, sizeof(*output))))) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr()) {
        return NX_ERROR_CONTEXT;
    }
    nx_health_resources_t result = {0};
    if (stack != NULL) {
        nx_result_t observed =
            nx_stack_observe(stack, bytes, paint, &result.stack);
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
