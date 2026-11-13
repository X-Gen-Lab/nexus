/**
 * \file            uart_wire.h
 * \brief           Explicit USART wire personalities and LIN break requests
 * \author          Nexus Team
 */
#ifndef NEXUS_UART_WIRE_H
#define NEXUS_UART_WIRE_H

#include "nexus/io/uart.h"

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
