/**
 * \file            uart_word.h
 * \brief           Independent nine-bit face and caller-owned word requests.
 * \author          Nexus Team
 */
#ifndef NEXUS_UART_WORD_H
#define NEXUS_UART_WORD_H

#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#include "nexus/io/uart_format.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Each uint16_t contains 0..511; count excludes bytes and parity.
 * \note Request and unchanged payload are borrowed until acquire SETTLED.
 */
typedef struct {
    nx_request_t base;
    const uint16_t* data;
    size_t words;
} nx_uart_word_tx_request_t;
typedef struct {
    nx_result_t (*submit)(void* context, nx_uart_word_tx_request_t* request);
    nx_result_t (*start_admitted)(void* context,
                                  nx_uart_word_tx_request_t* request);
    nx_result_t (*cancel)(void* context, nx_uart_word_tx_request_t* request);
    void (*service)(void* context);
    nx_result_t (*read)(void* context, uint16_t* words, size_t capacity,
                        size_t* count);
    nx_result_t (*stop)(void* context);
} nx_uart_word_ops_t;
/** \brief Independent immutable face; default byte face/state do not grow.
 * \note Static identity; lookup does not allocate, discover or initialize.
 */
struct nx_uart_word_port {
    const nx_uart_word_ops_t* ops;
    void* context;
};
/** \brief Prepare an unborrowed nine-bit payload, without admitting it.
 * \param[in,out] request: Initialized READY or consumed SETTLED descriptor.
 * \param[in] data: Aligned immutable words, disjoint from the descriptor.
 * \param[in] words: Positive word count, representable as a byte extent.
 * \param[in] deadline: Absolute nx_time_now_us monotonic transfer deadline.
 * \return Success, INVALID or STATE; rejection retains no references/effects.
 * \note Task owner; payload values are checked before provider admission.
 */
nx_result_t nx_uart_word_tx_prepare(nx_uart_word_tx_request_t* request,
                                    const uint16_t* data, size_t words,
                                    nx_time_us_t deadline);
/** \brief Admit one READY request to the single unmasked task executor.
 * \param[in] port: Live initialized face, private state and IRQ binding.
 * \param[in,out] request: Caller-owned descriptor, borrowed on success.
 * \return Success means ACCEPTED; rejection has no borrow/hardware effects.
 * \note Counts use words; completion requires actual wire TC.
 */
nx_result_t nx_uart_word_port_submit(const nx_uart_word_port_t* port,
                                     nx_uart_word_tx_request_t* request);
/** \brief Transfer an adapter's QUEUED borrow without readmission.
 * \param[in] port: Live single-owner face.
 * \param[in,out] request: Existing adapter borrow.
 * \return Success transfers execution; BUSY leaves QUEUED and its borrow.
 */
nx_result_t
nx_uart_word_port_start_admitted(const nx_uart_word_port_t* port,
                                 nx_uart_word_tx_request_t* request);
/** \brief Request cancellation without revoking request/payload ownership.
 * \param[in] port: Same unmasked task executor as submit/service.
 * \param[in,out] request: Live request; cross-task control uses a stable slot.
 * \return Success for this request, STATE for a different live request.
 * \note Continue service through drain; failed drain stays QUARANTINED.
 */
nx_result_t nx_uart_word_port_cancel(const nx_uart_word_port_t* port,
                                     nx_uart_word_tx_request_t* request);
/** \brief Perform bounded owner-driven deadline, drain and settlement work.
 * \param[in] port: Single unmasked task owner, kept alive through stop.
 * \note IRQ only publishes facts; SETTLED is final provider access. No wait,
 *         hidden worker or callback; obey the declared maximum service gap.
 */
void nx_uart_word_port_service(const nx_uart_word_port_t* port);
/** \brief Copy words without retaining destination storage.
 * \param[in] port: Initialized face, one consumer under the task executor.
 * \param[out] words: Aligned destination, disjoint from provider/RX storage.
 * \param[in] capacity: Positive destination capacity in words.
 * \param[out] count: Number copied, including on OVERFLOW.
 * \return Success, EMPTY, OVERFLOW, INVALID, CONTEXT or STATE.
 * \note Loss does not invent a hardware timestamp or missing word count.
 */
nx_result_t nx_uart_word_port_read(const nx_uart_word_port_t* port,
                                   uint16_t* words, size_t capacity,
                                   size_t* count);
/** \brief Close admission and stop after active TX and IRQ sources drain.
 * \param[in] port: One task owner; external producers/observers quiesced.
 * \return Success after hardware detach; BUSY while service must continue.
 * \note Caller joins any in-flight IRQ before reclaiming provider storage.
 */
nx_result_t nx_uart_word_port_stop(const nx_uart_word_port_t* port);
#ifdef __cplusplus
}
#endif
#endif
