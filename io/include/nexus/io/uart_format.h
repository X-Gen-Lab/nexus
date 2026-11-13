/**
 * \file            uart_format.h
 * \brief           Explicit payload, parity, stop and hardware-flow format.
 * \author          Nexus Team
 */
#ifndef NEXUS_UART_FORMAT_H
#define NEXUS_UART_FORMAT_H

#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_UART_PARITY_NONE,
    NX_UART_PARITY_EVEN,
    NX_UART_PARITY_ODD
} nx_uart_parity_t;
typedef enum { NX_UART_STOP_1 = 1, NX_UART_STOP_2 = 2 } nx_uart_stop_t;
typedef enum {
    NX_UART_FLOW_NONE,
    NX_UART_FLOW_RTS,
    NX_UART_FLOW_CTS,
    NX_UART_FLOW_RTS_CTS
} nx_uart_flow_t;
/** \brief Immutable wire format; 5..9 data_bits exclude the parity bit.
 * \note RTS/CTS needs independently reviewed Board routing and peer wiring.
 */
typedef struct {
    uint8_t data_bits;
    nx_uart_parity_t parity;
    nx_uart_stop_t stop;
    nx_uart_flow_t flow;
} nx_uart_format_t;
/** \brief Validate generic frame values without selecting hardware support.
 * \param[in] format: Explicit payload width, parity, stop and flow selection.
 * \return Success for 5..9 data bits, known parity/flow and one/two stop bits;
 *         INVALID for unknown values. Success does not authorize a controller.
 * \note Pure, bounded, task/IRQ safe. The selected provider separately checks
 *       its frame combinations, storage format and Board routing.
 */
nx_result_t nx_uart_format_validate(nx_uart_format_t format);
#ifdef __cplusplus
}
#endif
#endif
