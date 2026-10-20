/**
 * \file            stm32h743_uart_continuous_impl.h
 * \brief           Private selected RX view without growing RX-only storage
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_UART_CONTINUOUS_IMPL_H
#define NX_STM32H743_UART_CONTINUOUS_IMPL_H
#include "stm32h743_uart_duplex.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Temporary call view; it owns no memory, task or runtime registry. */
typedef struct {
    nx_stm32h743_uart_continuous_state_t* receive;
    nx_stm32h743_uart_dma_transfer_t* transmit;
} nx_stm32h743_uart_continuous_view_t;
/** \brief Shared selected cold admission, with an optional real TX extension.
 */
nx_result_t nx_stm32h743_uart_continuous_initialize_parts(
    nx_stm32h743_uart_continuous_view_t view, uint32_t clock_hz,
    uint32_t poll_limit);
/** \brief Progress only RX when the selected view has a DMA TX extension. */
void nx_stm32h743_uart_continuous_service_parts(
    nx_stm32h743_uart_continuous_view_t view);
/** \brief Borrow an explicitly disjoint stream after complete owner checks. */
nx_result_t nx_stm32h743_uart_continuous_start_parts(
    nx_stm32h743_uart_continuous_view_t view, nx_stream_t* stream);
/** \brief Withdraw RX while preserving TX and consumer responsibility. */
nx_result_t nx_stm32h743_uart_continuous_stop_rx_parts(
    nx_stm32h743_uart_continuous_view_t view);
/** \brief Stop the RX lane and already drained shared UART owner. */
nx_result_t nx_stm32h743_uart_continuous_stop_parts(
    nx_stm32h743_uart_continuous_view_t view);
/** \brief Validate all actual IRQ publishers and complete owner alias spans. */
nx_result_t nx_stm32h743_uart_continuous_attach_wake_parts(
    nx_stm32h743_uart_continuous_view_t view, const nx_irq_wake_t* wake,
    uint8_t ceiling);
/** \brief Validate TX separation from every live RX stream loan. */
nx_result_t nx_stm32h743_uart_continuous_validate_tx_parts(
    nx_stm32h743_uart_continuous_view_t view,
    const nx_uart_tx_request_t* request);
/** \brief Check the joint domain and permanently isolate all selected vectors.
 */
bool nx_stm32h743_uart_continuous_live_parts(
    nx_stm32h743_uart_continuous_view_t view);
/** \brief Observe the RX DMA boundary without maintaining cache lines. */
void nx_stm32h743_uart_continuous_dma_irq_parts(
    nx_stm32h743_uart_continuous_view_t view);
/** \brief Observe RX errors/IDLE and only the selected TX implementation. */
void nx_stm32h743_uart_continuous_uart_irq_parts(
    nx_stm32h743_uart_continuous_view_t view);
#ifdef __cplusplus
}
#endif
#endif
