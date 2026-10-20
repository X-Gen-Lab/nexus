/**
 * \file            stm32h743_uart_dma_parts.h
 * \brief           Shared finite TX DMA engine with an explicit outer owner
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_UART_DMA_PARTS_H
#define NX_STM32H743_UART_DMA_PARTS_H
#include "nexus/io/dma.h"
#include "stm32h743_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Selected TX extension; no second UART, buffers or hidden worker. */
typedef struct {
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* tx;
    DMAMUX_Channel_TypeDef* mux;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    bool retained;
    bool preparing;
    bool launched;
    bool dma_complete;
    bool draining;
    bool cached;
    bool domain_lost;
} nx_stm32h743_uart_dma_transfer_t;
/**
 * \brief Temporary private call view; no persistent dispatch or lookup.
 * \note The optional joint-domain check isolates all selected lanes. Cache
 *       preparation may reenter controls, so every later MMIO checks it again.
 */
typedef struct {
    nx_stm32h743_uart_state_t* uart;
    nx_stm32h743_uart_dma_transfer_t* transmit;
    const void* owner;
    size_t owner_bytes;
    bool (*domain_check)(void* context);
    void* domain_context;
} nx_stm32h743_uart_dma_view_t;
/** \brief Reject cold foreign TX lane ownership without any hardware writes. */
nx_result_t
nx_stm32h743_uart_dma_prepare_parts(nx_stm32h743_uart_dma_view_t view);
/** \brief Configure only TX after the common UART was initialized once. */
nx_result_t
nx_stm32h743_uart_dma_initialize_parts(nx_stm32h743_uart_dma_view_t view);
/** \brief Validate and borrow a finite TX request and disjoint payload. */
nx_result_t nx_stm32h743_uart_dma_start_parts(nx_stm32h743_uart_dma_view_t view,
                                              nx_uart_tx_request_t* request,
                                              bool admitted);
/** \brief Cancel preserves storage through real DMA and wire drain. */
nx_result_t
nx_stm32h743_uart_dma_cancel_parts(nx_stm32h743_uart_dma_view_t view,
                                   nx_uart_tx_request_t* request);
/** \brief Task service settles only after the final hardware/IRQ access. */
void nx_stm32h743_uart_dma_service_parts(nx_stm32h743_uart_dma_view_t view);
/** \brief Observe the selected TX lane or shared UART wire completion. */
void nx_stm32h743_uart_dma_irq_parts(nx_stm32h743_uart_dma_view_t view);
/** \brief Stop only TX; the outer owner drains RX and then the common UART. */
nx_result_t nx_stm32h743_uart_dma_stop_parts(nx_stm32h743_uart_dma_view_t view);
#ifdef __cplusplus
}
#endif
#endif
