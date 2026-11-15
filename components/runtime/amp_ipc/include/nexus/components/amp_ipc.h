/**
 * \file            amp_ipc.h
 * \brief           Bounded epoch-safe unidirectional shared-memory loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_AMP_IPC_H
#define NEXUS_COMPONENTS_AMP_IPC_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Explicit port-isolation unit; supported cache granules divide 64. */
#define NX_AMP_IPC_UNIT 64U
/** \brief One writer's isolated publication word; accesses use the port only.
 */
typedef struct {
    uint32_t value;
    uint32_t reserved[15];
} nx_amp_word_t;
/** \brief Shared descriptor; offsets refer to the configured payload window. */
typedef struct {
    uint32_t sender_epoch;
    uint32_t receiver_epoch;
    uint32_t sequence;
    uint32_t offset;
    uint32_t length;
    uint32_t reserved[11];
} nx_amp_record_t;
/**
 * \brief           Caller-owned shared slot with disjoint writer cache lines
 * \note            Sender alone writes offer/published; receiver alone writes
 *                  receipt/acknowledged. All four units must be reachable by
 *                  both CPUs. Cold initialization precedes either endpoint.
 */
typedef struct {
    nx_amp_record_t offer;
    nx_amp_word_t published;
    nx_amp_record_t receipt;
    nx_amp_word_t acknowledged;
} nx_amp_wire_slot_t;
typedef enum { NX_AMP_SENDER = 0, NX_AMP_RECEIVER } nx_amp_role_t;
typedef struct nx_amp_ipc nx_amp_ipc_t;
/**
 * \brief           Hardware/ownership boundary supplied by the integration
 * \note            Every callback is synchronous, bounded and non-reentrant.
 *                  claim grants the unique role owner of this exact lane and
 *                  atomically rejects a reused/non-increasing local boot epoch,
 *                  even after ordinary join. Its authoritative hardware boot
 *                  owner preserves this history across CPU resets; exhaustion
 *                  fails closed. Rejection has no effects. unclaim follows
 *                  final join only and never erases boot identity history.
 *                  clean/invalidate cover exact complete isolated cache lines,
 *                  finish their architectural barriers, and may fail. A failed
 *                  clean MUST NOT publish a control word. acquire/release are
 *                  infallible atomic 32-bit coherent control operations: the
 *                  port owns any control-cache maintenance. They must order
 *                  descriptor/payload access across both CPUs and bus fabric.
 *                  stopped proves the named old peer CPU, DMA, IRQ and all
 *                  observers can never access the lane or payload again. The
 *                  context binds the exact physical lane and peer execution
 *                  epoch covered by stopped. An epoch observation, cancel,
 *                  deadline or elapsed timeout never supplies this proof.
 *                  No existing single-core SoC supplies it.
 */
typedef struct {
    nx_result_t (*claim)(void* context, const void* owner,
                         const nx_amp_wire_slot_t* lane, nx_amp_role_t role,
                         uint32_t local_epoch);
    void (*unclaim)(void* context, const void* owner);
    nx_result_t (*clean)(void* context, const void* address, size_t bytes);
    nx_result_t (*invalidate)(void* context, const void* address, size_t bytes);
    uint32_t (*acquire)(void* context, const uint32_t* word);
    void (*release)(void* context, uint32_t* word, uint32_t value);
    nx_result_t (*stopped)(void* context, uint32_t old_peer_epoch);
} nx_amp_port_ops_t;
/** \brief Instance/boot/slot identity; endpoint outlives every ticket copy. */
typedef struct {
    nx_amp_ipc_t* owner;
    uint32_t local_epoch;
    uint32_t peer_epoch;
    uint32_t slot;
    uint32_t sequence;
} nx_amp_ticket_t;
/**
 * \brief           Receiver's borrowed read-only shared span
 * \note            Caller must join every local reader before finish. A valid
 *                  finish invocation consumes the caller view even when link
 *                  acknowledgement fails; endpoint storage remains retained.
 */
typedef struct {
    nx_amp_ticket_t ticket;
    const void* data;
    size_t bytes;
} nx_amp_view_t;
/** \brief Stable close identity; endpoint lives longer than all control copies.
 */
typedef struct {
    nx_amp_ipc_t* owner;
    uint32_t local_epoch;
    uint32_t peer_epoch;
} nx_amp_close_t;
/**
 * \brief           Publish one complete-cache-line payload span without copying
 * \return          SUCCESS transfers a read-only borrow until exact ACK poll or
 *                  successful reset_join. Rejection retains nothing and does
 *                  not publish. EXHAUSTED means no reusable slot identity.
 * \note            Serialized owner Thread only, same rule for every API below.
 *                  No waits, allocation, scheduler, ISR or implicit worker.
 *                  Outputs must not overlap any live instance/lane storage.
 *                  Producer must finish DMA/other writers before this call.
 */
nx_result_t nx_amp_ipc_send(nx_amp_ipc_t* ipc, uint32_t offset, uint32_t bytes,
                            nx_amp_ticket_t* ticket);
/** \brief Acquire-observe an exact ACK; BUSY/IO/STATE retain the sender borrow.
 */
nx_result_t nx_amp_ipc_poll(nx_amp_ticket_t ticket);
/** \brief Acquire one offer after descriptor/payload invalidation succeeds. */
nx_result_t nx_amp_ipc_receive(nx_amp_ipc_t* ipc, nx_amp_view_t* view);
/**
 * \brief           Join the caller view, then publish its exact ACK
 * \note            Valid ticket invocation permanently joins local readers.
 *                  IO or STATE retains endpoint/slot storage; retry finish or
 *                  use reset_join after closing. Stale tickets change nothing.
 */
nx_result_t nx_amp_ipc_finish(nx_amp_ticket_t ticket);
/** \brief Close admission; outstanding send/receive loans remain retained. */
nx_result_t nx_amp_ipc_close(nx_amp_ipc_t* ipc, nx_amp_close_t* control);
/**
 * \brief           Join a closed endpoint after all exact ACKs/local returns
 * \note            BUSY, IO or STATE retain the unique owner. Success performs
 *                  its final access and unclaims the role. Storage cannot be
 *                  reconstructed: boot/sequence identities never reset here.
 */
nx_result_t nx_amp_ipc_join(nx_amp_close_t control);
/**
 * \brief           Join after an explicit old-peer stop proof
 * \note            Closed endpoint required; receiver views must have been
 *                  consumed by finish first. Failed proof retains ALL sender
 *                  loans and ownership. Success retires this endpoint; any
 *                  replacement uses a strictly newer nonzero boot epoch from
 *                  the external boot owner, never rollover/reuse.
 */
nx_result_t nx_amp_ipc_reset_join(nx_amp_close_t control);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_AMP_IPC_H */
