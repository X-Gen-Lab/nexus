/**
 * \file            user_io_async.h
 * \brief           Bounded copied UART user mediation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_USER_IO_ASYNC_H
#define NEXUS_COMPONENTS_USER_IO_ASYNC_H
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif

enum nx_user_io_async_constants {
    NX_USER_IO_ASYNC_ABI = 1U,
    NX_USER_IO_ASYNC_SUBMIT = 1U,
    NX_USER_IO_ASYNC_POLL = 2U,
    NX_USER_IO_ASYNC_CANCEL = 3U,
    NX_USER_IO_ASYNC_CONSUME = 4U
};
/**
 * \brief           Twelve scalar local C gateway words; no principal, address
 * or callback.
 *
 * \note            SUBMIT proposes the exact next nonzero 64-bit epoch for an
 *                  explicit slot within the exact claimed nonzero session and
 * supplies positive length plus an absolute deadline in the existing owner's
 * clock domain. Other actions use the accepted exact epoch and zero
 * length/deadline words. device must match the immutable trusted binding;
 * reserved=0. Word pairs encode low then high halves, independently of the
 *                  compiler enum, pointer and uint64_t alignment ABI. These
 *                  are local C gateway records; an integrating kernel owns
 *                  byte order and any transport serialization.
 */
typedef struct {
    uint32_t abi;
    uint32_t action;
    uint32_t device;
    uint32_t slot;
    uint32_t session_low;
    uint32_t session_high;
    uint32_t epoch_low;
    uint32_t epoch_high;
    uint32_t length;
    uint32_t deadline_low;
    uint32_t deadline_high;
    uint32_t reserved;
} nx_user_io_async_message_t;
/**
 * \brief           Ten initialized scalar words; no privileged pointer.
 *
 * \note            state is a Core nx_request_state_t value encoded as
 * uint32_t. Only SETTLED has a final result/count; other results are BUSY with
 * count zero. accepted=1 denotes the retained copied cell, including a terminal
 * error when the existing owner rejects admission. No hardware effect follows
 * from acceptance alone.
 */
typedef struct {
    uint32_t abi;
    uint32_t slot;
    uint32_t session_low;
    uint32_t session_high;
    uint32_t epoch_low;
    uint32_t epoch_high;
    uint32_t state;
    uint32_t result;
    uint32_t transferred;
    uint32_t accepted;
} nx_user_io_async_reply_t;

typedef struct nx_user_io_async nx_user_io_async_t;
/**
 * \brief           Authenticate and copy one bounded user gateway transaction.
 *
 * \param[in,out]   mediation: Trusted privileged binding and exact copied
 * slots.
 *
 * \param[in]       message: Transient full readable user header, copied once.
 *
 * \param[in]       payload: Transient readable SUBMIT bytes; NULL otherwise.
 *
 * \param[out]      reply: Transient disjoint full writable user reply.
 *
 * \return          SUCCESS after reply copy;
 * identity/span/protocol/copy/control errors otherwise. No user address
 * survives any return. A reply-copy error can follow acceptance: POLL the
 * proposed slot/epoch; duplicate SUBMIT cannot replay or replace it.
 *
 * \note            Privileged unmasked Thread, one serialized gateway executor.
 *                  Authenticate the current principal on every call. The
 * trusted copy port supplies fault containment and mapping stability; range
 * checks alone prove neither. The caller separately drives the existing
 * BusOwner/UARTOwner; this call never services it. CANCEL only requests its
 * drain. CONSUME requires acquire SETTLED and the existing owner's idle/join
 * barrier, copies the reply first, then wipes bytes/releases the exact cell.
 *                  Failed reply copy, cancellation or quarantine never releases
 *                  an accepted cell. No SVC/CMSE entry or NS RTOS is provided.
 */
nx_result_t nx_user_io_async_dispatch(nx_user_io_async_t* mediation,
                                      const nx_user_io_async_message_t* message,
                                      const uint8_t* payload,
                                      nx_user_io_async_reply_t* reply);
/**
 * \brief           Close copied admission and cancel this binding's live work.
 *
 * \note            Trusted privileged management, not a user gateway action.
 *                  Same serialized lifecycle executor as dispatch/join, except
 *                  synchronous identity/copy callbacks may request close. This
 *                  is not a general concurrent cross-task control API.
 *                  Close is permanent; it does not close the underlying owner.
 *                  No worker or provider service runs here. Keep driving the
 *                  owner and authenticate POLL/CONSUME through normal dispatch.
 *                  Identity/copy callbacks may request close, never recursively
 *                  dispatch or reconstruct. All bound storage stays live.
 */
nx_result_t nx_user_io_async_close(nx_user_io_async_t* mediation);
/**
 * \brief           Privileged retirement after permanently lost user mappings.
 *
 * \note            Same trusted serialized management as close/join; never an
 *                  untrusted wire action. The kernel decides when cleanup is
 *                  authorized. First close/cancel and separately drive the real
 *                  owner. Every retained exact cell must acquire SETTLED and
 *                  the owner must be idle. The trusted drained proof then
 * fences future gateway entries and joins all outer observers while retaining
 * the role. Only then are all copied payloads wiped and cells retired without
 * user copyout. Pending, quarantined or unjoined work retains every cell
 * unchanged. Success is idempotent and permanently rejects dispatch. It does
 * not release the role: successful join is still required before reclaiming any
 * bound storage. No forced borrow revocation, process-liveness policy or
 * provider service is supplied.
 */
nx_result_t nx_user_io_async_reap(nx_user_io_async_t* mediation);
/**
 * \brief           Join closed/consumed cells, owner and trusted gateway role.
 *
 * \return          SUCCESS only after no gateway/copy, copied cell or owner
 *                  publisher remains and the trusted role release proves outer
 *                  gateway observers joined; BUSY/error retains role and
 * storage.
 *
 * \note            Trusted serialized management. No reinitialization/reopen,
 *                  hidden provider stop or identity registry is created. A
 *                  successful join permits a fresh object with a larger claimed
 *                  session; never reset or reconstruct this live object.
 */
nx_result_t nx_user_io_async_join(nx_user_io_async_t* mediation);
#ifdef __cplusplus
}
#endif
#endif
