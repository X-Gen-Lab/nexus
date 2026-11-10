/**
 * \file            uart_wire.h
 * \brief           Explicit USART wire personalities and LIN break requests
 * \author          Nexus Team
 */
#ifndef NEXUS_UART_WIRE_H
#define NEXUS_UART_WIRE_H

#include "nexus/io/uart.h"
#include "nexus/io/uart_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief A controller personality, independent of an application protocol. */
typedef enum {
    NX_UART_WIRE_ASYNCHRONOUS,
    NX_UART_WIRE_SYNCHRONOUS,
    NX_UART_WIRE_HALF_DUPLEX,
    NX_UART_WIRE_LIN,
    NX_UART_WIRE_IRDA,
    NX_UART_WIRE_SMARTCARD
} nx_uart_wire_mode_t;

/** \brief Immutable cold wire settings; unused fields must be zero.
 * \note Synchronous mode is master only; RX requires transmitting clock data.
 *       Half duplex and Smartcard require an authorized open-drain TX pad.
 *       LIN exposes raw break and byte events, without a LIN schedule/stack.
 *       IrDA uses SIR up to 115200 baud. The product owns optical turnaround,
 *       external transceiver settling and exclusion of concurrent reception.
 *       Smartcard uses one 8E1.5 character per TX request, with NACK/error
 *       reporting; card power, reset, ATR and retries belong to the product.
 */
typedef struct {
    nx_uart_wire_mode_t mode;
    nx_uart_flow_t flow;
    uint8_t prescaler;
    uint8_t guard_time;
    uint8_t lin_break_bits;
    bool clock_polarity;
    bool clock_phase;
    bool last_bit_clock;
    bool irda_low_power;
    bool smartcard_nack;
} nx_uart_wire_config_t;

/** \brief Prepare an unborrowed descriptor for one LIN 13-bit break.
 * \param[in,out] request: Initialized fresh or consumed SETTLED descriptor.
 * \param[in] deadline: Absolute nx_time_now_us deadline including wire tail.
 * \return Success or INVALID/STATE. No payload is retained by preparation.
 * \note Submit only to an explicitly selected LIN provider. The ordinary UART
 *       submit/cancel/service/stop contract applies, including retained borrow
 *       until real TC and withdrawal precede SETTLED. No LIN data is appended.
 */
nx_result_t nx_uart_lin_break_prepare(nx_uart_tx_request_t* request,
                                      nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif
