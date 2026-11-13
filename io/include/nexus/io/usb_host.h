/**
 * \file            usb_host.h
 * \brief           Logical USB host pipes and caller-owned packet loans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_USB_HOST_H
#define NEXUS_IO_USB_HOST_H

#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_USB_HOST_LOW_SPEED,
    NX_USB_HOST_FULL_SPEED,
    NX_USB_HOST_HIGH_SPEED
} nx_usb_host_speed_t;
typedef enum {
    NX_USB_HOST_CONTROL,
    NX_USB_HOST_ISOCHRONOUS,
    NX_USB_HOST_BULK,
    NX_USB_HOST_INTERRUPT
} nx_usb_host_transfer_t;
/** \brief Assembly-local logical pipe identity, independent of HCD channels. */
typedef uint8_t nx_usb_host_pipe_id_t;

typedef struct {
    uint16_t max_packet;
    uint8_t device_address;
    uint8_t endpoint_address;
    nx_usb_host_speed_t speed;
    nx_usb_host_transfer_t transfer;
} nx_usb_host_pipe_t;

/** \brief A single USB packet; no automatic retry, queue or class policy.
 * \note Query the selected pipe's storage requirements before submission.
 *       Capacity may exceed payload length, including for zero-length OUT.
 *       The provider may report OVERFLOW while safely draining a packet larger
 *       than requested length.
 */
typedef struct {
    nx_request_t base;
    uint8_t* bytes;
    size_t capacity;
    size_t length;
    bool setup;
    bool data1;
} nx_usb_host_request_t;

typedef struct {
    uint32_t frame;
    nx_usb_host_speed_t speed;
    bool started;
    bool closing;
    bool connected;
    bool port_enabled;
    bool suspended;
} nx_usb_host_state_t;

/** \brief Minimum complete packet storage and power-of-two address alignment.
 * \note Capacity includes all selected transport accesses, not just payload.
 *       Zero capacity permits NULL storage; alignment remains positive.
 *       These facts grant neither DMA reachability nor a buffer loan.
 */
typedef struct {
    size_t capacity;
    size_t alignment;
} nx_usb_host_storage_requirements_t;

typedef struct {
    nx_result_t (*open)(void*, nx_usb_host_pipe_id_t,
                        const nx_usb_host_pipe_t*);
    nx_result_t (*close)(void*, nx_usb_host_pipe_id_t);
    nx_result_t (*submit)(void*, nx_usb_host_pipe_id_t, nx_usb_host_request_t*);
    nx_result_t (*cancel)(void*, nx_usb_host_pipe_id_t, nx_usb_host_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*state)(void*, nx_usb_host_state_t*);
    nx_result_t (*reset)(void*, nx_time_us_t);
    nx_result_t (*suspend)(void*, bool, nx_time_us_t);
    nx_result_t (*stop)(void*);
    nx_result_t (*requirements)(const void*, nx_usb_host_pipe_id_t, size_t,
                                nx_usb_host_storage_requirements_t*);
} nx_usb_host_ops_t;
struct nx_usb_host_port {
    const nx_usb_host_ops_t* ops;
    void* context;
};

/**
 * \brief Prepare a host packet without accessing or borrowing its bytes.
 * \param[in,out] request: Initialized unborrowed storage.
 * \param[in,out] bytes: Packet buffer; NULL only for zero OUT packets.
 * \param[in] capacity: Actual writable IN storage or readable OUT extent.
 * \param[in] length: Requested packet bytes, at most the configured MPS.
 * \param[in] setup: Select the eight-byte SETUP data packet on an OUT
 * control pipe.
 * \param[in] data1: Explicit DATA toggle; false for single-transaction ISO.
 * \param[in] deadline: Absolute Core deadline, independent of USB SOF.
 * \return SUCCESS, INVALID or STATE; rejection retains no storage.
 */
nx_result_t nx_usb_host_prepare(nx_usb_host_request_t* request, uint8_t* bytes,
                                size_t capacity, size_t length, bool setup,
                                bool data1, nx_time_us_t deadline);

/**
 * \brief Configure an idle logical pipe for one endpoint target.
 * \param[in] port: Stable controller face, successful external power handover.
 * \param[in] pipe_id: Selected logical identity, separate from endpoint
 * address. \param[in] pipe: Readable target facts, copied before return.
 * \return SUCCESS or rejection; no transfer buffer is borrowed.
 * \note Serialized privileged unmasked task; no hub split transaction policy.
 */
nx_result_t nx_usb_host_open(const nx_usb_host_port_t* port,
                             nx_usb_host_pipe_id_t pipe_id,
                             const nx_usb_host_pipe_t* pipe);
/**
 * \brief Release an idle pipe after its IRQ and hardware references drain.
 * \param[in] port: Same serialized controller owner.
 * \param[in] pipe_id: Same logical identity used to open the pipe.
 * \return SUCCESS, BUSY or rejection; failure retains pipe responsibility.
 */
nx_result_t nx_usb_host_close(const nx_usb_host_port_t* port,
                              nx_usb_host_pipe_id_t pipe_id);
/**
 * \brief Query selected packet geometry without effects or reference retention.
 * \param[in] port: Stable started controller under its serialized task owner.
 * \param[in] pipe_id: Open logical pipe; direction and MPS belong to that pipe.
 * \param[in] length: Requested packet payload bytes, at most its MPS.
 * \param[out] requirements: Separate ordinary writable output, unchanged on
 * rejection. \return SUCCESS, INVALID, STATE, CONTEXT or UNSUPPORTED.
 * \note Pure bounded task work, no peripheral MMIO, hardware admission,
 *       allocation or storage borrow. Submit still validates current state,
 *       the actual extent, alignment, aliases and selected memory domain.
 *       An absent optional query is explicit UNSUPPORTED, never a default
 *       alignment or capacity assumption. Zero OUT length queries the real
 *       minimum extent needed by the selected transport.
 */
nx_result_t nx_usb_host_storage_requirements(
    const nx_usb_host_port_t* port, nx_usb_host_pipe_id_t pipe_id,
    size_t length, nx_usb_host_storage_requirements_t* requirements);
/**
 * \brief Admit one packet with no implicit retry or terminating ZLP.
 * \param[in] port: Stable started controller and privileged task executor.
 * \param[in] pipe_id: Configured logical pipe identity.
 * \param[in,out] request: READY request alive until acquire-observed SETTLED.
 * \return SUCCESS means ACCEPTED; rejection has no retained references.
 * \note IN capacity must cover MPS. DMA buffers also require aligned reviewed
 *       DMA-visible storage. PIO OUT returns BUSY without borrowing when the
 *       selected FIFO or request queue cannot accept the complete packet.
 *       Control/bulk/interrupt NAK/NYET settles as NACK for explicit retry.
 *       ISO admits one DATA0 opportunity; frame/error failure settles IO after
 *       actual withdrawal, without implicit retry or interval scheduling.
 *       Copied PIO OUT FIFO data may retain recovery responsibility until a
 *       controller stop that also joins its other pipes.
 */
nx_result_t nx_usb_host_submit(const nx_usb_host_port_t* port,
                               nx_usb_host_pipe_id_t pipe_id,
                               nx_usb_host_request_t* request);
/**
 * \brief Request pipe halt without revoking a packet loan.
 * \param[in] port: Same serialized controller owner.
 * \param[in] pipe_id: Original logical identity of the accepted request.
 * \param[in,out] request: Exact accepted incarnation, still alive.
 * \return SUCCESS requests drain; failed drain retains the same request.
 */
nx_result_t nx_usb_host_cancel(const nx_usb_host_port_t* port,
                               nx_usb_host_pipe_id_t pipe_id,
                               nx_usb_host_request_t* request);
/**
 * \brief Advance all selected pipes with bounded task work.
 * \param[in] port: Stable privileged unmasked task owner.
 * \return SUCCESS or BUSY; late hardware progress can settle quarantine.
 * \note A callback alone never proves buffer release. No worker is created.
 */
nx_result_t nx_usb_host_service(const nx_usb_host_port_t* port);
/**
 * \brief Read host connection and frame facts without retaining output.
 * \param[in] port: Same serialized controller owner.
 * \param[out] state: Disjoint caller storage, unchanged on rejection.
 * \return SUCCESS or rejection; no physical speed qualification is inferred.
 * \note After successful stop, cold facts are copied without gated MMIO.
 */
nx_result_t nx_usb_host_state(const nx_usb_host_port_t* port,
                              nx_usb_host_state_t* state);
/**
 * \brief Reset a connected idle port using the independent absolute deadline.
 * \param[in] port: Same controller owner; all pipe loans are settled.
 * \param[in] deadline: Finite absolute Core deadline.
 * \return SUCCESS or failure; reset never revokes an active request.
 * \note Admission requires strictly more than 10 ms plus one microsecond of
 *       clock-quantization guard; insufficient or expired budget returns
 *       TIMEOUT without port effects. An admitted reset keeps
 *       its minimum signal even if preemption crosses the deadline, then
 *       reports TIMEOUT after withdrawal. Failed withdrawal closes admission
 *       and retains the controller until stop can join the signal. An invalid
 *       or persistently frozen Core clock observes a terminal fault and parks
 *       with effects retained; it cannot authorize a short signal.
 */
nx_result_t nx_usb_host_reset(const nx_usb_host_port_t* port,
                              nx_time_us_t deadline);
/**
 * \brief Suspend or resume an idle host port without stopping Core time.
 * \param[in] port: Same serialized owner with settled pipe loans.
 * \param[in] suspended: Requested suspended state.
 * \param[in] deadline: Finite absolute Core deadline.
 * \return SUCCESS after readback or failure retaining controller ownership.
 * \note Resume requires strictly more than 20 ms plus one microsecond of
 *       clock-quantization guard before port effects. Its admitted minimum
 *       hold, late TIMEOUT, retained withdrawal
 *       and terminal clock-failure rules match reset. A late completed resume
 *       still publishes the actual resumed state; it does not retain a stale
 *       suspended snapshot solely because the deadline elapsed.
 */
nx_result_t nx_usb_host_suspend(const nx_usb_host_port_t* port, bool suspended,
                                nx_time_us_t deadline);
/**
 * \brief Halt, drain FIFOs/DMA, detach IRQ and release the host transport.
 * \param[in] port: Same owner, observers independently joined by caller.
 * \return SUCCESS ends transport access; failure retains all required state.
 * \note External VBUS power must remain authorized until successful stop.
 *       Power switch control belongs to the explicit role/power owner.
 */
nx_result_t nx_usb_host_stop(const nx_usb_host_port_t* port);

#ifdef __cplusplus
}
#endif
#endif
