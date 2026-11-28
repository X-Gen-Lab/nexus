/**
 * \file            instance.h
 * \brief           Exact STM32F100RB CEC link and its static frame storage.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_CEC_INSTANCE_H
#define NEXUS_STM32F1_CEC_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Maximum raw CEC frame length, including its header byte. */
#define NX_STM32F1_CEC_FRAME_BYTES 16U

/** \brief One default PB8 CEC endpoint for a serialized Thread owner. */
typedef struct {
    nx_stm32f1_pads_t pad;
    uint8_t tx[NX_STM32F1_CEC_FRAME_BYTES];
    uint8_t rx[NX_STM32F1_CEC_FRAME_BYTES];
    nx_result_t tx_result;
    nx_result_t rx_result;
    uint8_t tx_size;
    uint8_t tx_next;
    uint8_t rx_size;
    uint8_t address;
    uint8_t saved_priority;
    bool receiving;
    bool rx_ready;
    bool clock_owned;
    bool controls_owned;
    bool vector_owned;
    bool afio_added;
    bool live;
    bool stopping;
} nx_stm32f1_cec_t;

/** \brief Claim a reset endpoint with one assigned logical address, 0..14.
 * \note Zero-initialize the state once. PB8 requires an external pull-up.
 *       After a partial failure use close to release acquired receipts.
 */
nx_result_t nx_stm32f1_cec_prepare(nx_stm32f1_cec_t* state, uint8_t address,
                                   uint8_t priority);

/** \brief Start a raw frame using a copied payload of zero to fifteen bytes.
 * \note A zero-byte payload sends a directed header-only poll. No software
 *       retry is issued. Hardware arbitration may delay its initial send.
 */
nx_result_t nx_stm32f1_cec_send(nx_stm32f1_cec_t* state, uint8_t destination,
                                const void* payload, size_t bytes);

/** \brief Observe the most recent submit, including BUSY or NACK. */
nx_result_t nx_stm32f1_cec_transmit_status(const nx_stm32f1_cec_t* state);

/** \brief Copy one complete received frame including its raw header.
 * \note No ready frame returns EMPTY. Too small a buffer retains the frame.
 */
nx_result_t nx_stm32f1_cec_receive(nx_stm32f1_cec_t* state, void* frame,
                                   size_t capacity, size_t* bytes);

/** \brief Drain and disable the endpoint with a finite absolute deadline.
 * \note An accepted transmission may complete on the wire while draining.
 *       On timeout the peripheral, IRQ and pad receipts are retained for
 *       retry. Releasing a stuck CEC bus by forced reset is outside this API.
 */
nx_result_t nx_stm32f1_cec_close(nx_stm32f1_cec_t* state,
                                 nx_time_us_t deadline);

/** \brief Service the statically attached CEC vector. */
void nx_stm32f1_cec_irq(nx_stm32f1_cec_t* state);

#ifdef __cplusplus
}
#endif
#endif
