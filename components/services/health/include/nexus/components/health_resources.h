/**
 * \file            health_resources.h
 * \brief           Quiescent stack and ordinary ring resource facts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_HEALTH_RESOURCES_H
#define NEXUS_COMPONENTS_HEALTH_RESOURCES_H
#include "nexus/core/stack_observation.h"
#include "nexus/os/diagnostic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_HEALTH_STACK_KNOWN (1U << 0)
#define NX_HEALTH_RING_KNOWN  (1U << 1)

/** \brief Stack paint is not peak usage; ring loss is not a completion count.
 */
typedef struct {
    uint32_t known;
    uint32_t ring_dropped;
    nx_stack_observation_t stack;
} nx_health_resources_t;

/**
 * \brief           Sample one original quiescent painted span and optional
 * ring.
 *
 * \param[in]       stack: Original paint-provenance span, or NULL for
 * unknown.
 *
 * \param[in]       bytes: Original positive capacity; zero when stack
 * is NULL.
 *
 * \param[in]       paint: Exact byte used before any frame became
 * live.
 *
 * \param[in]       ring: Existing live ordinary ring, or NULL for
 * unknown.
 *
 * \param[out]      output: Separate result; unchanged on any
 * rejection.
 *
 * \return          SUCCESS or actual stack/ring INVALID/CONTEXT
 * error.
 *
 * \note            Same quiescence/provenance contract as
 * nx_stack_observe; excludes current SP, live frames and exception reservation.
 *                  At most bytes reads and one ring count query, no suspension,
 *                  live painting, kernel enumeration, transport or allocation.
 *                  The caller serializes output and input lifetime. Task-only;
 *                  optional ring retains its privileged single-CPU domain.
 */
nx_result_t nx_health_stack_resources(const void* stack, size_t bytes,
                                      uint8_t paint,
                                      const nx_diagnostic_ring_t* ring,
                                      nx_health_resources_t* output);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_HEALTH_RESOURCES_H */
