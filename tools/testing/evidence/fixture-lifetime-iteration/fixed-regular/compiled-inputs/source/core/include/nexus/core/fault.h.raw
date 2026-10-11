/**
 * \file            fault.h
 * \brief           Bounded observation before retaining platform fail-stop
 * \author          Nexus Team
 */
#ifndef NEXUS_CORE_FAULT_H
#define NEXUS_CORE_FAULT_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Fault origin, without any product safety or recovery decision. */
typedef enum {
    NX_FAULT_FREERTOS_ASSERT = 0,
    NX_FAULT_FREERTOS_STACK,
    NX_FAULT_THREADX_ADAPTER,
    NX_FAULT_PLATFORM
} nx_fault_domain_t;
/**
 * \brief           Observe a terminal fault before the caller retains effects.
 * \param[in]       domain: Actual reporting path, not a severity assessment.
 * \param[in]       reason: Observed failure, never permission to reclaim loans.
 * \param[in]       file: Static source literal or NULL; do not retain or read
 *                  untrusted dynamic memory. No task name is dereferenced.
 * \param[in]       line: Diagnostic location, zero when unavailable.
 * \note            The weak default returns and has no state or side effects.
 *                  A product may supply one strong implementation, bounded and
 *                  allocation-free in every calling task/ISR/masked context.
 *                  No functioning kernel, clock, stack, transport or peripheral
 *                  may be assumed after corruption. This is best-effort
 *                  observation, not stack repair, safe outputs or reset proof.
 *                  The hook must not acquire OS locks, reenter the provider,
 *                  change incoming masks, mutate accepted requests or release
 *                  retained storage. If it
 *                  returns, the terminal caller still parks. A product may
 *                  perform a separately reviewed non-returning recovery action.
 *                  No retained event, registry or worker is implicit.
 */
void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                      const char* file, unsigned line);
#ifdef __cplusplus
}
#endif
#endif
