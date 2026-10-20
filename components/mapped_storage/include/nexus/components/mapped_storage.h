/**
 * \file            mapped_storage.h
 * \brief           Bounded read mappings and exclusive storage requests
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_MAPPED_STORAGE_H
#define NEXUS_COMPONENTS_MAPPED_STORAGE_H
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Read-only consumers; capability does not prove physical routing. */
typedef enum {
    NX_MAPPED_STORAGE_CPU_READ = 1U,
    NX_MAPPED_STORAGE_XIP = 2U,
    NX_MAPPED_STORAGE_DMA_READ = 4U
} nx_mapped_storage_reader_t;

/** \brief Exact byte geometry; units divide size and one another. */
typedef struct {
    const uint8_t* mapped_base;
    size_t size;
    size_t program_unit;
    size_t erase_unit;
    uint32_t readers;
} nx_mapped_storage_geometry_t;

/** \brief Physical mutation, separate from read-map ownership. */
typedef enum {
    NX_MAPPED_STORAGE_PROGRAM = 0,
    NX_MAPPED_STORAGE_ERASE
} nx_mapped_storage_operation_t;

/**
 * \brief Caller-owned request and input, borrowed through SETTLED
 * \note Initialize base before prepare. PROGRAM borrows length input bytes;
 * ERASE requires NULL input. Geometry alignment is checked at admission.
 * Partial effects survive cancellation, timeout and I/O failure. No replay.
 */
typedef struct {
    nx_request_t base;
    nx_mapped_storage_operation_t operation;
    size_t offset;
    const void* data;
    size_t length;
} nx_mapped_storage_request_t;

/**
 * \brief Stable instance-bound identity of one read mapping
 * \note Fields are immutable until release. Data stays read-only. Join every
 * CPU, XIP instruction fetch and DMA reader before release; a copied token
 * grants no additional loan. A failed release keeps the complete loan live.
 */
typedef struct {
    const void* instance;
    const void* data;
    size_t length;
    size_t slot;
    uint64_t epoch;
} nx_mapped_storage_loan_t;

/** \brief Stable writer identity; drain controls before reuse or destruction.
 */
typedef struct {
    const void* instance;
    uint64_t epoch;
} nx_mapped_storage_ticket_t;

/** \brief Shared immutable operations; every callback receives instance state.
 */
typedef struct {
    const nx_mapped_storage_geometry_t* (*geometry)(const void* context);
    nx_result_t (*acquire)(void* context, nx_mapped_storage_reader_t reader,
                           size_t offset, size_t length,
                           nx_mapped_storage_loan_t* loan);
    nx_result_t (*release)(void* context, const nx_mapped_storage_loan_t* loan);
    nx_result_t (*submit)(void* context, nx_mapped_storage_request_t* request,
                          nx_mapped_storage_ticket_t* ticket);
    nx_result_t (*cancel)(void* context, nx_mapped_storage_ticket_t ticket);
    void (*service)(void* context);
    nx_result_t (*stop)(void* context);
} nx_mapped_storage_ops_t;

/** \brief Immutable face; context and all retained storage outlive users. */
typedef struct {
    const nx_mapped_storage_ops_t* ops;
    void* context;
} nx_mapped_storage_port_t;

/**
 * \brief Prepare unborrowed caller storage without accessing payload
 * \return SUCCESS or INVALID/STATE; no retained references on failure.
 */
nx_result_t nx_mapped_storage_prepare(nx_mapped_storage_request_t* request,
                                      nx_mapped_storage_operation_t operation,
                                      size_t offset, const void* data,
                                      size_t length, nx_time_us_t deadline);
/** \brief Query immutable geometry without starting hardware. */
const nx_mapped_storage_geometry_t*
nx_mapped_storage_geometry(const nx_mapped_storage_port_t* port);
/**
 * \brief Acquire one bounded, positive read-only span on a serialized executor
 * \return SUCCESS or INVALID/UNSUPPORTED/BUSY/EXHAUSTED/STATE. Rejection
 * retains no loan. Live read maps exclude all command/program/erase/cache/DMA
 * writers. CPU/XIP/DMA consumers may run concurrently after successful
 * acquisition.
 */
nx_result_t nx_mapped_storage_acquire(const nx_mapped_storage_port_t* port,
                                      nx_mapped_storage_reader_t reader,
                                      size_t offset, size_t length,
                                      nx_mapped_storage_loan_t* loan);
/**
 * \brief Release an exact live loan after its consumers have joined
 * \return SUCCESS, INVALID/STATE, or backend unmap error retaining the loan.
 * Task-only, same serialized executor; no forced revocation or timeout release.
 */
nx_result_t nx_mapped_storage_release(const nx_mapped_storage_port_t* port,
                                      const nx_mapped_storage_loan_t* loan);
/**
 * \brief Admit one exclusive PROGRAM/ERASE request with an absolute deadline
 * \return SUCCESS is ACCEPTED, possibly settled. Rejection retains no request
 * or payload and produces no hardware effects. No map loan may remain live.
 * Task-only serialized owner; no workers, allocation, locks or queue exist.
 */
nx_result_t nx_mapped_storage_submit(const nx_mapped_storage_port_t* port,
                                     nx_mapped_storage_request_t* request,
                                     nx_mapped_storage_ticket_t* ticket);
/**
 * \brief Request cancellation by instance and nonrepeating writer epoch
 * \return SUCCESS starts/observes drain; STATE rejects stale/foreign identity.
 * Cancellation never releases storage. Continue service through drain.
 */
nx_result_t nx_mapped_storage_cancel(const nx_mapped_storage_port_t* port,
                                     nx_mapped_storage_ticket_t ticket);
/**
 * \brief Advance one bounded backend step or drain attempt without waiting
 * \note Backend/IRQ/data references detach before release-publication of
 * SETTLED, which is the final request access. Failed drain is QUARANTINED;
 * storage remains borrowed and later service may retry source withdrawal.
 */
void nx_mapped_storage_service(const nx_mapped_storage_port_t* port);
/**
 * \brief Close admission and request writer cancellation
 * \return SUCCESS only after all map loans and writer sources are gone;
 * BUSY retains storage and needs release/service. Repeated successful stop
 * performs no backend operation. The owner cannot restart or revoke loans.
 */
nx_result_t nx_mapped_storage_stop(const nx_mapped_storage_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
