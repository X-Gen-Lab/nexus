/**
 * \file            nand.h
 * \brief           Finite raw NAND page/OOB operations and hardware ECC codes
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_NAND_H
#define NEXUS_IO_NAND_H
#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Explicit device geometry in bytes, independent of bus word width.
 * \note Selected software facts, not discovered good-block capacity. No bad
 *       block policy, ECC correction/placement, FTL or filesystem is supplied.
 */
typedef struct {
    uint32_t page_bytes;
    uint32_t oob_bytes;
    uint32_t pages_per_block;
    uint32_t blocks;
    uint16_t ecc_bytes;
    uint8_t width;
    uint8_t column_cycles;
    uint8_t row_cycles;
} nx_nand_geometry_t;
/** \brief Device status with protocol bit positions confined to the provider.
 * \note failed reports the device's failure latch; it does not identify a
 *       page or classify bit errors. Ready alone never releases a live loan.
 */
typedef struct {
    bool ready;
    bool failed;
    bool write_protected;
} nx_nand_status_t;
/** \brief Explicit opaque ECC encoding identity, not a portable decoder. */
typedef enum {
    NX_NAND_ECC_UNKNOWN = 0,
    NX_NAND_ECC_STM32_FSMC_RAW32
} nx_nand_ecc_encoding_t;
/** \brief Selected ECC step and output container without correction policy.
 * \note step_bytes is main data covered by one code. code_bytes describes
 *       storage, not meaningful parity bits. STM32_FSMC_RAW32 is one numeric
 *       uint32_t per step, not a serialized byte sequence or a standard codec.
 *       Placement, comparison, correction and bad-block policy stay external.
 */
typedef struct {
    nx_nand_ecc_encoding_t encoding;
    uint32_t step_bytes;
    uint8_t code_bytes;
} nx_nand_ecc_format_t;
/** \brief Raw one-page, OOB-only, whole-block erase or device reset. */
typedef enum {
    NX_NAND_READ_PAGE = 0,
    NX_NAND_PROGRAM_PAGE,
    NX_NAND_READ_OOB,
    NX_NAND_PROGRAM_OOB,
    NX_NAND_ERASE_BLOCK,
    NX_NAND_RESET
} nx_nand_operation_t;
/** \brief Finite raw request, no implicit bad-block or OOB interpretation.
 * \note page identifies an absolute physical page, including bad blocks.
 *       READ/PROGRAM_PAGE requires exactly page_bytes; optional OOB is exactly
 *       oob_bytes. OOB-only uses data=NULL and exact OOB. Erase names the first
 *       page of a block. Reset has no buffers/page. Optional ECC output has
 *       one uint32_t per main-data chunk; raw codes have no
 * placement/correction policy. READ destinations, PROGRAM sources and ECC codes
 * are borrowed through acquire-observed SETTLED. Cancellation cannot abort a
 * pulse. base.transferred is complete main+OOB bytes on SUCCESS, zero on error;
 *       failed read storage can contain unqualified partial data.
 */
typedef struct {
    nx_request_t base;
    nx_nand_operation_t operation;
    uint32_t page;
    void* data;
    size_t bytes;
    void* oob;
    size_t oob_bytes;
    uint32_t* ecc;
    size_t ecc_count;
} nx_nand_request_t;
/** \brief Same static physical owner, called by one privileged task. */
typedef struct {
    const nx_nand_geometry_t* (*geometry)(const void* context);
    nx_result_t (*id)(void* context, uint8_t* bytes, size_t count);
    nx_result_t (*status)(void* context, nx_nand_status_t* status);
    nx_result_t (*submit)(void* context, nx_nand_request_t* request);
    nx_result_t (*cancel)(void* context, nx_nand_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*ecc_format)(const void* context,
                              nx_nand_ecc_format_t* format);
} nx_nand_ops_t;
/** \brief Immutable raw face; factory returns existing identity only.
 * \note State/facts/faces outlive all calls and retained requests. No hidden
 *       worker/queue/heap/IRQ exists. Calls are serialized with cold lifecycle.
 */
struct nx_nand_port {
    const nx_nand_ops_t* ops;
    void* context;
};
/** \brief Query explicit raw geometry without starting device operations.
 * \param[in] port: Stable selected serialized face.
 * \return Immutable facts or NULL before successful prepare/invalid face.
 * \note No ID inference or bad-block scan is performed.
 */
const nx_nand_geometry_t* nx_nand_geometry(const nx_nand_port_t* port);
/** \brief Read up to eight ID bytes while idle, retaining no output.
 * \param[in] port: Idle selected privileged task owner.
 * \param[out] bytes: Distinct writable internal output.
 * \param[in] count: Positive 1..8 raw signature bytes.
 * \return SUCCESS, BUSY, INVALID or hardware error; no product identification.
 * \note Read-ID does not authorize a geometry or command-set substitution.
 */
nx_result_t nx_nand_id(const nx_nand_port_t* port, uint8_t* bytes,
                       size_t count);
/** \brief Read semantic device status while no finite request owns the bus.
 * \param[in] port: Idle selected privileged task owner.
 * \param[out] status: Disjoint ready/failure/protection snapshot.
 * \return SUCCESS or exact rejected/hardware result, no retained output.
 * \note A status read alone never revokes any request or bank lease.
 */
nx_result_t nx_nand_status(const nx_nand_port_t* port,
                           nx_nand_status_t* status);
/** \brief Query the selected ECC container and step without device commands.
 * \param[in] port: Idle prepared selected owner, serialized with lifecycle.
 * \param[out] format: Disjoint caller output, unchanged on rejection.
 * \return SUCCESS, UNSUPPORTED for absent format metadata, or the provider's
 *         lifecycle/context/bus error. No storage is retained or allocated.
 * \note This identifies existing raw code output, not a decoder, OOB layout,
 *       guaranteed correction strength or physical media qualification.
 */
nx_result_t nx_nand_ecc_format(const nx_nand_port_t* port,
                               nx_nand_ecc_format_t* format);
/** \brief Prepare metadata without touching page/OOB or hardware.
 * \param[in,out] request: Initialized READY or consumed SETTLED storage;
 *                          operation/page/buffers are caller-filled afterward.
 * \param[in] deadline: Finite absolute Core monotonic deadline.
 * \return SUCCESS or INVALID/STATE; failure leaves descriptor unchanged.
 * \note Caller joins previous observers and fills every operation field before
 *       submission; provider checks complete spans/geometry before admission.
 */
nx_result_t nx_nand_prepare(nx_nand_request_t* request, nx_time_us_t deadline);
/** \brief Admit exactly one finite raw operation with borrowed spans.
 * \param[in] port: Live selected privileged serialized task owner.
 * \param[in,out] request: Exact READY immutable descriptor and spans.
 * \return SUCCESS means ACCEPTED, including immediate settled failure.
 * \note Rejection retains nothing and issues no command. A deadline cannot
 *       revoke program/erase/reset or unsettled ECC hardware accesses.
 */
nx_result_t nx_nand_submit(const nx_nand_port_t* port,
                           nx_nand_request_t* request);
/** \brief Stop further data/pulses and drain the currently accepted operation.
 * \param[in] port: Same serialized privileged task owner.
 * \param[in,out] request: Exact current live descriptor.
 * \return SUCCESS starts cancellation; SETTLED alone releases borrows.
 * \note A partial program-data load is reset and that reset must become ready.
 */
nx_result_t nx_nand_cancel(const nx_nand_port_t* port,
                           nx_nand_request_t* request);
/** \brief Make bounded command, up to 32 bus units, ECC or ready progress.
 * \param[in] port: Same serialized privileged task owner.
 * \return SUCCESS after progress/settlement; BUSY during device/ECC drain;
 *         IO with retained QUARANTINED storage on missing hardware.
 * \note Caller schedules service; no sleeping, replay or implicit rearm.
 */
nx_result_t nx_nand_service(const nx_nand_port_t* port);
/** \brief Seal admission and cancel/drain the current request.
 * \param[in] port: Same serialized privileged task owner.
 * \return SUCCESS after final request detach; BUSY/IO retains bank and borrow.
 * \note Cold bank close is separate and refuses any pending device operation.
 */
nx_result_t nx_nand_stop(const nx_nand_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
