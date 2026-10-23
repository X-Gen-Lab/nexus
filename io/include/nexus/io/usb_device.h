/**
 * \file            usb_device.h
 * \brief           Full-speed device packets and caller-owned endpoint loans
 * \author          Nexus Team
 */
#ifndef NEXUS_USB_DEVICE_H
#define NEXUS_USB_DEVICE_H
#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { NX_USB_DATA, NX_USB_STATUS } nx_usb_stage_t;
typedef struct {
    nx_request_t base;
    const uint8_t* in;
    uint8_t* out;
    size_t length;
    uint64_t control_epoch;
    nx_usb_stage_t stage;
} nx_usb_request_t;
typedef struct {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
    uint64_t epoch;
} nx_usb_setup_t;
typedef enum {
    NX_USB_EVENT_SETUP,
    NX_USB_EVENT_RESET,
    NX_USB_EVENT_SUSPEND,
    NX_USB_EVENT_RESUME,
    NX_USB_EVENT_LOSS
} nx_usb_event_kind_t;
typedef struct {
    nx_usb_setup_t setup;
    nx_time_us_t timestamp_us;
    uint64_t lost;
    nx_usb_event_kind_t kind;
} nx_usb_event_t;
typedef struct {
    nx_usb_setup_t setup;
    uint64_t epoch;
    uint64_t lost_events;
    bool started;
    bool closing;
    bool suspended;
    bool control_pending;
    uint8_t address;
} nx_usb_device_state_t;
/** \brief Shared readonly packet methods; context is a selected endpoint. */
typedef struct {
    nx_result_t (*submit)(void*, nx_usb_request_t*);
    nx_result_t (*cancel)(void*, nx_usb_request_t*);
    nx_result_t (*stall)(void*, bool);
} nx_usb_endpoint_ops_t;
struct nx_usb_endpoint_port {
    const nx_usb_endpoint_ops_t* ops;
    void* context;
};
/** \brief Controller identity remains separate from endpoint identities. */
typedef struct {
    nx_result_t (*service)(void*);
    nx_result_t (*read_events)(void*, nx_usb_event_t*, size_t, size_t*);
    nx_result_t (*state)(void*, nx_usb_device_state_t*);
    nx_result_t (*address)(void*, uint64_t, uint8_t);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
    nx_result_t (*stop)(void*);
} nx_usb_device_ops_t;
struct nx_usb_device_port {
    const nx_usb_device_ops_t* ops;
    void* context;
};
/**
 * \brief Prepare one IN packet without reading or borrowing its bytes.
 * \param[in,out] request: Initialized unborrowed request.
 * \param[in] bytes: Immutable packet bytes, NULL only for zero length.
 * \param[in] length: Packet length, no automatic terminating ZLP.
 * \param[in] stage: DATA or control STATUS; STATUS must be zero length.
 * \param[in] epoch: Current SETUP epoch for EP0, zero for nonzero endpoints.
 * \param[in] deadline: Absolute independent monotonic clock deadline.
 * \return Success, INVALID or STATE; rejection retains no references.
 */
nx_result_t nx_usb_in_prepare(nx_usb_request_t* request, const uint8_t* bytes,
                              size_t length, nx_usb_stage_t stage,
                              uint64_t epoch, nx_time_us_t deadline);
/**
 * \brief Prepare an OUT packet capacity without modifying its storage.
 * \param[in,out] request: Initialized unborrowed request.
 * \param[out] bytes: Borrowed until SETTLED only after accepted submission.
 * \param[in] capacity: At most endpoint MPS; short packets complete normally.
 * \param[in] stage: DATA or zero-capacity control STATUS.
 * \param[in] epoch: Current SETUP epoch for EP0, zero for nonzero endpoints.
 * \param[in] deadline: Absolute independent monotonic clock deadline.
 * \return Success, INVALID or STATE; rejection creates no borrow.
 */
nx_result_t nx_usb_out_prepare(nx_usb_request_t* request, uint8_t* bytes,
                               size_t capacity, nx_usb_stage_t stage,
                               uint64_t epoch, nx_time_us_t deadline);
/**
 * \brief Admit one packet, never a hidden queue or complete USB transaction.
 * \param[in] port: Stable endpoint face and private state.
 * \param[in,out] request: READY packet, buffers alive until acquire SETTLED.
 * \return Success means ACCEPTED; fourth-party buffers are not copied early.
 * \note One privileged task executor, IRQ masks clear. IN FIFO writes do not
 *       prove host ACK. OUT overrun drains all packet words without overflow.
 */
nx_result_t nx_usb_endpoint_submit(const nx_usb_endpoint_port_t* port,
                                   nx_usb_request_t* request);
/**
 * \brief Request endpoint disable/drain without revoking a buffer loan.
 * \param[in] port: Same serialized privileged task owner.
 * \param[in,out] request: Live request accepted by this exact endpoint.
 * \return Success requests drain; failures retain all prior accepted loans.
 * \note Control OUT0 has no hardware disable command: cancellation may require
 *       full device stop to settle. Missing proof becomes QUARANTINED.
 * Reset/SETUP invalidates epochs but releases only after actual source drain.
 */
nx_result_t nx_usb_endpoint_cancel(const nx_usb_endpoint_port_t* port,
                                   nx_usb_request_t* request);
/**
 * \brief Set/clear STALL only after the endpoint has no live transfer.
 * \param[in] port: Serialized owner, started and IRQ masks clear.
 * \param[in] stalled: Desired STALL; clearing bulk/interrupt resets DATA0.
 * \return Success after readback, BUSY retains a pending request.
 */
nx_result_t nx_usb_endpoint_stall(const nx_usb_endpoint_port_t* port,
                                  bool stalled);
/**
 * \brief Task-only bounded progress; deadline never depends on USB SOF.
 * \param[in] port: Serialized privileged owner with incoming IRQ masks clear.
 * \return Success requests progress; no clock/endpoint loan is released early.
 */
nx_result_t nx_usb_device_service(const nx_usb_device_port_t* port);
/**
 * \brief Consume bounded value events; output is never retained.
 * \param[in] port: Single task consumer and stable controller.
 * \param[out] events: Caller records disjoint from provider and live loans.
 * \param[in] capacity: Positive output count.
 * \param[out] count: Disjoint count output; EMPTY means zero.
 * \note Invalid/STATE rejection leaves both outputs unchanged.
 * \return Success, EMPTY or rejection; authoritative latest SETUP is in state.
 */
nx_result_t nx_usb_device_read_events(const nx_usb_device_port_t* port,
                                      nx_usb_event_t* events, size_t capacity,
                                      size_t* count);
/**
 * \brief Task-only authoritative snapshot, independent of event-ring space.
 * \param[in] port: Serialized privileged owner with IRQ masks clear.
 * \param[out] state: Disjoint caller snapshot, never retained.
 * \return Success copies state; alias/context rejection leaves output
 * unchanged.
 */
nx_result_t nx_usb_device_state(const nx_usb_device_port_t* port,
                                nx_usb_device_state_t* state);
/**
 * \brief Stage SET_ADDRESS for the matching zero-data standard SETUP.
 * \param[in] port: Serialized privileged controller owner.
 * \param[in] epoch: Current SETUP identity.
 * \param[in] address: 0..127, matching SETUP wValue.
 * \return Success stages only; address applies after real status-IN XFRC.
 */
nx_result_t nx_usb_device_address(const nx_usb_device_port_t* port,
                                  uint64_t epoch, uint8_t address);
/**
 * \brief Attach/detach a borrowed task/IRQ-compatible wake hint.
 * \param[in] port: Started serialized owner with IRQ masks clear.
 * \param[in] wake: Immutable target, or NULL to detach.
 * \param[in] syscall_ceiling: Additional numerical IRQ priority floor.
 * \return Success after actual priority policy checks, BUSY while publisher
 *         remains live; rejection retains no new sink.
 */
nx_result_t nx_usb_device_attach_wake(const nx_usb_device_port_t* port,
                                      const nx_irq_wake_t* wake,
                                      uint8_t syscall_ceiling);
/**
 * \brief Disconnect, disable, drain, detach IRQs, then release clock consumer.
 * \param[in] port: Serialized owner; external observers must also join.
 * \return Success ends provider access to endpoint/ring/wake storage; BUSY
 *         retains all storage and 48MHz ownership until actual progress.
 * \note PIO OUT0 may remain enabled: successful global stop proves disconnect,
 *       OUT NAK, no DMA, empty RX FIFO, IRQ withdrawal and publisher join
 * before final SETTLED. No forced reset revokes an unproved accepted borrow.
 */
nx_result_t nx_usb_device_stop(const nx_usb_device_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
