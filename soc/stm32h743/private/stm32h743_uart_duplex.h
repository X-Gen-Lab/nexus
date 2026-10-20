/**
 * \file            stm32h743_uart_duplex.h
 * \brief           Selected USART3 continuous RX and finite TX DMA owner
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_UART_DUPLEX_H
#define NX_STM32H743_UART_DUPLEX_H
#include "stm32h743_uart_continuous.h"
#include "stm32h743_uart_dma_parts.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief One UART owner with two independently drained DMA lanes.
 * \note Only dma-duplex-continuous selects this layout. TX and all RX blocks
 *       occupy disjoint AXI SRAM spans and, with cache on, exclusive full
 *       32-byte lines. No request, stream, slot or wake aliases this object.
 *       Consumers retain acquired blocks until release. Failed stop preserves
 *       all remaining loans and prevents platform/cache/timebase withdrawal.
 */
typedef struct {
    nx_stm32h743_uart_continuous_state_t receive;
    nx_stm32h743_uart_dma_transfer_t transmit;
} nx_stm32h743_uart_duplex_state_t;
extern const nx_uart_ops_t nx_stm32h743_uart_duplex_ops;
/**
 * \brief Cold-admit V USART3 RX1/request45 and TX0/request46 as one owner.
 * \param[in,out] state: Exclusive static owner and explicit AXI region.
 * \param[in] clock_hz: Exact fixed HSI64 frequency, no PLL or running handoff.
 * \param[in] poll_limit: Bounded USART acknowledgement observation budget.
 * \return SUCCESS or error; retained flags preserve cleanup responsibility.
 * \note Privileged unmasked task, baremetal, priority5/calls_os=false. TX
 *       borrows 1..65535 bytes without cache or complete lines up to65504 with
 *       cache, through observed SETTLED. DMA completion is not wire completion.
 */
nx_result_t
nx_stm32h743_uart_duplex_initialize(nx_stm32h743_uart_duplex_state_t* state,
                                    uint32_t clock_hz, uint32_t poll_limit);
/** \brief Observe the selected finite TX DMA lane, never consume RDR. */
void nx_stm32h743_uart_duplex_dma_irq(nx_stm32h743_uart_duplex_state_t* state);
/** \brief Observe RX IDLE/errors and independent TX wire completion. */
void nx_stm32h743_uart_duplex_uart_irq(nx_stm32h743_uart_duplex_state_t* state);
/** \brief Observe the selected RX bank boundary without cache maintenance. */
void nx_stm32h743_uart_duplex_receive_irq(
    nx_stm32h743_uart_duplex_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
