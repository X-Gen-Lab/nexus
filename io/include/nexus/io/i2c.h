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
/** \brief Explicit bounded profile used by the current selected providers.
 * \note These are implementation budgets, not I2C message grammar limits.
 */
#define NX_I2C_BOUNDED_MAX_MESSAGES      8U
#define NX_I2C_BOUNDED_MAX_MESSAGE_BYTES 256U

/** \brief Immutable selected admission limits, including aggregate progress.
 * \note All limits are positive. A provider checks them before retaining any
 *       request or touching hardware. They do not allocate payload storage.
 */
typedef struct {
    size_t max_messages;
    size_t max_message_bytes;
    size_t max_total_bytes;
} nx_i2c_limits_t;

/** \brief Shared readonly profile; current providers retain their 8x256 budget.
 */
extern const nx_i2c_limits_t nx_i2c_bounded_limits;
/**
 * \brief           One message; a new message starts with repeated START,
 *                  final with STOP.
 */
typedef struct {
    union {
        const uint8_t* tx;
        uint8_t* rx;
    } buffer;
    size_t length;
    bool read;
} nx_i2c_message_t;

/** \brief Describe immutable TX storage without borrowing or reading it.
 * \param[in] data: Write payload, kept immutable throughout the transaction.
 * \param[in] length: Payload bytes; full bounds are checked before admission.
 * \return A write descriptor, owned by the caller until explicit submission.
 * \note Pure task/IRQ construction, without allocation or hardware effects.
 */
static inline nx_i2c_message_t nx_i2c_message_write(const uint8_t* data,
                                                    size_t length) {
    nx_i2c_message_t message;
    message.buffer.tx = data;
    message.length = length;
    message.read = false;
    return message;
}

/** \brief Describe writable RX storage without borrowing or accessing it.
 * \param[in] data: Writable receive span, exclusively owned during transfer.
 * \param[in] length: Payload bytes; full bounds are checked before admission.
 * \return A read descriptor, owned by the caller until explicit submission.
 * \note Pure task/IRQ construction. A descriptor never converts const storage
 *       into a writable receive span; cancellation does not revoke its loan.
 */
static inline nx_i2c_message_t nx_i2c_message_read(uint8_t* data,
                                                   size_t length) {
    nx_i2c_message_t message;
    message.buffer.rx = data;
    message.length = length;
    message.read = true;
    return message;
}

/** \brief Inspect the selected span without granting writable access.
 * \param[in] message: Live descriptor, immutable during this bounded access.
 * \return Selected payload address; the helper does not validate its bounds.
 * \note Pure task/IRQ view, no reference retention or payload access. A
 *       provider writes only buffer.rx after checking the read direction.
 */
static inline const uint8_t*
nx_i2c_message_data(const nx_i2c_message_t* message) {
    return message->read ? message->buffer.rx : message->buffer.tx;
}

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
 * \brief           Execute a polling transaction at the endpoint's address.
 *
 * \param[in]       endpoint: Fixed address/controller binding.
 *
 * \param[in,out]   messages: Message buffers; write data are not modified.
 *
 * \param[in]       count: Positive descriptor count within provider limits.
 *
 * \param[in]       deadline: Finite absolute provider-clock deadline.
 *
 * \param[out]      transferred: Provider byte progress across messages,
 *                  including error returns. TX may count bytes handed to the
 *                  data register before their wire/ACK outcome is known.
 *
 * \return          Success or INVALID/UNSUPPORTED/CONTEXT/STATE/BUSY/TIMEOUT/
 *                  NACK/ARBITRATION/IO. Each message has a positive byte length
 *                  within selected limits; any message may read or write.
 *                  Return releases buffers after STOP/abort drain.
 *                  Controller settlement does not establish bus recovery.
 *
 * \note            Task-only, one executor. No hidden scan or GPIO recovery.
 *                  Descriptors, progress and payloads cannot overlap provider
 *                  control metadata. Progress cannot alias descriptors or any
 *                  payload; opposite-direction payloads must not overlap.
 *                  Context or invalid storage/shape rejection leaves progress
 *                  unchanged. Failed recovery never reports transfer success.
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
 * \param[in,out]   messages: Complete caller-owned descriptors
 * \param[in]       count: Exact descriptor count
 * \param[in]       deadline: Absolute provider-clock deadline
 * \return          SUCCESS, INVALID or STATE; no new loan
 * \note            Payloads cannot overlap control storage, nor a read span
 *                  overlap an immutable write span. Preparation checks grammar;
 *                  the selected provider separately enforces admission limits.
 *                  Task-only work is O(count squared) for alias checks.
 */
nx_result_t nx_i2c_request_prepare(nx_i2c_request_t* request,
                                   nx_i2c_message_t* messages, size_t count,
                                   nx_time_us_t deadline);
/**
 * \brief           Validate finite descriptors and payload/control separation
 * \param[in]       request: Complete caller-owned request, not retained
 * \return          SUCCESS or INVALID without mutation
 * \note            Task-only grammar check with O(count squared) alias work.
 *                  Provider validates its limits, state, context and DMA
 *                  domains before acquiring any loan.
 */
nx_result_t nx_i2c_request_validate(const nx_i2c_request_t* request);
/**
 * \brief           Validate grammar within explicit selected provider limits
 * \param[in]       request: Complete caller-owned request, not retained
 * \param[in]       limits: Stable positive count, message and aggregate bounds
 * \return          SUCCESS, INVALID or UNSUPPORTED without mutation or a loan
 * \note            Task-only preflight. Count is bounded before descriptors
 *                  are scanned; payload lengths are checked before access.
 *                  The provider still validates actual state and memory
 * domains.
 */
nx_result_t nx_i2c_request_validate_limits(const nx_i2c_request_t* request,
                                           const nx_i2c_limits_t* limits);
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
