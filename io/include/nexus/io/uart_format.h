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
/** \brief Immutable wire format; data_bits excludes the parity bit.
 * \note RTS/CTS needs independently reviewed Board routing and peer wiring.
 */
typedef struct {
    uint8_t data_bits;
    nx_uart_parity_t parity;
    nx_uart_stop_t stop;
    nx_uart_flow_t flow;
} nx_uart_format_t;
/** \brief Validate the common USART-v1 subset without hardware access.
 * \param[in] format: Explicit payload width, parity, stop and flow selection.
 * \return Success for 7E/O, 8N/E/O or 9N with one/two stop bits; INVALID for
 *         unknown values, UNSUPPORTED for a known incompatible combination.
 * \note Pure, bounded, task/IRQ safe; Board admission remains separate.
 */
nx_result_t nx_uart_format_validate(nx_uart_format_t format);
#ifdef __cplusplus
}
#endif
#endif
