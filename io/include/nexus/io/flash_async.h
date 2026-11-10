/**
 * \file            flash_async.h
 * \brief           Optional finite physical Flash pulses with retained drain
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_FLASH_ASYNC_H
#define NEXUS_IO_FLASH_ASYNC_H
#include "nexus/core/request.h"
#include "nexus/io/flash.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Explicit finite program or whole-sector erase operation. */
typedef enum {
    NX_FLASH_ASYNC_PROGRAM = 0,
    NX_FLASH_ASYNC_ERASE = 1
} nx_flash_async_operation_t;
/** \brief Caller-owned finite operation; base.transferred counts bytes.
 * \note Request and program input remain immutable/borrowed until observed
 *       SETTLED, including cancellation, deadline and hardware faults. A pulse
 *       cannot be cancelled. Partial success is the completed prefix only;
 *       interrupted/failed physical units require caller recovery policy.
 */
typedef struct {
    nx_request_t base;
    nx_flash_async_operation_t operation;
    uint32_t offset;
    const void* data;
    size_t length;
} nx_flash_async_request_t;
/** \brief Static methods of the same physical Flash owner, no queue/worker. */
typedef struct {
    nx_result_t (*submit)(void* context, nx_flash_async_request_t* request);
    nx_result_t (*cancel)(void* context, nx_flash_async_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
} nx_flash_async_ops_t;
/** \brief Immutable optional face; synchronous Flash contract is unchanged.
 * \note One privileged task serializes this face, the associated Flash face
 *       and cold lifecycle. Configuration/state/faces outlive every retained
 *       request. Factory lookup neither initializes nor acquires hardware.
 */
struct nx_flash_async_port {
    const nx_flash_async_ops_t* ops;
    void* context;
};
/** \brief Prepare a finite request without accessing payload or hardware.
 * \param[in,out] request: Initialized READY or consumed SETTLED storage.
 * \param[in] operation: Explicit program or erase.
 * \param[in] offset: Physical byte offset; provider checks geometry.
 * \param[in] data: Program input, NULL for erase; disjoint from request.
 * \param[in] length: Positive byte count; provider checks alignment/sectors.
 * \param[in] deadline: Finite absolute Core monotonic deadline.
 * \return SUCCESS or INVALID/STATE with request unchanged on rejection.
 * \note Join all previous observers before reuse; no allocation occurs.
 */
nx_result_t nx_flash_async_prepare(nx_flash_async_request_t* request,
                                   nx_flash_async_operation_t operation,
                                   uint32_t offset, const void* data,
                                   size_t length, nx_time_us_t deadline);
/** \brief Admit one finite operation, retaining storage only on SUCCESS.
 * \param[in] port: Live selected same-Flash face, serialized task context.
 * \param[in,out] request: Exact READY descriptor and immutable input.
 * \return SUCCESS means ACCEPTED, including immediate settled failure.
 * \note Rejection creates no borrow or new hardware effect. SETTLED proves
 *       the pulse idle and final provider access. Deadline is not drain.
 */
nx_result_t nx_flash_async_submit(const nx_flash_async_port_t* port,
                                  nx_flash_async_request_t* request);
/** \brief Prevent further pulses while allowing the current pulse to drain.
 * \param[in] port: Same serialized owner as submit.
 * \param[in,out] request: Exact live accepted descriptor.
 * \return SUCCESS starts cancellation; it does not imply settlement.
 * \note First observed terminal cause wins; storage stays borrowed until idle.
 */
nx_result_t nx_flash_async_cancel(const nx_flash_async_port_t* port,
                                  nx_flash_async_request_t* request);
/** \brief Make bounded pulse/drain progress without sleeping or rearming.
 * \param[in] port: Same privileged serialized task owner.
 * \return SUCCESS after progress/settlement; BUSY while draining or IO with
 *         retained QUARANTINED storage when hardware cannot prove idle.
 * \note Caller schedules service; there is no hidden IRQ, worker or queue.
 */
nx_result_t nx_flash_async_service(const nx_flash_async_port_t* port);
/** \brief Seal admission and cancel/drain any current finite operation.
 * \param[in] port: Same privileged serialized task owner.
 * \return SUCCESS only after request detach; BUSY/IO retains all
 * responsibility. \note This does not close the physical bank. Cold close is a
 * separate platform operation and cannot withdraw a busy physical pulse.
 */
nx_result_t nx_flash_async_stop(const nx_flash_async_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
