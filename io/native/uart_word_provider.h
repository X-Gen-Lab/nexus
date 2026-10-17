/**
 * \file            uart_word_provider.h
 * \brief           Independent caller-owned Native nine-bit UART state.
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_UART_WORD_PROVIDER_H
#define NEXUS_NATIVE_UART_WORD_PROVIDER_H

#include "nexus/io/uart_word.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Explicit model storage; no pool or addition to default UART state. */
typedef struct {
    nx_uart_word_tx_request_t* active;
    uint16_t* storage;
    uint16_t* log;
    size_t capacity;
    size_t log_capacity;
    size_t head;
    size_t count;
    size_t position;
    size_t logged;
    nx_result_t terminal;
    bool initialized;
    bool closing;
    bool tc;
    bool loss;
    bool hold_drain;
} nx_native_uart_word_state_t;
extern const nx_uart_word_ops_t nx_native_uart_word_ops;
/** \brief Cold initialization validates disjoint caller-owned word buffers.
 * \param[in,out] state: Zero-initialized, quiescent independent model state.
 * \param[in] storage: Aligned RX word storage, retained through stop.
 * \param[in] capacity: Positive RX capacity in words.
 * \param[in] log: Optional aligned TX observation buffer, retained through
 * stop. \param[in] log_capacity: Exact TX log capacity; zero iff log is NULL.
 * \return Success or INVALID/BUSY with no effects on failure.
 */
nx_result_t nx_native_uart_word_initialize(nx_native_uart_word_state_t* state,
                                           uint16_t* storage, size_t capacity,
                                           uint16_t* log, size_t log_capacity);
/** \brief Inject one bounded TX-empty or distinct wire-TC IRQ observation.
 * \param[in,out] state: Initialized model, independently serialized IRQ source.
 */
void nx_native_uart_word_irq(nx_native_uart_word_state_t* state);
/** \brief Inject a valid received word or bounded loss for an invalid word.
 * \param[in,out] state: Initialized model, one serialized producer.
 * \param[in] word: Nine-bit received word; out-of-range values record loss.
 */
void nx_native_uart_word_receive(nx_native_uart_word_state_t* state,
                                 uint16_t word);
#ifdef __cplusplus
}
#endif
#endif
