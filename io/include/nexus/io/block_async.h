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
    void* data;
    size_t data_bytes;
    uint32_t blocks;
    bool write;
} nx_block_request_t;

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
 * \note Initialize base with nx_request_initialize/prepare. Entire descriptor
 *       and payload remain stable until acquire-observed SETTLED. IRQ/DMA
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
