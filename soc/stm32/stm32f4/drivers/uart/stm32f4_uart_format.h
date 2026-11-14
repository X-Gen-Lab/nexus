/**
 * \file            stm32f4_uart_format.h
 * \brief           Private USART-v1 frame admission before hardware effects
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_STM32F4_UART_FORMAT_H
#define NEXUS_PRIVATE_STM32F4_UART_FORMAT_H

#include "nexus/io/uart_format.h"

/** \brief RM0090 word length includes parity; generic validity stays separate.
 * \note Pure cold check. Byte/word storage and physical flow routes require
 *       their own mode admission after this shared silicon constraint.
 */
static inline nx_result_t
nx_stm32_uart_frame_validate(nx_uart_format_t format) {
    nx_result_t result = nx_uart_format_validate(format);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (format.data_bits < 7U ||
        (format.data_bits == 7U && format.parity == NX_UART_PARITY_NONE) ||
        (format.data_bits == 9U && format.parity != NX_UART_PARITY_NONE)) {
        return NX_ERROR_UNSUPPORTED;
    }
    return NX_SUCCESS;
}

#endif
