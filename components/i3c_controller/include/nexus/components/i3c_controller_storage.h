/**
 * \file            i3c_controller_storage.h
 * \brief           Caller storage and aggregate I3C hardware boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_I3C_CONTROLLER_STORAGE_H
#define NEXUS_COMPONENTS_I3C_CONTROLLER_STORAGE_H
#include "nexus/components/i3c_controller.h"
/** \brief Authored immutable target policy, not runtime factory discovery. */
typedef struct {
    uint64_t pid;
    uint8_t address;
} nx_i3c_target_definition_t;
/**
 * \brief           Caller-owned target record, zero-initialized once
 * \note            Core retains tentative and confirmed addresses on DAA error.
 *                  Epoch never resets or wraps, including after bus reset.
 */
typedef struct {
    uint64_t epoch;
    uint8_t address;
    uint8_t bcr;
    uint8_t dcr;
    bool assigned;
    bool tentative;
} nx_i3c_target_state_t;
/** \brief Value command; begin copies it synchronously and retains no pointer.
 */
typedef struct {
    nx_i3c_kind_t kind;
    uint32_t bytes;
    uint8_t address;
    uint8_t ccc;
    bool read;
    bool broadcast;
} nx_i3c_command_t;
/**
 * \brief           One bounded hardware observation and aggregate FIFO progress
 * \note            No callback retains spans. tx/rx counts may not exceed the
 *                  supplied capacities. DAA acknowledged confirms the preceding
 *                  assignment BEFORE any next identity in this observation.
 *                  A complete DAA with a tentative unacknowledged address is an
 *                  error, never implicit success. Event data are fully copied.
 */
typedef struct {
    size_t transmitted;
    size_t received;
    nx_result_t error;
    uint64_t pid;
    uint8_t bcr;
    uint8_t dcr;
    uint8_t ibi_address;
    uint8_t ibi_payload[4];
    uint8_t ibi_bytes;
    bool identity;
    bool assigned;
    bool complete;
    bool ibi;
    bool hot_join;
} nx_i3c_observation_t;
/**
 * \brief           Synchronous bounded hardware leaves; no request forwarding
 * \note            All port/clock callbacks are non-reentrant: no core calls,
 *                  application calls or mutation of retained core state.
 *                  claim atomically acquires an exclusive controller and its
 *                  IRQ/clock/pins. SUCCESS requires independently established
 *                  proof that EVERY target dynamic address is invalidated,
 *                  including targets still powered across MCU reset. The
 *                  caller establishes this baseline before construction;
 *                  claim never attempts an unproven reset. Rejection has no
 *                  effects. ready is a read-only admission check.
 *
 *                  begin and assign are infallible commands. pump reports
 *                  later hardware failures and services at most the supplied
 *                  batch capacities plus one IBI and one hot-join latch. No
 *                  command or span is retained. No application code is called.
 *                  withdraw requests abort. quiescent SUCCESS proves actual
 *                  wire/FIFO/IRQ quiescence and all port observers detached.
 *
 *                  reset additionally proves EVERY target dynamic address
 *                  invalidated. FIFO flush or controller reset alone cannot
 *                  report SUCCESS. BUSY/IO/UNSUPPORTED proof retains loans and
 *                  ownership. release follows successful final quiescence.
 *                  The port context and supplied storage outlive observers.
 */
typedef struct {
    nx_result_t (*claim)(void* context, const void* owner);
    void (*release)(void* context, const void* owner);
    nx_result_t (*ready)(void* context);
    void (*begin)(void* context, const nx_i3c_command_t* command);
    void (*pump)(void* context, const uint8_t* tx, size_t tx_capacity,
                 uint8_t* rx, size_t rx_capacity,
                 nx_i3c_observation_t* observation);
    void (*assign)(void* context, uint8_t address);
    void (*withdraw)(void* context);
    nx_result_t (*quiescent)(void* context);
    nx_result_t (*reset)(void* context);
} nx_i3c_port_ops_t;
/** \brief Exact caller capacities; separate static controller and target data.
 */
typedef struct {
    const nx_i3c_port_ops_t* ops;
    void* context;
    nx_clock_t clock;
    const nx_i3c_target_definition_t* definitions;
    nx_i3c_target_state_t* targets;
    size_t target_count;
    nx_i3c_event_t* events;
    size_t event_capacity;
    size_t batch_bytes;
} nx_i3c_controller_config_t;
struct nx_i3c_controller {
    nx_i3c_controller_config_t config;
    nx_request_slot_t slot;
    nx_i3c_request_t* active;
    size_t progress;
    size_t pending_target;
    size_t event_head;
    size_t event_count;
    nx_i3c_event_status_t event_status;
    nx_result_t result;
    bool initialized;
    bool closed;
    bool faulted;
    bool draining;
    bool joined;
};
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Bind unused zero storage and claim one controller
 * \note            Reject overlapping/unaligned/overflowing storage, duplicate
 *                  PIDs/addresses, reserved I3C addresses and nonzero targets.
 *                  The config value is copied and need not remain alive; its
 *                  referenced storage and contexts remain live and separate
 *                  from every request/payload/output. 48-bit PIDs are caller-
 *                  authored, never selected by a factory.
 *                  Positive event capacity and 1..8-byte service batches match
 *                  this bounded SDR/IBI reference. No legacy I2C, DMA, HDR,
 *                  target role, implicit discovery or hidden event worker.
 */
nx_result_t
nx_i3c_controller_construct(nx_i3c_controller_t* controller,
                            const nx_i3c_controller_config_t* config);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_I3C_CONTROLLER_STORAGE_H */
