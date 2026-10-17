/**
 * \file            gd32f470_uart_word.h
 * \brief           Separate caller-owned nine-bit USART IRQ state.
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_UART_WORD_H
#define NEXUS_GD32F470_UART_WORD_H

#include "gd32f470_provider.h"
#include "nexus/io/uart_word.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Separate nine-bit state; no byte/DMA state is enlarged. */
typedef struct {
    const nx_gd32_uart_controller_t* controller;
    nx_uart_word_tx_request_t* active;
    uint16_t* storage;
    size_t capacity;
    size_t head;
    size_t count;
    size_t position;
    uint32_t baud;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    bool initialized;
    bool owned;
    bool closing;
    bool tc;
    bool loss;
} nx_gd32_uart_word_state_t;
extern const nx_uart_word_ops_t nx_gd32_uart_word_ops;
/**
 * \brief           Initialize cold nine-bit, no-parity, one-stop UART.
 *
 * \param[in,out]   state: Zeroed, exclusive controller and IRQ binding storage.
 *
 * \param[in]       controller: Exact immutable reviewed peripheral resource.
 *
 * \param[in]       baud: Representable baud with oversampling sixteen.
 *
 * \param[in]       storage: Aligned RX words disjoint from state and controls.
 *
 * \param[in]       capacity: Positive exact RX capacity in words.
 *
 * \param[in]       priority: Actual unshifted IRQ priority, 0..15.
 *
 * \return          SUCCESS; INVALID/CONTEXT/BUSY before effects; IO if clock,
 *                  register or IRQ readback fails after acquisition.
 *
 * \note            Sole unmasked startup task, with reviewed Board routing.
 *                  Failed initialization closes admission and attempts stop.
 *                  initialized/owned stay true while effects remain unproved;
 *                  preserve state, RX storage and resources and retry stop.
 *                  Only proved withdrawal clears both markers. The GD32
 *                  driver releases its clock only after observed gate-off.
 */
nx_result_t
nx_gd32_uart_word_initialize(nx_gd32_uart_word_state_t* state,
                             const nx_gd32_uart_controller_t* controller,
                             uint32_t baud, uint16_t* storage, size_t capacity,
                             unsigned priority);
/** \brief Bounded exact IRQ entry; at most one RX and one TX word, never
 * settle. \param[in,out] state: Live controller bound by generated vector
 * ownership.
 */
void nx_gd32_uart_word_irq(nx_gd32_uart_word_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
