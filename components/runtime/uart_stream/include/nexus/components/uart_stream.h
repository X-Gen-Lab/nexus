/**
 * \file            uart_stream.h
 * \brief           Copying byte face over one existing UART request owner
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_UART_STREAM_H
#define NEXUS_COMPONENTS_UART_STREAM_H
#include "nexus/io/uart.h"
#include "nexus/network/byte_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Caller-owned inner request and copy storage identity; fields private.
 */
typedef struct {
    nx_uart_tx_request_t request;
    const nx_uart_port_t* uart;
    nx_clock_t clock;
    nx_byte_stream_t face;
    uint8_t* transmit;
    size_t capacity;
    nx_time_us_t last_time;
    nx_result_t failure;
    bool initialized;
    bool borrowed;
    bool closing;
    bool closed;
    bool active;
} nx_uart_stream_t;
/**
 * \brief Bind a copied stream to an already active exclusive byte-profile UART.
 * \param[in,out] stream: Zeroed fresh or successfully stopped caller metadata.
 * \param[in] uart: Stable face with submit/cancel/service/read_bytes methods.
 * \param[in] clock: Live same monotonic domain as the UART/Core deadlines.
 * \param[in,out] transmit: Disjoint stable private TX copy buffer.
 * \param[in] capacity: Positive exact buffer bytes; provider checks
 * reachability. \return SUCCESS, INVALID, UNSUPPORTED or STATE without UART
 * I/O/rebinding a live adapter. No allocation, lock, queue or controller
 * acquisition. \note One task owns stream, UART submit/service/RX and teardown.
 * Context, buffer, face, clock and provider outlive SETTLED and all upper
 * users. A clock-domain change requires joined stop and explicit rebind;
 * numeric rollback checks cannot detect a reset to an equal/larger clock value.
 */
nx_result_t nx_uart_stream_init(nx_uart_stream_t* stream,
                                const nx_uart_port_t* uart, nx_clock_t clock,
                                uint8_t* transmit, size_t capacity);
/**
 * \brief Query the already bound readonly immediate-copy byte face.
 * \param[in] stream: Initialized stable metadata.
 * \return Existing face or NULL for invalid/uninitialized metadata; no I/O.
 * \note Writes copy at most capacity and accept one inner UART request. Local
 *       admission is not wire/peer completion. New writes remain BUSY until
 *       acquire-observed SETTLED; later error closes stream admission.
 *       Reads use the existing UART RX consumer. EMPTY/zero bytes mean BUSY,
 *       never EOF. RX loss/error invalidates that call's copied prefix and
 *       latches failure; product restores module framing after physical
 * recovery.
 */
const nx_byte_stream_t* nx_uart_stream_face(const nx_uart_stream_t* stream);
/**
 * \brief Progress UART once and consume the real inner settlement predicate.
 * \param[in,out] stream: Same initialized serialized owner and live storage.
 * \return SUCCESS when locally idle/settled, BUSY while borrowed, or latched
 *         transfer/clock/loss failure. Failure never revokes a borrow.
 * \note Caller explicitly invokes this alongside AT/module progress. IRQ wake
 *       is a hint, not completion. No hidden execution or renewed deadline.
 */
nx_result_t nx_uart_stream_service(nx_uart_stream_t* stream);
/**
 * \brief Close byte admission and cancel/drain the exact original TX request.
 * \param[in,out] stream: Same metadata after all upper stream callers join.
 * \return SUCCESS only after acquire-observed SETTLED/no borrow; BUSY/error
 *         retains request/buffer/provider and requires further cleanup.
 * \note No timeout revokes a borrow. Stop still drains after a clock/data
 * fault; cancellation cannot extend the accepted deadline. Success reports
 * local withdrawal, not prior delivery, controller stop, RX/IRQ join or module
 *       synchronization. Product separately stops those external owners before
 *       reclaiming them. A quarantined request remains live through recovery.
 */
nx_result_t nx_uart_stream_stop(nx_uart_stream_t* stream);
#ifdef __cplusplus
}
#endif
#endif
