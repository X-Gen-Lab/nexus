/**
 * \file            freertos_stack.h
 * \brief           Optional bounded task stack paint snapshot
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_FREERTOS_STACK_H
#define NEXUS_OS_FREERTOS_STACK_H

#include "FreeRTOS.h"
#include "nexus/core/stack_observation.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Exact static task facts supplied at original creation.
 * \note            The caller retains all storage, excludes delete/join/discard
 *                  and never replaces the live binding. This is not a registry.
 */
typedef struct {
    TaskHandle_t handle;
    StaticTask_t* control;
    StackType_t* stack;
    size_t words;
} nx_freertos_stack_binding_t;

/**
 * \brief           Copy bounded stack paint facts using actual kernel identity.
 * \param[in]       binding: Exact original TCB, stack and capacity, all live.
 * \param[out]      out: Distinct output, unchanged on rejection.
 * \return          SUCCESS, INVALID, CONTEXT or STATE for a binding mismatch.
 * \note            Privileged unmasked running Thread; task stack quiescent
 *                  throughout the scan. Available only with NEXUS_OS_TRACE.
 *                  xTaskGetStaticBuffers verifies TCB/stack identity, not size.
 *                  The original capacity is a caller contract. No task suspend,
 *                  interrupt mask, allocation or live painting is performed.
 *                  Worst-case work is words*sizeof(StackType_t) byte reads.
 *                  Paint is the locked kernel's 0xA5. Untouched/touched bytes
 * do not prove physical stack peaks. The POSIX kernel port uses a separate
 * pthread execution stack; its query is not an MCU or Linux execution-stack
 * peak measurement.
 */
nx_result_t
nx_freertos_stack_observe(const nx_freertos_stack_binding_t* binding,
                          nx_stack_observation_t* out);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_FREERTOS_STACK_H */
