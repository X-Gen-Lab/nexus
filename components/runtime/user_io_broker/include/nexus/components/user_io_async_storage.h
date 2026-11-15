/**
 * \file            user_io_async_storage.h
 * \brief           Trusted kernel assembly for exact copied UART user cells
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_USER_IO_ASYNC_STORAGE_H
#define NEXUS_COMPONENTS_USER_IO_ASYNC_STORAGE_H
#include "nexus/components/uart_owner.h"
#include "nexus/components/user_io_async.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Nominal copy permissions; actual access still requires trusted copy.
 */
enum nx_user_io_copy_access {
    NX_USER_IO_COPY_READ = 1U,
    NX_USER_IO_COPY_WRITE = 2U
};
/** \brief Nominal actual user RAM; trusted copy still enforces real access. */
typedef struct {
    uintptr_t begin;
    size_t bytes;
    uint32_t access;
} nx_user_io_copy_region_t;
/**
 * \brief           Bounded trusted kernel identity and fault-contained copies.
 *
 * \note            identity authenticates the current entry, never user input.
 *                  Copy operations guarantee complete checked CPU access and
 *                  mapping/process lifetime for the callback duration. Neither
 *                  success nor error retains a user address, kernel scratch
 *                  pointer, mapping or observer after return. Errors may have
 *                  partially copied bytes; the copied payload cell is wiped on
 * rejection, and reply failure is recoverable through exact slot/epoch. No
 * callback blocks, dispatches recursively, or substitutes a permission
 * observation for fault-safe copying. Identity/copy callbacks may close this
 * mediation. A new kernel/CMSE wrapper must actually establish these
 * preconditions; this component does not.
 *
 *                  claim is a trusted exclusive role authority for the actual
 *                  owner/controller binding. It rejects a second live role even
 *                  with a larger session, and rejects reused/non-increasing
 *                  nonzero sessions from persistent, nonwrapping history across
 *                  fresh storage, process/device reset and reconstruction.
 *                  Failed claim retains no reference/effect. Successful claim
 *                  permanently consumes that session and retains the exact role
 *                  until release. release SUCCESS proves outer
 * gateway/transport observers joined and detaches all authority references;
 * failure retains the original role unchanged.
 *                  drained SUCCESS permanently fences future outer gateway
 *                  entry and joins all gateway/transport observers, while the
 *                  exact role remains claimed. Failed drained preserves the
 *                  role and every cell unchanged. It must not modify bound
 *                  configuration, cell identities or requests. Claim, release
 *                  and drained are bounded and cannot recursively construct,
 *                  dispatch, close, reap or join. Context/history must remain
 * live; no source code implements this kernel authority, physical isolation,
 * storage partition or recovery policy. Alternate wrappers of one controller
 * are not distinct roles.
 */
typedef struct {
    nx_result_t (*claim)(void* context, uint32_t principal, uint32_t device,
                         uint64_t session, nx_bus_owner_t* owner,
                         const nx_user_io_async_t* role);
    nx_result_t (*release)(void* context, const nx_user_io_async_t* role,
                           uint64_t session);
    nx_result_t (*drained)(void* context, const nx_user_io_async_t* role,
                           uint64_t session);
    nx_result_t (*identity)(void* context, uint32_t* principal);
    nx_result_t (*copy_in)(void* context, uint32_t principal, void* destination,
                           const void* user_source, size_t bytes);
    nx_result_t (*copy_out)(void* context, uint32_t principal,
                            void* user_destination, const void* source,
                            size_t bytes);
} nx_user_io_copy_ops_t;
typedef struct {
    const nx_user_io_copy_ops_t* ops;
    void* context;
    size_t context_bytes;
} nx_user_io_copy_port_t;
/**
 * \brief           Immutable trusted one-principal/one-device/one-owner grant.
 *
 * \note            This is kernel configuration, never copied from users.
 *                  owner/executor/port must be the already constructed exact
 *                  BusOwner + UARTOwner bridge, with no other direct producers
 *                  or external request/payload observers. Completion callbacks
 *                  are not installed. All callback contexts remain privileged;
 *                  explicit extents cover their complete live controls. Guard,
 *                  clock and wake contexts may be the same trusted object.
 *                  The caller owns actual UART startup, hardware attribution,
 *                  executor scheduling, clock/deadline domain and final stop.
 */
typedef struct {
    uint32_t principal;
    uint32_t device;
    uint64_t session;
    nx_bus_owner_t* owner;
    nx_uart_owner_executor_t* executor;
    const nx_uart_port_t* port;
    size_t port_context_bytes;
    size_t guard_context_bytes;
    size_t clock_context_bytes;
    size_t wake_context_bytes;
    nx_user_io_copy_port_t copy;
    const nx_user_io_copy_region_t* regions;
    size_t region_count;
} nx_user_io_async_config_t;
/**
 * \brief           Exact privileged cell; initialize only data and capacity.
 *
 * \note            All other fields start zero and are mechanism-private after
 *                  construction. data is independent writable privileged RAM,
 *                  capacity is its exact positive byte budget, at most
 * UINT32_MAX. No user pointer is ever assigned here. Core identity epochs never
 * wrap and survive consume/close; never copy or reset a live cell. UARTOwner
 * borrows operation/payload until real provider final access, then user-result
 * retention lasts until successful reply copy plus joined CONSUME, or trusted
 *                  joined reap. Epoch history survives both paths.
 */
typedef struct {
    uint8_t* data;
    size_t capacity;
    nx_request_slot_t identity;
    nx_request_t request;
    nx_uart_owner_operation_t operation;
    nx_owner_ticket_t owner_ticket;
} nx_user_io_async_slot_t;
/** \brief Caller-owned trusted control; initialize once, never reset epochs. */
struct nx_user_io_async {
    const nx_user_io_async_config_t* config;
    nx_user_io_async_slot_t* slots;
    size_t count;
    bool initialized;
    bool constructing;
    bool closing;
    bool closing_call;
    bool working;
    bool releasing;
    bool joined;
    bool reaping;
    bool reaped;
};
/**
 * \brief           Cold construct one exact copied mediation over a real owner.
 *
 * \param[out]      mediation: Fresh zero privileged storage, initialized once.
 *
 * \param[in,out]   slots: Exact caller-owned cells/payloads, no maximum pool.
 *
 * \param[in]       count: Positive cell count representable in a scalar slot
 * ID.
 *
 * \param[in]       config: Immutable trusted principal/grant/copy/owner
 * binding.
 *
 * \return          SUCCESS or INVALID/STATE/CONTEXT/BUSY/EXHAUSTED, or trusted
 *                  claim error without new retained users or hardware effects;
 *                  every rejected output remains unchanged.
 *
 * \note            Privileged unmasked Thread with owner idle and no concurrent
 *                  users. Cold O(n squared) alias checks reject all known
 *                  mutable cell/payload/control overlaps and user-RAM aliases.
 *                  Admission checks fixed region extents and copies at most the
 *                  selected exact payload budget; no queue or worker is added.
 *                  Keep policy, cells, owner, provider and all contexts stable
 *                  through permanent close, consumption and successful join.
 *                  Pure cold checks precede a constructing reservation and the
 *                  trusted role claim; no fallible step follows successful
 * claim. Fresh objects require a larger externally claimed session; this object
 * cannot be reconstructed to reset its epochs.
 */
nx_result_t nx_user_io_async_construct(nx_user_io_async_t* mediation,
                                       nx_user_io_async_slot_t* slots,
                                       size_t count,
                                       const nx_user_io_async_config_t* config);
#ifdef __cplusplus
}
#endif
#endif
