/**
 * \file            block_async.h
 * \brief           Finite block borrows with explicit IRQ and DMA drain
 * \author          Nexus Team
 */
#ifndef NEXUS_BLOCK_ASYNC_H
#define NEXUS_BLOCK_ASYNC_H
#include "nexus/core/request.h"
#include "nexus/io/block.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Stable caller descriptor and payload stay borrowed until SETTLED. */
typedef struct {
    nx_request_t base;
    uint64_t first_block;
    union {
        const void* tx;
        void* rx;
    } buffer;
    size_t data_bytes;
    uint32_t blocks;
    bool write;
} nx_block_request_t;

/**
 * \brief Prepare a finite block receive with writable caller storage.
 * \param[in,out] request: Initialized READY or consumed SETTLED descriptor.
 * \param[in] first_block: Logical first block; complete range must not wrap.
 * \param[out] data: Writable payload; prepare establishes no provider borrow.
 * \param[in] data_bytes: Positive actual capacity, checked by the provider.
 * \param[in] blocks: Positive requested count; geometry is provider-owned.
 * \param[in] deadline: Finite absolute transfer deadline, including queue time.
 * \return SUCCESS, INVALID or STATE; failure leaves the descriptor unchanged.
 * \note Task-only preparation creates no hardware effect or provider borrow.
 *       Caller storage, format and alignment still require actual admission.
 */
nx_result_t nx_block_read_prepare(nx_block_request_t* request,
                                  uint64_t first_block, void* data,
                                  size_t data_bytes, uint32_t blocks,
                                  nx_time_us_t deadline);
/**
 * \brief Prepare a finite block write from immutable caller storage.
 * \param[in,out] request: Initialized READY or consumed SETTLED descriptor.
 * \param[in] first_block: Logical first block; complete range must not wrap.
 * \param[in] data: Immutable source, kept alive through eventual settlement.
 * \param[in] data_bytes: Positive actual readable capacity, provider-checked.
 * \param[in] blocks: Positive requested count; geometry is provider-owned.
 * \param[in] deadline: Finite absolute transfer deadline, including queue time.
 * \return SUCCESS, INVALID or STATE; failure leaves the descriptor unchanged.
 * \note No allocation, payload copy or provider borrow. Prepare again to change
 *       direction; toggling write alone does not select another union member.
 */
nx_result_t nx_block_write_prepare(nx_block_request_t* request,
                                   uint64_t first_block, const void* data,
                                   size_t data_bytes, uint32_t blocks,
                                   nx_time_us_t deadline);
/** \brief Read the selected payload address without granting write access.
 * \param[in] request: Nonnull live descriptor with a correctly selected member.
 * \return Payload address; no bounds, state or hardware check is performed.
 * \note Pure bounded task/IRQ view. A provider writes only buffer.rx after
 *       validating the receive direction; TX storage remains immutable.
 */
static inline const void*
nx_block_request_data(const nx_block_request_t* request) {
    return request->write ? request->buffer.tx : request->buffer.rx;
}

/** \brief Optional finite transport shares the ordinary block session. */
typedef struct {
    nx_result_t (*submit)(void* context, nx_block_request_t* request);
    nx_result_t (*cancel)(void* context, nx_block_request_t* request);
    void (*service)(void* context);
    nx_result_t (*media_removed)(void* context);
} nx_block_async_ops_t;

/** \brief Immutable identity; queries and lookup never start media. */
typedef struct {
    const nx_block_async_ops_t* ops;
    void* context;
} nx_block_async_port_t;

/**
 * \brief Admit a complete finite block request without allocation or workers.
 * \param[in] port: Single privileged unmasked Thread execution owner.
 * \param[in,out] request: READY descriptor, finite deadline and aligned span.
 * \return SUCCESS means ACCEPTED; rejection retains nothing and has no effect.
 * \note Initialize base, then use nx_block_read_prepare/write_prepare. Keep
 *       the matching buffer member and direction immutable through SETTLED.
 *       The complete descriptor and payload remain borrowed. IRQ/DMA
 *       completion alone never releases them. The ordinary face owns explicit
 *       activation, sync and stop of the same media session.
 */
nx_result_t nx_block_async_submit(const nx_block_async_port_t* port,
                                  nx_block_request_t* request);
/**
 * \brief Request cancellation while retaining every borrowed reference.
 * \param[in] port: Same serialized execution owner as submit/service.
 * \param[in,out] request: Exact live descriptor, never a stale pointer.
 * \return SUCCESS begins drain; STATE for another request.
 * \note Cross-task controls use external stable slots and epochs. This call
 *       does not cancel a physical card programming pulse or release storage.
 */
nx_result_t nx_block_async_cancel(const nx_block_async_port_t* port,
                                  nx_block_request_t* request);
/**
 * \brief Advance card readiness, deadlines and final source withdrawal.
 * \param[in] port: Same privileged unmasked Thread executor.
 * \note No worker is created. Failed drain leaves QUARANTINED storage live;
 *       later service can retry the same cleanup. SETTLED is final access.
 */
void nx_block_async_service(const nx_block_async_port_t* port);
/**
 * \brief Report externally established physical removal of this exact card.
 * \param[in] port: Same serialized Thread owner, after Board detect evidence.
 * \return SUCCESS records removal and begins any needed host drain.
 * \note This is not a presence probe or debounce policy. Remove invalidates
 *       geometry; no replacement is admitted until explicit stop/activation.
 *       Removal never revokes an outstanding payload borrow.
 */
nx_result_t nx_block_async_media_removed(const nx_block_async_port_t* port);

#ifdef __cplusplus
}
#endif
#endif
