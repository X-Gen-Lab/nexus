/**
 * \file            freertos_diagnostics.h
 * \brief           Bounded explicit FreeRTOS trace sink without global storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_OS_FREERTOS_DIAGNOSTICS_H
#define NEXUS_OS_FREERTOS_DIAGNOSTICS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Maintained kernel trace identities; product interpretation is
 * external. */
typedef enum {
    NX_FREERTOS_TRACE_SWITCH_IN = 1,
    NX_FREERTOS_TRACE_CREATE,
    NX_FREERTOS_TRACE_DELETE
} nx_freertos_trace_kind_t;
/**
 * \brief           Consume one bounded kernel trace event.
 * \param[in]       event: Maintained trace kind.
 * \param[in]       identity: Opaque task identity, never a dereference license.
 * \param[in]       value: Optional caller-defined numeric diagnostic.
 * \note            Application may override the weak no-op and write an
 *                  explicit nx_diagnostic_ring_t. May run under kernel masks
 *                  or configurable IRQ, so never block, allocate, format,
 *                  transmit, acquire OS locks or call kernel services. The
 *                  sink owns timestamp and lifetime; no buffer is provided
 *                  automatically. Trace profile enables real kernel hooks.
 */
void nx_freertos_trace_event(nx_freertos_trace_kind_t event, uintptr_t identity,
                             uint32_t value);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_FREERTOS_DIAGNOSTICS_H */
