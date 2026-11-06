/**
 * \file            block.h
 * \brief           Synchronous sector I/O with explicit partial progress
 * \author          Nexus Team
 */
#ifndef NEXUS_BLOCK_H
#define NEXUS_BLOCK_H
#include "nexus/core/time.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Media geometry remains immutable during one initialized session. */
typedef struct {
    uint64_t block_count;
    uint32_t block_bytes;
    uint32_t buffer_alignment;
    bool writable;
} nx_block_geometry_t;
/** \brief Confirmed whole-block prefix; failed block bytes are unspecified. */
typedef struct {
    nx_result_t result;
    uint32_t completed_blocks;
} nx_block_io_result_t;
/** \brief Availability never substitutes for physical card-detect evidence. */
typedef enum {
    NX_BLOCK_UNAVAILABLE,
    NX_BLOCK_READY,
    NX_BLOCK_ACTIVE,
    NX_BLOCK_UNCERTAIN,
    NX_BLOCK_STOPPED
} nx_block_state_t;
/** \brief Shared readonly methods for one serialized execution owner. */
typedef struct {
    const nx_block_geometry_t* (*geometry)(const void* context);
    nx_block_state_t (*state)(const void* context);
    nx_result_t (*activate)(void* context, nx_time_us_t deadline);
    nx_block_io_result_t (*read)(void* context, uint64_t first_block,
                                 void* data, size_t data_bytes, uint32_t blocks,
                                 nx_time_us_t deadline);
    nx_block_io_result_t (*write)(void* context, uint64_t first_block,
                                  const void* data, size_t data_bytes,
                                  uint32_t blocks, nx_time_us_t deadline);
    nx_result_t (*sync)(void* context, nx_time_us_t deadline);
    nx_result_t (*stop)(void* context, nx_time_us_t deadline);
} nx_block_ops_t;
/** \brief Immutable face; lookup never starts clocks or accesses media. */
struct nx_block_port {
    const nx_block_ops_t* ops;
    void* context;
};
/**
 * \brief Query admitted media geometry without starting an operation.
 * \param[in] port: Initialized exact media session, serialized by its owner.
 * \return Geometry valid until session stop/reinitialization, or NULL.
 */
const nx_block_geometry_t* nx_block_port_geometry(const nx_block_port_t* port);
/**
 * \brief Observe the owner state without asserting physical card presence.
 * \param[in] port: Selected face under the same serialized owner.
 * \return Actual state, or UNAVAILABLE for an invalid/missing state method.
 */
nx_block_state_t nx_block_port_state(const nx_block_port_t* port);
/**
 * \brief Explicitly discover one card after infrastructure cold preparation.
 * \param[in,out] port: Same serialized Thread owner; no existing live session.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success after exact card identification and geometry admission;
 *         failure retains any unproved controller/session cleanup owner.
 * \note Factory lookup and Platform startup never discover or replace media.
 *       Geometry is unavailable before activation. A different card requires
 *       explicit stop and activation; errors never silently reprobe a socket.
 */
nx_result_t nx_block_port_activate(const nx_block_port_t* port,
                                   nx_time_us_t deadline);
/**
 * \brief Read whole sectors synchronously with a finite absolute deadline.
 * \param[in,out] port: Single privileged Thread owner; no ISR or reentrancy.
 * \param[in] first_block: Logical sector index, never a byte address.
 * \param[out] data: Aligned caller storage, disjoint from provider/MMIO.
 * \param[in] data_bytes: Accessible bytes, at least blocks times block_bytes.
 * \param[in] blocks: Positive bounded count accepted by the selected provider.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Status and confirmed whole-block prefix; current failed block may
 *         have changed. REJECTED parameters have zero hardware effects.
 * \note No data pointer remains borrowed after return in this synchronous
 *       profile. Media/controller uncertainty may independently retain their
 *       owner, pins and gates and make stop refuse. No DMA is implied.
 */
nx_block_io_result_t nx_block_port_read(const nx_block_port_t* port,
                                        uint64_t first_block, void* data,
                                        size_t data_bytes, uint32_t blocks,
                                        nx_time_us_t deadline);
/**
 * \brief Write sectors; memory completion alone never proves media ready.
 * \param[in,out] port: Single Thread owner with an admitted writable session.
 * \param[in] first_block: Logical sector index, checked before command writes.
 * \param[in] data: Aligned immutable caller storage until this call returns.
 * \param[in] data_bytes: Accessible source bytes, at least the selected extent.
 * \param[in] blocks: Positive bounded count; no erase or product policy.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Status and whole blocks confirmed after card programming readiness.
 *         A failed current block may already have modified media.
 * \note Return ends CPU buffer access but not uncertain media programming;
 *       UNCERTAIN retains controller responsibility. No atomicity, power-loss
 *       recovery or durability beyond the selected protocol is promised.
 */
nx_block_io_result_t nx_block_port_write(const nx_block_port_t* port,
                                         uint64_t first_block, const void* data,
                                         size_t data_bytes, uint32_t blocks,
                                         nx_time_us_t deadline);
/**
 * \brief Observe protocol readiness without replaying a prior failed write.
 * \param[in,out] port: Same initialized media session and serialized owner.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success only after proved host/media readiness; failure retains
 *         uncertainty and never upgrades a prior transfer outcome.
 */
nx_result_t nx_block_port_sync(const nx_block_port_t* port,
                               nx_time_us_t deadline);
/**
 * \brief Quiesce a session; a timeout never authorizes clock/pad withdrawal.
 * \param[in,out] port: Same owner, live through a refused stop/retry.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success after actual publisher/media withdrawal, or failure with
 *         provider cleanup responsibility retained. Platform cold close owns
 *         subsequent pad and gate restoration.
 */
nx_result_t nx_block_port_stop(const nx_block_port_t* port,
                               nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
