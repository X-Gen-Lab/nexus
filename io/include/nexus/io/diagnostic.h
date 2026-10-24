/**
 * \file            diagnostic.h
 * \brief           Explicit copied facts explain pending I/O without recovery
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_DIAGNOSTIC_H
#define NEXUS_IO_DIAGNOSTIC_H

#include "nexus/core/request_observer.h"
#include "nexus/io/stream_budget.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_IO_FACT_REQUEST   (1U << 0)
#define NX_IO_FACT_STREAM    (1U << 1)
#define NX_IO_FACT_LIFECYCLE (1U << 2)
#define NX_IO_FACT_OWNER     (1U << 3)
#define NX_IO_FACT_SERVICE   (1U << 4)
#define NX_IO_FACT_OBSERVERS (1U << 5)
#define NX_IO_FACT_ALL       ((1U << 6) - 1U)

#define NX_IO_DIAG_QUEUED             (1U << 0)
#define NX_IO_DIAG_ACTIVE             (1U << 1)
#define NX_IO_DIAG_DRAINING           (1U << 2)
#define NX_IO_DIAG_QUARANTINED        (1U << 3)
#define NX_IO_DIAG_SETTLED            (1U << 4)
#define NX_IO_DIAG_DEADLINE_REACHED   (1U << 5)
#define NX_IO_DIAG_NOT_STARTED        (1U << 6)
#define NX_IO_DIAG_STOPPING           (1U << 7)
#define NX_IO_DIAG_OWNER_CLOSED       (1U << 8)
#define NX_IO_DIAG_NO_FREE_SLOT       (1U << 9)
#define NX_IO_DIAG_OWNER_PENDING      (1U << 10)
#define NX_IO_DIAG_SERVICE_OVERDUE    (1U << 11)
#define NX_IO_DIAG_OBSERVERS_PENDING  (1U << 12)
#define NX_IO_DIAG_STREAM_OUTSTANDING (1U << 13)
#define NX_IO_DIAG_STREAM_CURSOR_BUSY (1U << 14)
#define NX_IO_DIAG_STREAM_LOSS        (1U << 15)

/**
 * \brief           Stable caller copies, with independently known fact groups.
 * \note            request/stream may be NULL; their presence establishes the
 *                  corresponding group. known accepts only LIFECYCLE, OWNER,
 *                  SERVICE and OBSERVERS. Zero-initialized unknown values
 *                  never establish readiness, health or a missed service.
 *                  OWNER fields may come from an owner observer only when its
 *                  diagnostic_result succeeded. They describe admission and
 *                  pending work, not the cause of a BUSY service result.
 *                  SERVICE timestamps use the same monotonic domain; both are
 *                  finite and maximum_service_gap_us is a finite positive
 *                  declared budget. This helper reads no clock itself.
 *                  Copies remain stable throughout this call. Observations
 *                  need not be simultaneous across devices.
 */
typedef struct {
    const nx_request_observation_t* request;
    const nx_stream_observation_t* stream;
    uint32_t known;
    nx_time_us_t last_service_at;
    nx_time_us_t observed_at;
    nx_time_us_t maximum_service_gap_us;
    bool started;
    bool stopping;
    bool owner_accepting;
    bool owner_queued;
    bool owner_slot_available;
    bool observers_pending;
} nx_io_diagnostic_facts_t;

/**
 * \brief           Independent findings and unavailable fact groups.
 * \note            Zero findings is not a health certificate. SETTLED does not
 *                  establish successful I/O or joined observers. Outstanding
 *                  stream blocks include producer, ready and consumer loans;
 *                  cursor busy describes only the ordinary stream cursor.
 *                  Loss is pending stream metadata in this snapshot, not a
 *                  cumulative total; publication moves it into a later block.
 */
typedef struct {
    uint32_t findings;
    uint32_t unknown_facts;
} nx_io_diagnostic_t;

/**
 * \brief           Classify supplied observations without touching live I/O.
 * \param[in]       facts: Stable aligned copies; no pointer is retained.
 * \param[out]      out: Aligned output distinct from every input span.
 * \return          SUCCESS, INVALID for malformed facts/alias, STATE for
 *                  reversed service time. Rejection leaves output unchanged.
 * \note            Task/IRQ bounded fixed work, no MMIO, locks, allocation,
 *                  service or recovery. Deadline and quarantine never revoke
 *                  an accepted borrow. Callers must still drain controls and
 *                  join observers before storage reuse.
 */
nx_result_t nx_io_diagnose(const nx_io_diagnostic_facts_t* facts,
                           nx_io_diagnostic_t* out);

/**
 * \brief           Optional stable reason key for exactly one finding bit.
 * \param[in]       finding: One NX_IO_DIAG flag, never a combined mask.
 * \return          Immutable static text, or NULL for zero/unknown/many bits.
 * \note            Task/IRQ constant work, no formatting or allocation.
 */
const char* nx_io_diagnostic_name(uint32_t finding);

#ifdef __cplusplus
}
#endif
#endif
