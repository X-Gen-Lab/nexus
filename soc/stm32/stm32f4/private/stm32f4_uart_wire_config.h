/**
 * \file            stm32f4_uart_wire_config.h
 * \brief           Private STM32F4 USART cold wire configuration
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_UART_WIRE_CONFIG_H
#define NX_STM32F4_UART_WIRE_CONFIG_H

#include "nexus/io/uart_format.h"
#include "nexus/io/uart_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable F4 cold wire settings; unused fields must be zero.
 * \note Expert provider settings: prescaler and guard_time are the raw STM32F4
 *       USART GTPR PSC and GT encodings, not portable rates or durations.
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
} nx_stm32_uart_wire_config_t;

#ifdef __cplusplus
}
#endif
#endif
