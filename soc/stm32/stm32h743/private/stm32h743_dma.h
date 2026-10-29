/**
 * \file            stm32h743_dma.h
 * \brief           Private finite USART3 DMA1 stream0 ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_DMA_H
#define NX_STM32H743_DMA_H
#include "nexus/io/dma.h"
#include "stm32h743_cold.h"
#include "stm32h743_uart_dma_parts.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Static DMA context; one selected stream, no allocator or worker.
 * \note Cache-enabled submissions borrow complete exclusive 32-byte lines.
 *       Configuration and cache state stay stable until successful stop.
 */
typedef struct {
    nx_stm32h743_uart_state_t uart;
    nx_stm32h743_uart_dma_transfer_t transmit;
} nx_stm32h743_uart_dma_state_t;
extern const nx_uart_ops_t nx_stm32h743_uart_dma_ops;
/**
 * \brief Cold-admit exact DMA1 stream0 and DMAMUX1 channel0/request46.
 * \param[in,out] state: Exclusive static provider with caller-explicit RX ring.
 * \param[in] clock_hz: Reviewed fixed APB1 clock selected by the assembly.
 * \param[in] poll_limit: Finite UART acknowledgement observation budget.
 * \return Success or error; retained flags preserve cleanup responsibility.
 * \note Privileged unmasked task context; no active request or foreign IRQ.
 */
nx_result_t
nx_stm32h743_uart_dma_initialize(nx_stm32h743_uart_dma_state_t* state,
                                 uint32_t clock_hz, uint32_t poll_limit);
/** \brief Observe only selected stream flags and withdraw DMA before wire TC.
 */
void nx_stm32h743_uart_dma_irq(nx_stm32h743_uart_dma_state_t* state);
/** \brief Observe TX wire completion and bounded independent RX reception. */
void nx_stm32h743_uart_dma_uart_irq(nx_stm32h743_uart_dma_state_t* state);
/** \brief Bind exact static DMA resources before this mode initializes.
 * \note Task-only; Platform owns pads/gates and callers retain immutable
 *       configuration, regions and storage through successful public stop.
 */
nx_result_t
nx_stm32h743_uart_dma_prepare(nx_stm32h743_uart_dma_state_t* port,
                              const nx_stm32h743_uart_config_t* config,
                              const nx_dma_memory_region_t* regions,
                              size_t region_count);

#ifdef __cplusplus
}
#endif
#endif
