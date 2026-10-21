/**
 * \file            mapped_storage_owner.h
 * \brief           Explicit portable mapped-storage constructor and storage
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_MAPPED_STORAGE_OWNER_H
#define NEXUS_COMPONENTS_MAPPED_STORAGE_OWNER_H
#include "nexus/components/mapped_storage.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Private provider boundary; immutable callbacks and independent context
 * \note Context is one explicitly sized private state span. Geometry and
 * mapped address stay fixed. map SUCCESS establishes the
 * readable aperture; map failure leaves no effects. unmap SUCCESS proves all
 * aperture/cache references withdrawn; failure retains the last map loan.
 * start is called only after request admission and mapping exclusion. start
 * errors may have effects and always proceed through drain. poll advances
 * bounded work, returns BUSY or a terminal result, and supplies monotonically
 * increasing completed bytes. cancel initiates withdrawal but proves no drain.
 * drain SUCCESS detaches every hardware/IRQ/cache/DMA/descriptor/data source;
 * BUSY retains ownership, other errors quarantine it. No callback retains the
 * request itself. Command/cache/DMA writers outside these callbacks are
 * forbidden while the owner exists. Physical adapters own cache-line domains,
 * XIP execution exclusion and hardware-specific drain proof.
 */
typedef struct {
    void* context;
    size_t context_size;
    nx_mapped_storage_geometry_t geometry;
    nx_result_t (*map)(void* context);
    nx_result_t (*unmap)(void* context);
    nx_result_t (*start)(void* context, nx_mapped_storage_operation_t operation,
                         size_t offset, const void* data, size_t length);
    nx_result_t (*poll)(void* context, size_t* completed);
    nx_result_t (*cancel)(void* context);
    nx_result_t (*drain)(void* context);
} nx_mapped_storage_backend_t;

/** \brief Exact caller-owned map capacity; fields belong to the owner. */
typedef struct {
    uint64_t epoch;
    size_t next;
    const void* data;
    size_t length;
    bool live;
} nx_mapped_storage_slot_t;

/**
 * \brief Caller-owned state, published only after successful initialization
 * \note All methods and controls use one serialized task executor. Consumers
 * may use their maps elsewhere, then join before release. No implicit IRQ
 * guard or cross-task synchronization is supplied. Keep state, slots, backend
 * context, clock and face alive through stop and control/consumer join.
 */
typedef struct {
    nx_mapped_storage_backend_t backend;
    nx_clock_t clock;
    nx_mapped_storage_slot_t* slots;
    size_t capacity;
    size_t free_head;
    size_t loans;
    nx_mapped_storage_request_t* active;
    uint64_t writer_epoch;
    size_t completed;
    nx_result_t terminal;
    bool draining;
    bool accepting;
} nx_mapped_storage_owner_t;

/** \brief One read-only method table shared by every portable owner instance.
 */
extern const nx_mapped_storage_ops_t nx_mapped_storage_owner_ops;

/**
 * \brief Bind unused explicit storage without hardware or payload access
 * \return SUCCESS or INVALID; zero capacity, overlapping metadata/aperture,
 * overflowing spans, inconsistent units and missing callbacks reject.
 * Caller constructs a const face with owner_ops and this owner context.
 */
nx_result_t nx_mapped_storage_owner_init(nx_mapped_storage_owner_t* owner,
                                         nx_mapped_storage_slot_t* slots,
                                         size_t capacity,
                                         nx_mapped_storage_backend_t backend,
                                         nx_clock_t clock);
#ifdef __cplusplus
}
#endif
#endif
