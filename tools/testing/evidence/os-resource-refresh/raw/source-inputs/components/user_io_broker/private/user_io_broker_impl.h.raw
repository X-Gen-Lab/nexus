/**
 * \file            user_io_broker_impl.h
 * \brief           Explicit kernel policy, pinned user loans and call slots
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_USER_IO_BROKER_IMPL_H
#define NEXUS_USER_IO_BROKER_IMPL_H
#include "nexus/components/user_io_broker.h"
#include "nexus/io/gpio.h"
/** \brief User RAM permission; no executable/MMIO capability is inferred. */
enum { NX_USER_MEMORY_READ = 1U, NX_USER_MEMORY_WRITE = 2U };
/** \brief Trusted nominal RAM window, all bytes must be in one such window. */
typedef struct {
    void* base;
    size_t bytes;
    uint32_t principal;
    uint32_t access;
} nx_user_io_region_t;
/** \brief Fixed scalar GPIO authorization and explicit private context extent.
 */
typedef struct {
    const nx_gpio_port_t* port;
    size_t context_bytes;
    uint32_t principal;
    uint32_t device;
    uint32_t read_mask;
    uint32_t write_mask;
} nx_user_io_gpio_grant_t;
/**
 * \brief           Trusted bounded identity and address-space pin operations
 * \note            identity authenticates the kernel's current source, never a
 *                  user-supplied scalar. Successful pin guarantees specified
 *                  read/write CPU access and mapping/process lifetime until
 *                  successful unpin. Failed pin has no retained mapping or
 *                  effects; failed unpin retains it. No callback may block,
 *                  recursively admit/release/execute or borrow its cookie
 *                  output. Kernel revocation may mark close during a callback.
 *                  Callbacks run outside Arch guards. Actual fault containment,
 *                  task identity and MPU/kernel enforcement remain integration
 *                  obligations, not assertions from a software range check.
 */
typedef struct {
    nx_result_t (*identity)(const void* context, uint32_t* principal);
    nx_result_t (*pin)(void* context, uint32_t principal, void* address,
                       size_t bytes, uint32_t access, uintptr_t* cookie);
    nx_result_t (*unpin)(void* context, uintptr_t cookie);
} nx_user_io_memory_ops_t;
/** \brief Immutable memory port with its known privileged control span. */
typedef struct {
    const nx_user_io_memory_ops_t* ops;
    void* context;
    size_t context_bytes;
} nx_user_io_memory_port_t;
/**
 * \brief           Trusted provider and observer final-access proof
 * \note            SUCCESS proves no peripheral/DMA/IRQ/deferred/user-copy
 *                  access remains. BUSY/error retains the loan. Context and
 *                  proof storage are privileged and kept alive until release.
 *                  A user message cannot supply or claim this proof.
 */
typedef struct {
    nx_result_t (*check)(const void* context);
    const void* context;
    size_t context_bytes;
} nx_user_io_drain_port_t;
/** \brief Static exact policy; readonly storage outlives constructed broker. */
typedef struct {
    const nx_user_io_region_t* regions;
    size_t region_count;
    const nx_user_io_gpio_grant_t* grants;
    size_t grant_count;
    size_t loan_capacity;
    nx_user_io_memory_port_t memory;
} nx_user_io_broker_config_t;
/** \brief Kernel-only stable token; zero initialize, never reset live epochs.
 */
typedef struct nx_user_io_loan {
    nx_user_io_broker_t* owner;
    struct nx_user_io_loan* next;
    void* address;
    size_t bytes;
    nx_user_io_drain_port_t drain;
    uintptr_t cookie;
    uint64_t epoch;
    uint32_t principal;
    uint32_t access;
    bool pinned;
    bool working;
} nx_user_io_loan_t;
/** \brief Kernel control identity; never published as an arbitrary-pointer ABI.
 */
typedef struct {
    nx_user_io_loan_t* slot;
    uint64_t epoch;
} nx_user_io_loan_ticket_t;
/** \brief Admission result preserves a successfully acquired pin on error. */
typedef struct {
    nx_result_t result;
    bool accepted;
    nx_user_io_loan_ticket_t ticket;
} nx_user_io_loan_result_t;
/** \brief Caller-owned provider state, no global registry or maximum pool. */
struct nx_user_io_broker {
    const nx_user_io_broker_config_t* config;
    nx_user_io_loan_t* head;
    size_t count;
    bool initialized;
    bool closing;
    bool working;
};
/** \brief Persistent kernel storage survives every failed memory cleanup. */
struct nx_user_io_call {
    nx_user_io_broker_t* owner;
    nx_user_io_loan_t input;
    nx_user_io_loan_t output;
    nx_user_io_message_t snapshot;
    nx_user_io_reply_t reply;
    nx_result_t result;
    nx_result_t cleanup_result;
    bool accepted;
    bool device_effect_possible;
    bool inflight;
};
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Cold privileged kernel construct; validate all known control spans.
 */
nx_result_t
nx_user_io_broker_construct(nx_user_io_broker_t* broker,
                            const nx_user_io_broker_config_t* config);
/**
 * \brief           Kernel-only admission of one exact user span
 * \note            principal is authenticated by the invoking trusted gateway.
 *                  Incoming masks clear; positive bounded bytes/access, full
 *                  nominal RAM permission,
 *                  no known control alias or conflicting writable peer span.
 *                  Successful pin retains slot and drain context on any later
 *                  revocation; accepted errors require continued release.
 */
nx_user_io_loan_result_t
nx_user_io_loan_acquire(nx_user_io_broker_t* broker, nx_user_io_loan_t* slot,
                        uint32_t principal, void* address, size_t bytes,
                        uint32_t access, nx_user_io_drain_port_t drain);
/** \brief Exact kernel ticket with clear masks, actual drain, then unpin. */
nx_result_t nx_user_io_loan_release(nx_user_io_broker_t* broker,
                                    nx_user_io_loan_ticket_t ticket);
/** \brief Close admission without revoking any retained mapping or IO access.
 */
nx_result_t nx_user_io_broker_close(nx_user_io_broker_t* broker);
/** \brief Closed zero-loan/no-executor barrier; no implicit shutdown. */
nx_result_t nx_user_io_broker_quiescent(const nx_user_io_broker_t* broker);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_USER_IO_BROKER_IMPL_H */
