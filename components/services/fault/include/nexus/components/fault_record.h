/**
 * \file            fault_record.h
 * \brief           Fixed raw crash capture without a kernel
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_FAULT_RECORD_H
#define NEXUS_COMPONENTS_FAULT_RECORD_H
#include "nexus/core/fault.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_FAULT_RECORD_BYTES   160U
#define NX_FAULT_BUILD_ID_BYTES 16U
#define NX_FAULT_CORE_VALID     (1U << 0)
#define NX_FAULT_STATUS_VALID   (1U << 1)
#define NX_FAULT_STACK_VALID    (1U << 2)
#define NX_FAULT_FRAME_EXTENDED (1U << 3)

/** \brief Explicit numeric facts; no pointer requires dereferencing on fault.
 */
typedef struct {
    uint32_t sequence;
    uint32_t domain;
    uint32_t reason;
    uint32_t flags;
    uint32_t exception;
    uint32_t exception_return;
    uint32_t msp;
    uint32_t psp;
    uint8_t build_id[NX_FAULT_BUILD_ID_BYTES];
    uint32_t core[8];
    uint32_t status[8];
    uint32_t stack_begin;
    uint32_t stack_end;
    uint32_t source_line;
} nx_fault_capture_t;

/**
 * \brief           Aligned retained storage with explicit little-endian
 * payload.
 *
 * \note            The application reserves this storage outside
 * startup zeroing and validates the chip reset/retention domain. It remains
 *                  live and CPU-accessible through capture and reboot. The
 *                  commit word is written last. CRC detects damage, without
 *                  authentication or a power-loss guarantee. Capture is one
 *                  privileged CPU/security domain; no DMA or concurrent host
 *                  access is permitted. Startup must not initialize a retained
 *                  record before examining it.
 */
typedef struct {
    uint8_t bytes[NX_FAULT_RECORD_BYTES - sizeof(uint32_t)];
    uint32_t committed;
} nx_fault_record_t;

/** \brief Boot reset facts remain independent from a previous crash record. */
typedef struct {
    uint32_t reset_causes;
    bool has_fault;
    nx_fault_capture_t fault;
} nx_reset_record_t;

/**
 * \brief           Capture fixed facts before optional fault interpretation.
 *
 * \param[in,out]   record: Provenance-valid aligned retained CPU storage.
 *
 * \param[in]       facts: Stable readable numeric facts, disjoint from record.
 *
 * \return          SUCCESS, INVALID, or BUSY when a record/capture already
 * exists.
 *
 * \note            Fixed work in task/fault/ISR/masked context. No
 * clock, heap, lock, log, kernel, Flash or transport is called. First capture
 *                  wins; nested capture never overwrites it. Pointer
 * provenance, a usable instruction/stack path and physical retention cannot be
 * established by this function.
 */
nx_result_t nx_fault_record_capture(volatile nx_fault_record_t* record,
                                    const nx_fault_capture_t* facts);

/**
 * \brief           Validate and decode a quiescent retained record.
 *
 * \param[in]       record: Aligned retained storage; no concurrent writer.
 *
 * \param[out]      facts: Separate destination, unchanged on failure.
 *
 * \return          SUCCESS, EMPTY for no commit, IO for corrupt data, INVALID.
 *
 * \note            Task/startup fixed work; retains no pointer and changes no
 *                  source bytes. Unknown versions return UNSUPPORTED.
 */
nx_result_t nx_fault_record_read(const volatile nx_fault_record_t* record,
                                 nx_fault_capture_t* facts);

/**
 * \brief           Acknowledge one exact committed record in normal context.
 *
 * \param[in,out]   record: Retained storage, serialized with readers/capture.
 *
 * \param[in]       sequence: Expected captured identity.
 *
 * \return          SUCCESS, STATE for a different sequence, or read error.
 *
 * \note            Task/startup only. Clears commit, never resets hardware or
 *                  establishes that any persistent copy exists.
 */
nx_result_t nx_fault_record_ack(volatile nx_fault_record_t* record,
                                uint32_t sequence);

/**
 * \brief           Combine already-latched reset bits with optional crash
 * facts.
 *
 * \param[in]       record: Quiescent retained record.
 *
 * \param[in]
 * reset_causes: Existing nx_reset_cause result, unmodified.
 *
 * \param[out] output:
 * Separate output; unchanged for corrupt record.
 *
 * \return          SUCCESS for
 * valid or absent crash; otherwise read error.
 *
 * \note            Startup/task
 * only; no reset flags are read or cleared here.
 */
nx_result_t nx_reset_record_read(const volatile nx_fault_record_t* record,
                                 uint32_t reset_causes,
                                 nx_reset_record_t* output);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_FAULT_RECORD_H */
