/**
 * \file            gd32f470_uart_receive.h
 * \brief           Selected USART0 DMA receive targets and producer loans
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_UART_RECEIVE_H
#define NEXUS_GD32F470_UART_RECEIVE_H

#include "gd32f470_dma.h"

/** \brief Only selected receive modes own these exact hardware targets. */
typedef struct {
    nx_gd32_uart_dma_state_t transmit;
    nx_stream_t* stream;
    nx_stream_producer_t producer;
    nx_stream_fill_t fill[3];
    uint32_t boundary;
    unsigned expected_bank;
    size_t loans;
    bool continuous;
    bool pending;
    bool fault;
    bool stopping;
} nx_gd32_uart_receive_state_t;

/** \brief Build-time name for the same selected caller-owned storage. */
typedef nx_gd32_uart_receive_state_t nx_gd32_uart_duplex_state_t;

#ifdef __cplusplus
extern "C" {
#endif

extern const nx_uart_ops_t nx_gd32_uart_receive_ops;
extern const nx_uart_ops_t nx_gd32_uart_duplex_ops;

/**
 * \brief           Initialize exact USART0/DMA1 channel2 receive facts.
 * \param[in,out]   state: Fresh selected state; rx_start supplies exact caller
 *                  blocks retained through successful rx_stop.
 * \param[in]       baud: Maintained 8N1 baud rate.
 * \param[in]       priority: Unshifted priority for UART and DMA publishers.
 * \return          Success or INVALID/UNSUPPORTED/BUSY without block borrow.
 * \note            Task-only; optional TX DMA uses channel7/selector4. Board
 *                  wiring and clocks remain explicit assembly resources.
 */
nx_result_t nx_gd32_uart_receive_initialize(nx_gd32_uart_receive_state_t* state,
                                            uint32_t baud, unsigned priority);
/**
 * \brief           Observe bounded USART IDLE/error and independent TX facts.
 * \param[in,out]   state: Exact selected instance retained through stop.
 * \note            Never reads DATA while an RX DMA engine may still write.
 */
void nx_gd32_uart_receive_irq(nx_gd32_uart_receive_state_t* state);
/**
 * \brief           Observe only DMA1 channel2 receive completion or faults.
 * \param[in,out]   state: Same instance as the USART vector.
 * \note            Prove source detachment and actual engine disable before
 *                  publishing or aborting any producer loan.
 */
void nx_gd32_uart_receive_dma_irq(nx_gd32_uart_receive_state_t* state);

/**
 * \brief           Side-effect-free checks shared with this exact TX start.
 * \param[in]       context: Exact immutable face's selected provider state.
 * \param[in]       request: Live descriptor, not admitted or retained here.
 * \return          Same payload/domain/context/state/deadline rejection.
 * \note            Task-only; reads facts but performs no MMIO store or borrow.
 *                  RS485 binding must use this provider's matching validator.
 */
nx_result_t
nx_gd32_uart_receive_tx_validate(void* context,
                                 const nx_uart_tx_request_t* request);

/**
 * \brief           Initialize selected TX-DMA plus independent RX DMA.
 * \param[in,out]   state: Same selected caller-owned receive storage.
 * \param[in]       baud: Exact selected UART baud.
 * \param[in]       priority: Unshifted reviewed UART/DMA priority.
 * \return          Same cold route and ownership checks as receive initialize.
 * \note            Only this constructor retains TX-DMA initialization code.
 */
nx_result_t nx_gd32_uart_duplex_initialize(nx_gd32_uart_receive_state_t* state,
                                           uint32_t baud, unsigned priority);
/** \brief UART vector for selected TX-DMA and independent RX DMA state. */
void nx_gd32_uart_duplex_irq(nx_gd32_uart_receive_state_t* state);
/** \brief Matching pure validator for the selected duplex transport face. */
nx_result_t
nx_gd32_uart_duplex_tx_validate(void* context,
                                const nx_uart_tx_request_t* request);

#ifdef __cplusplus
}
#endif
#endif
