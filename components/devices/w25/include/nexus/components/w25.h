/**
 * \file            w25.h
 * \brief           Bounded W25Q128JV commands and retained busy ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_W25_H
#define NEXUS_COMPONENTS_W25_H

#include "nexus/io/spi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_W25_MAX_CHUNK_BYTES 252U
#define NX_W25_PAGE_BYTES      256U
#define NX_W25_SECTOR_BYTES    4096U
#define NX_W25_Q128JV_BYTES    UINT32_C(16777216)
#define NX_W25_Q128JV_ID       UINT32_C(0xef4018)

/** \brief An explicit reviewed model; JEDEC identity alone is not suffix proof.
 */
typedef enum { NX_W25_PROFILE_Q128JV = 1 } nx_w25_profile_t;

/** \brief Last requested array operation, independent of its final outcome. */
typedef enum {
    NX_W25_OPERATION_NONE = 0,
    NX_W25_OPERATION_PROGRAM,
    NX_W25_OPERATION_ERASE_4K
} nx_w25_operation_t;

/**
 * \brief Explicit product region and stable power/clock identity declarations.
 * \note Region offset/size are 4 KiB aligned, nonempty and inside the reviewed
 *       16 MiB array. Public offsets are relative to this caller-selected
 * region. ready_at is the absolute time after the actual rail has satisfied the
 *       datasheet power-up/write delay; the caller must establish that fact.
 *       Domain/epoch labels are declarations, not runtime clock attestation.
 */
typedef struct {
    nx_w25_profile_t profile;
    uint32_t region_offset;
    uint32_t region_bytes;
    uint32_t clock_domain;
    uint32_t clock_epoch;
    nx_time_us_t ready_at;
} nx_w25_config_t;

/** \brief Caller-owned state, zero-initialized before its first initialization.
 * \note Private fields are exposed for static allocation, not caller mutation.
 */
typedef struct {
    const nx_spi_endpoint_t* endpoint;
    nx_clock_t clock;
    nx_w25_config_t config;
    nx_time_us_t last_clock;
    nx_time_us_t operation_deadline;
    uint32_t operation_offset;
    uint32_t operation_bytes;
    nx_result_t operation_result;
    nx_w25_operation_t operation;
    bool initialized;
    bool entered;
    bool pending;
    bool uncertain;
    bool may_have_modified;
    bool clock_fault;
} nx_w25_t;

/** \brief Serialized observation; SUCCESS never implies power-loss durability.
 */
typedef struct {
    nx_w25_operation_t operation;
    nx_result_t result;
    uint32_t offset;
    uint32_t bytes;
    bool initialized;
    bool pending;
    bool uncertain;
    bool may_have_modified;
    bool clock_fault;
} nx_w25_status_t;

/**
 * \brief Probe one explicitly selected awake W25Q128JV through a polling face.
 * \param[in,out] device: Disjoint caller state, live through successful stop.
 * \param[in] endpoint: Stable mode 0/3, single-line, at most 50 MHz SPI
 * binding. \param[in] clock: Ready monotonic microseconds, stable through
 * pending duty. \param[in] config: Explicit region/model/power/clock facts,
 * copied on success. \param[in] deadline: Finite absolute deadline in that same
 * live clock epoch. \return SUCCESS after EF4018 and idle/non-suspended/WEL0
 * observation; BUSY before ready_at or while the chip is busy/armed/suspended.
 * Unknown models reject. Rejection issues no mutating command. \note Task-only
 * serialized calls; no worker, heap, lock, discovery fallback, reset or
 * status/QE/protection writes. JEDEC is necessary, not unique model/revision
 * proof. The caller excludes foreign flash, pin, power, clock and bus owners.
 * Endpoint, ops, context and clock outlive the component and stay disjoint from
 * state and writable payloads.
 */
nx_result_t nx_w25_init(nx_w25_t* device, const nx_spi_endpoint_t* endpoint,
                        nx_clock_t clock, const nx_w25_config_t* config,
                        nx_time_us_t deadline);

/**
 * \brief Read one 1..252-byte region-relative chunk through command 03h.
 * \param[in,out] device: Initialized, idle single owner.
 * \param[in] offset: Offset inside the configured region.
 * \param[out] data: Disjoint output, unchanged on any failed transaction.
 * \param[in] bytes: Positive bounded byte count inside the region.
 * \param[in] deadline: Finite absolute deadline covering complete CS
 * withdrawal. \return SUCCESS or a checked range/state/transport/time error.
 * \note Every return ends CPU access to data. No asynchronous payload borrow.
 */
nx_result_t nx_w25_read(nx_w25_t* device, uint32_t offset, uint8_t* data,
                        size_t bytes, nx_time_us_t deadline);

/**
 * \brief Issue one bounded 02h page program without crossing its 256-byte page.
 * \param[in,out] device: Initialized idle owner; retained while pending.
 * \param[in] offset: Region-relative offset inside one physical page.
 * \param[in] data: 1..252 source bytes, no longer accessed after return.
 * \param[in] bytes: Positive bounded span; target obeys NOR 1-to-0 semantics.
 * \param[in] deadline: Finite absolute deadline including later device busy
 * time. \return SUCCESS means the command was issued, not completed. Validation
 *         rejection has no I/O. A transport/time failure after write-enable
 *         may leave pending/uncertain device duty; inspect status and service.
 * \note Only pre-existing unprotected WPS0/CMP0/BP0 array mode is admitted.
 *       Protection is never unlocked. No caller payload pointer is retained.
 *       This does not verify bytes or promise atomic/durable replacement.
 */
nx_result_t nx_w25_program_start(nx_w25_t* device, uint32_t offset,
                                 const uint8_t* data, size_t bytes,
                                 nx_time_us_t deadline);

/**
 * \brief Issue 20h erase for one 4 KiB-aligned sector in the declared region.
 * \param[in,out] device: Initialized idle owner retained through final service.
 * \param[in] offset: Region-relative, 4 KiB-aligned sector start.
 * \param[in] deadline: Finite operation deadline; cannot preempt a flash pulse.
 * \return Same command-issued and uncertainty contract as program_start.
 * \note No whole-chip erase, erase suspend, implicit retry or product policy.
 */
nx_result_t nx_w25_erase_4k_start(nx_w25_t* device, uint32_t offset,
                                  nx_time_us_t deadline);

/**
 * \brief Perform bounded status progress using a separate cleanup deadline.
 * \param[in,out] device: Same binding, storage and live clock retained.
 * \param[in] cleanup_deadline: Fresh finite budget, never extends operation
 * time. \return BUSY while WIP remains set. SUS never establishes completion.
 * After WIP0/SUS0 and observed WEL0, pending duty ends and the original
 *         operation outcome is returned, including a latched TIMEOUT/IO.
 * \note At most four short SPI transactions; no waiting loop. WRDI may clear an
 *       observed idle WEL, then is checked. No reset, resume, unlock or replay.
 *       Lost clock epochs retain pending duty. A quiescent device does not
 *       prove old data, erase contents or power-loss outcome; read back under
 *       product policy. Shared SPI recovery is the outer bus owner's duty.
 */
nx_result_t nx_w25_service(nx_w25_t* device, nx_time_us_t cleanup_deadline);

/**
 * \brief Observe component state and the last requested operation result.
 * \param[in] device: Stable serialized state, no in-progress callback.
 * \param[out] status: Disjoint caller output, unchanged on rejection.
 * \return SUCCESS or INVALID/BUSY; no physical I/O or qualification inference.
 */
nx_result_t nx_w25_get_status(const nx_w25_t* device, nx_w25_status_t* status);

/**
 * \brief Forget a known-quiescent component binding after callers join.
 * \param[in,out] device: State with no pending or uncertain array operation.
 * \return SUCCESS after unbinding; BUSY preserves state until service proves
 *         settlement. Clock loss during a pending operation requires external
 *         system recovery; a timeout never revokes that responsibility.
 * \note No I/O, bus stop, pin release, power-off or erasure of SPI owner
 * faults.
 */
nx_result_t nx_w25_stop(nx_w25_t* device);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_W25_H */
