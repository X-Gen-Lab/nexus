/**
 * \file            health_freertos.h
 * \brief           Resources from original static FreeRTOS tasks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_HEALTH_FREERTOS_H
#define NEXUS_COMPONENTS_HEALTH_FREERTOS_H
#include "nexus/components/health_resources.h"
#include "nexus/os/freertos_stack.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Sample one exact original static-task allocation, not a TCB
 * walk.
 *
 * \param[in]       binding: Existing quiescent task binding, or NULL for
 * unknown.
 *
 * \param[in]       ring: Existing ordinary ring, or NULL for unknown.
 *
 * \param[out]      output: Independent result, unchanged on failure.
 *
 * \return          SUCCESS or the existing observation/ring error.
 *
 * \note            Available only in an explicitly selected FreeRTOS trace
 *                  profile. All nx_freertos_stack_observe running, privileged,
 *                  unmasked, quiescent-stack and no-delete/join preconditions
 *                  apply. This adapter creates no task, performs no suspension,
 *                  does not query private offsets and is never used on fault.
 */
nx_result_t
nx_health_freertos_resources(const nx_freertos_stack_binding_t* binding,
                             const nx_diagnostic_ring_t* ring,
                             nx_health_resources_t* output);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_HEALTH_FREERTOS_H */
