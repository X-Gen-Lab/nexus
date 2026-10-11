/**
 * \file            uart.h
 *
 * \brief           IRQ TX requests and independently budgeted byte/event
 *                  reception.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_UART_H
#define NEXUS_UART_H

#include "nexus/core/request.h"
#include "nexus/io/stream.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_uart_port nx_uart_port_t;
typedef enum {
    NX_UART_RX_BYTES,
    NX_UART_RX_EVENTS,
    NX_UART_RX_BLOCKS
} nx_uart_rx_profile_t;
#define NX_UART_EVENT_PARITY  1u
#define NX_UART_EVENT_FRAMING 2u
#define NX_UART_EVENT_OVERRUN 4u
#define NX_UART_EVENT_NOISE   8u
#define NX_UART_EVENT_LOSS    16u
#define NX_UART_EVENT_NO_BYTE 32u
/**
 * \brief           Timestamp belongs to provider clock; precision is
 *                  hardware-specific.
 *
 * \note            NO_BYTE marks status-only IRQ facts: byte is invalid. LOSS
 *                  is an independent NO_BYTE boundary marker, ordered after
 *                  older buffered facts and before later reception. Its
 *                  timestamp is provider detection/report time, never a
 *                  reconstructed timestamp for missing bytes. Byte-profile
 *                  reads report aggregate loss and omit status-only facts.
 */
typedef struct {
    nx_time_us_t timestamp_us;
    uint32_t flags;
    uint8_t byte;
} nx_uart_rx_event_t;
/**
 * \brief           Caller-owned TX descriptor; initialize base before first
 *                  prepare.
 */
typedef struct {
    nx_request_t base;
    const uint8_t* data;
    size_t length;
} nx_uart_tx_request_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*submit)(void* context, nx_uart_tx_request_t* request);
    nx_result_t (*start_admitted)(void* context, nx_uart_tx_request_t* request);
    nx_result_t (*cancel)(void* context, nx_uart_tx_request_t* request);
    void (*service)(void* context);
    nx_result_t (*read_events)(void* context, nx_uart_rx_event_t* events,
                               size_t capacity, size_t* count);
    nx_result_t (*read_bytes)(void* context, uint8_t* bytes, size_t capacity,
                              size_t* count);
    nx_result_t (*stop)(void* context);
    nx_result_t (*attach_wake)(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling);
    nx_result_t (*rx_start)(void* context, nx_stream_t* stream);
    nx_result_t (*rx_stop)(void* context);
} nx_uart_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_uart_port {
    const nx_uart_ops_t* ops;
    void* context;
};

/**
 * \brief           Prepare an unborrowed IRQ TX descriptor without copying
 *                  data.
 *
 * \param[in,out]   request: Fresh initialized or consumed SETTLED request.
 *
 * \param[in]       data: Nonempty payload retained unchanged through SETTLED.
 *
 * \param[in]       length: Positive byte count.
 *
 * \param[in]       deadline: Absolute deadline in nx_time_now_us clock domain.
 *
 * \return          NX_SUCCESS or INVALID/STATE; failure retains no buffer.
 */
nx_result_t nx_uart_tx_prepare(nx_uart_tx_request_t* request,
                               const uint8_t* data, size_t length,
                               nx_time_us_t deadline);
/**
 * \brief           Submit one direct request to a single-executor UART.
 *
 * \param[in,out]   port: Initialized idle port owned by this execution context.
 *
 * \param[in,out]   request: READY descriptor; borrowed only on NX_SUCCESS.
 *
 * \return          NX_SUCCESS means ACCEPTED, including a later start failure.
 *                  BUSY/INVALID/CONTEXT/STATE/TIMEOUT means REJECTED with zero
 *                  borrow; an already expired direct request is not admitted.
 *
 * \note            Task-only, bounded admission. TX completion waits for true
 *                  TC (wire idle). IRQ only moves bytes/publishes hardware
 *                  facts; the owner calls service to observe deadlines and
 *                  settle. The first observed terminal fact latches its result.
 *                  Later service, deadline or cancellation does not overwrite
 *                  an already observed TC. IRQ/service latency bounds remain
 *                  provider and product qualification inputs.
 */
nx_result_t nx_uart_port_submit(const nx_uart_port_t* port,
                                nx_uart_tx_request_t* request);
/**
 * \brief           Start an adapter-admitted QUEUED request without
 *                  readmission.
 *
 * \param[in,out]   port: Single-executor initialized port.
 *
 * \param[in,out]   request: QUEUED descriptor already borrowed by adapter.
 *
 * \return          BUSY retains QUEUED; NX_SUCCESS transfers execution to port.
 *                  Other start errors must be settled by the adapter executor.
 */
nx_result_t nx_uart_port_start_admitted(const nx_uart_port_t* port,
                                        nx_uart_tx_request_t* request);
/**
 * \brief           Request cancellation; acceptance does not mean settlement.
 *
 * \param[in,out]   port: Same single execution owner as submit/service.
 *
 * \param[in,out]   request: Active live descriptor; cross-task callers use a
 *                  stable adapter slot and expected epoch instead of this
 *                  pointer.
 *
 * \return          NX_SUCCESS for pending/already settled cancellation; STATE
 *                  for a different request. Borrow continues until SETTLED.
 */
nx_result_t nx_uart_port_cancel(const nx_uart_port_t* port,
                                nx_uart_tx_request_t* request);
/**
 * \brief           Observe time/hardware completion and drain one active
 *                  request.
 *
 * \param[in,out]   port: Single task/loop execution owner.
 *
 * \note            Bounded, no wait/callback; call throughout stop/drain and at
 *                  the declared maximum service interval. Publication is final
 *                  request access. Hardware unable to drain retains
 *                  QUARANTINED.
 */
void nx_uart_port_service(const nx_uart_port_t* port);
/**
 * \brief           Read buffered RX events from the selected event profile.
 *
 * \param[in,out]   port: Initialized event-profile port; one consumer.
 *
 * \param[out]      events: Caller-owned destination; not retained.
 *
 * \param[in]       capacity: Destination event count.
 *
 * \param[out]      count: Events copied, including explicit loss/error flags.
 *
 * \return          NX_SUCCESS, EMPTY or INVALID/UNSUPPORTED. IRQ produces only
 *                  into configured storage; overflow never silently overwrites.
 */
nx_result_t nx_uart_port_read_events(const nx_uart_port_t* port,
                                     nx_uart_rx_event_t* events,
                                     size_t capacity, size_t* count);
/**
 * \brief           Read buffered bytes from the selected byte profile.
 *
 * \param[in,out]   port: Initialized byte-profile port; one consumer.
 *
 * \param[out]      bytes: Caller-owned destination; not retained.
 *
 * \param[in]       capacity: Destination capacity in bytes.
 *
 * \param[out]      count: Bytes copied; valid even when OVERFLOW is returned.
 *
 * \return          NX_SUCCESS, EMPTY, OVERFLOW or INVALID/UNSUPPORTED. Byte
 *                  profile reports aggregate loss without invented timestamps.
 */
nx_result_t nx_uart_port_read_bytes(const nx_uart_port_t* port, uint8_t* bytes,
                                    size_t capacity, size_t* count);
/**
 * \brief           Close admission and drain TX before releasing hardware.
 *
 * \param[in,out]   port: Owner; producers must already have quiesced.
 *
 * \return          NX_SUCCESS when stopped; BUSY while service must continue.
 *
 * \note            RX/IRQ and external consumers must quiesce before storage
 *                  reclamation. No automatic task/notification destruction.
 */
nx_result_t nx_uart_port_stop(const nx_uart_port_t* port);
/**
 * \brief           Attach or detach one explicit IRQ wake target.
 *
 * \param[in]       port: Initialized provider, controlled by one task executor.
 *
 * \param[in]       wake: Immutable live target or NULL to detach.
 *
 * \param[in]       syscall_ceiling: Optional additional unshifted kernel-safe
 *                  priority floor; cannot weaken generated policy. PRIMASK
 *                  kernel targets require zero. Ignored without kernel calls.
 *
 * \return          Success, CONTEXT outside task control, PERMISSION for unsafe
 *                  actual IRQ priority, or provider STATE/UNSUPPORTED.
 *
 * \note            Quiesce every previous publisher before reclaiming an old
 *                  target. IRQ events latch RX/completion facts before
 *                  notifying. Detachment removes future publications;
 *                  notification storage still requires joining external waiters
 *                  and callbacks.
 */
nx_result_t nx_uart_port_attach_wake(const nx_uart_port_t* port,
                                     const nx_irq_wake_t* wake,
                                     uint8_t syscall_ceiling);
/**
 * \brief           Start the selected RX block mode using exact caller storage.
 *
 * \param[in]       port: Block-capable provider, one task execution owner.
 *
 * \param[in,out]   stream: Initialized block stream retained until RX stops and
 *                  all acquired consumer blocks are released.
 *
 * \return          Success or INVALID/BUSY/UNSUPPORTED with zero failed
 *                  admission loan; selected hardware modes validate every DMA
 *                  block domain.
 *
 * \note            Provider boundaries and gaps are explicit capability facts.
 *                  The provider must never overwrite a borrowed consumer block.
 */
nx_result_t nx_uart_port_rx_start(const nx_uart_port_t* port,
                                  nx_stream_t* stream);
/**
 * \brief           Stop RX block production without revoking consumer loans.
 *
 * \param[in]       port: Same executor as RX start; storage stays live on BUSY.
 *
 * \return          Success proves memory sources detached; BUSY retains
 *                  producer loan, UNSUPPORTED for a byte/event-only provider.
 *
 * \note            Consumers separately drain READY blocks and release loans;
 *                  successful RX stop does not destroy stream or wake storage.
 */
nx_result_t nx_uart_port_rx_stop(const nx_uart_port_t* port);
#ifdef __cplusplus
}
#endif

#endif
