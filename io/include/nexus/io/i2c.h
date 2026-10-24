/**
 * \file            i2c.h
 *
 * \brief           Address endpoints, repeated START and explicit STOP
 *                  settlement.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_H
#define NEXUS_I2C_H

#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Maintained finite-message admission bounds, checked before hardware
 * access.
 */
#define NX_I2C_MAX_MESSAGES      8U
#define NX_I2C_MAX_MESSAGE_BYTES 256U
/**
 * \brief           One message; a new message starts with repeated START,
 *                  final with STOP.
 */
typedef struct {
    uint8_t* data;
    size_t length;
    bool read;
} nx_i2c_message_t;

/**
 * \brief           Caller-owned finite message sequence
 * \note            Descriptors and payloads remain borrowed until SETTLED.
 *                  Writes stay immutable; reads receive into caller storage.
 *                  Cancellation, deadlines and quarantine never revoke loans.
 */
typedef struct {
    nx_request_t base;
    nx_i2c_message_t* messages;
    size_t count;
} nx_i2c_request_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*recover)(void* context);
    nx_result_t (*cancel)(void* context, nx_i2c_request_t* request);
    void (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*attach_wake)(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling);
} nx_i2c_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_i2c_port {
    const nx_i2c_ops_t* ops;
    void* context;
};

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*transaction)(void* context, nx_i2c_message_t* messages,
                               size_t count, nx_time_us_t deadline,
                               size_t* transferred);
    nx_result_t (*submit)(void* context, nx_i2c_request_t* request);
    nx_result_t (*start_admitted)(void* context, nx_i2c_request_t* request);
    bool (*on_port)(const void* context, const nx_i2c_port_t* port);
} nx_i2c_endpoint_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_i2c_endpoint {
    const nx_i2c_endpoint_ops_t* ops;
    void* context;
};

/**
 * \brief           Execute a polling 7-bit-address message transaction.
 *
 * \param[in]       endpoint: Fixed address/controller binding.
 *
 * \param[in,out]   messages: Message buffers; write data are not modified.
 *
 * \param[in]       count: One through NX_I2C_MAX_MESSAGES.
 *
 * \param[in]       deadline: Finite absolute provider-clock deadline.
 *
 * \param[out]      transferred: Provider byte progress across messages,
 *                  including error returns. TX may count bytes handed to the
 *                  data register before their wire/ACK outcome is known.
 *
 * \return          Success or INVALID/UNSUPPORTED/CONTEXT/STATE/BUSY/TIMEOUT/
 *                  NACK/ARBITRATION/IO. Each message has
 *                  1..NX_I2C_MAX_MESSAGE_BYTES bytes; any message may read or
 *                  write. Return releases buffers after STOP/abort drain.
 *                  Controller settlement does not establish bus recovery.
 *
 * \note            Task-only, one executor. No hidden scan or GPIO recovery.
 *                  Failed bus recovery is never reported as transfer success.
 */
nx_result_t nx_i2c_endpoint_transaction(const nx_i2c_endpoint_t* endpoint,
                                        nx_i2c_message_t* messages,
                                        size_t count, nx_time_us_t deadline,
                                        size_t* transferred);
/**
 * \brief           Recover an idle controller after a bus fault.
 *
 * \param[in,out]   port: Single executor, no live transaction/buffer borrow.
 *
 * \return          Success only when hardware bus idle is observed; otherwise
 *                  BUSY/IO/UNSUPPORTED. Does not replay a failed transaction.
 */
nx_result_t nx_i2c_port_recover(const nx_i2c_port_t* port);

/**
 * \brief           Prepare an unborrowed request without copying payload
 * \param[in,out]   request: Initialized READY or consumed SETTLED storage
 * \param[in,out]   messages: One through eight complete descriptors
 * \param[in]       count: Exact descriptor count
 * \param[in]       deadline: Absolute provider-clock deadline
 * \return          SUCCESS, INVALID, UNSUPPORTED or STATE; no new loan
 * \note            Payloads cannot overlap control storage, nor a read span
 *                  overlap an immutable write span. Limits match polling.
 */
nx_result_t nx_i2c_request_prepare(nx_i2c_request_t* request,
                                   nx_i2c_message_t* messages, size_t count,
                                   nx_time_us_t deadline);
/**
 * \brief           Validate finite descriptors and payload/control separation
 * \param[in]       request: Complete caller-owned request, not retained
 * \return          SUCCESS, INVALID or UNSUPPORTED without mutation
 * \note            Bounded cold check. Provider separately validates state,
 *                  context and DMA domains before acquiring any loan.
 */
nx_result_t nx_i2c_request_validate(const nx_i2c_request_t* request);
/**
 * \brief           Admit one finite asynchronous repeated-START transaction
 * \param[in]       endpoint: Initialized fixed address and single executor
 * \param[in,out]   request: READY descriptor and all referenced storage
 * \return          SUCCESS means ACCEPTED; rejection retains no references
 * \note            Selected modes declare DMA domains. DMA completion alone
 *                  never establishes wire/STOP drain or borrow release.
 *                  Any admitted abort requires explicit local recover before
 *                  reuse; recovery never replays or pulses Board GPIO lines.
 */
nx_result_t nx_i2c_endpoint_submit(const nx_i2c_endpoint_t* endpoint,
                                   nx_i2c_request_t* request);
/**
 * \brief           Start an already borrowed QUEUED request without readmission
 * \param[in]       endpoint: Fixed address on the adapter's exact controller
 * \param[in,out]   request: Adapter-admitted QUEUED storage
 * \return          SUCCESS hands execution to provider; BUSY retains QUEUED
 * \note            Other errors require adapter settlement; no polling
 * fallback.
 */
nx_result_t nx_i2c_endpoint_start_admitted(const nx_i2c_endpoint_t* endpoint,
                                           nx_i2c_request_t* request);
/**
 * \brief           Prove controller identity without hardware effects
 * \param[in]       endpoint: Immutable addressed child
 * \param[in]       port: Exact execution controller
 * \return          True only for matching provider state and compatible methods
 */
bool nx_i2c_endpoint_on_port(const nx_i2c_endpoint_t* endpoint,
                             const nx_i2c_port_t* port);
/**
 * \brief           Request cancellation without reclaiming any storage
 * \param[in]       port: Same single executor as submit/service
 * \param[in,out]   request: Exact active descriptor, never a stale raw pointer
 * \return          SUCCESS begins drain; STATE for another request
 */
nx_result_t nx_i2c_port_cancel(const nx_i2c_port_t* port,
                               nx_i2c_request_t* request);
/**
 * \brief           Advance deadline and STOP/DMA drain in bounded task context
 * \param[in]       port: Live single executor, serviced throughout stop
 * \note            Final publication follows source detach and local wire idle.
 *                  Drain failure quarantines all storage. This invokes no
 *                  payload completion callback and never automatically replays.
 */
void nx_i2c_port_service(const nx_i2c_port_t* port);
/**
 * \brief           Close admission and stop only after every borrower drains
 * \param[in]       port: Controller with external producers quiesced
 * \return          SUCCESS releases sources; BUSY requires continued service
 */
nx_result_t nx_i2c_port_stop(const nx_i2c_port_t* port);
/**
 * \brief           Attach one optional hint sink after actual IRQ policy checks
 * \param[in]       port: Initialized asynchronous controller
 * \param[in]       wake: Borrowed sink or NULL to detach after publisher exit
 * \param[in]       syscall_ceiling: Cannot weaken selected kernel policy
 * \return          SUCCESS or rejection before replacing any live binding
 * \note            Task-only. Sink remains alive until successful stop/detach.
 */
nx_result_t nx_i2c_port_attach_wake(const nx_i2c_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif

#endif
