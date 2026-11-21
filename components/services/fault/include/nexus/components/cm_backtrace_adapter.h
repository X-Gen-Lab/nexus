/**
 * \file            cm_backtrace_adapter.h
 * \brief           CmBacktrace with explicit retained stack bounds
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_CM_BACKTRACE_ADAPTER_H
#define NEXUS_COMPONENTS_CM_BACKTRACE_ADAPTER_H
#include "nexus/components/fault_m4.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Initialize upstream linker bounds before faults are
 * possible.
 *
 * \param[in]       firmware: Valid immutable firmware name, at most
 * 32 characters.
 *
 * \param[in]       hardware: Valid immutable hardware version,
 * at most 32 characters.
 *
 * \param[in]       software: Valid immutable software
 * version, at most 32 characters.
 *
 * \return          SUCCESS, INVALID, CONTEXT or
 * STATE for repeated initialization.
 *
 * \note            Explicit F407/Cortex-M4
 * target only, single privileged startup owner. Uses current generated MSP and
 * live-Flash linker bounds. No vendor handler, OS platform, printer, kernel
 * query or task registry is installed. Default physical exception ownership
 *                  remains with the selected startup composition.
 */
nx_result_t nx_cm_backtrace_init(const char* firmware, const char* hardware,
                                 const char* software);

/**
 * \brief           Run the actual upstream stack scan over one proven live
 * span.
 *
 * \param[in]       record: Previously committed validated M4F crash
 * record.
 *
 * \param[in]       binding: Original still-readable interrupted MSP/PSP
 * stack.
 *
 * \param[out]      addresses: Exclusive output, disjoint from all input
 * storage.
 *
 * \param[in]       capacity: Positive address count, at most 32.
 *
 * \param[in]       maximum_scan_words: Explicit scan budget, one through 1024.
 *
 * \param[out]      depth: Separate output count, unchanged on rejection.
 *
 * \return          SUCCESS, INVALID, STATE or raw-record validation error.
 *
 * \note            Best-effort same-boot diagnosis after raw capture. Skips the
 *                  admitted basic/extended M4F frame and alignment word, then
 *                  calls unmodified cm_backtrace_call_stack_any. Full declared
 *                  scan range may be read even when address capacity is
 * reached. PSP uses explicit original binding, never FreeRTOS TCB offsets. A
 * reset does not preserve the stack allocation contents; use the raw register
 * record/host decoder after reboot instead. No ordinary Log, RTT, kernel, Flash
 * or clock is used. This selected code range excludes RAM-executed functions.
 */
nx_result_t nx_cm_backtrace_record(const volatile nx_fault_record_t* record,
                                   const nx_fault_stack_binding_t* binding,
                                   uint32_t* addresses, size_t capacity,
                                   size_t maximum_scan_words, size_t* depth);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_CM_BACKTRACE_ADAPTER_H */
