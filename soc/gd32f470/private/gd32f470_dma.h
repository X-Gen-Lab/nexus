/**
 * \file            gd32f470_dma.h
 * \brief           Private fixed USART0 TX DMA state and independent IRQ RX
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_DMA_H
#define NEXUS_GD32F470_DMA_H

#include "gd32f470_provider.h"
#include "nexus/io/dma.h"

/**
 * \brief           Exact USART0 / DMA1 channel7 selector4 instance storage.
 * \note            Assembly owns DMA1 clock and reviewed UART pins. Regions
 *                  cover only DMA-visible memory; RX retains its separate IRQ
 *                  ring. No copy buffer, stream allocator or hidden task
 * exists.
 */
typedef struct {
    nx_gd32_uart_state_t uart;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_time_us_t drain_deadline;
    bool dma_complete;
    bool aborting;
} nx_gd32_uart_dma_state_t;

#ifdef __cplusplus
extern "C" {
#endif
extern const nx_uart_ops_t nx_gd32_uart_dma_ops;
/**
 * \brief           Initialize fixed DMA TX after Board wiring and DMA1 clock.
 * \param[in,out]   state: Static storage with immutable memory domains
 * supplied. \param[in]       baud: Reviewed 8N1 baud supported by USART0.
 * \param[in]       profile: Independently selected IRQ receive representation.
 * \param[in,out]   storage: RX ring kept alive until source and consumer drain.
 * \param[in]       capacity: Positive RX capacity in selected profile units.
 * \param[in]       priority: Unshifted priority for both UART and DMA
 * publishers. \return          Success or INVALID/BUSY/CONTEXT before any
 * request admission.
 */
nx_result_t nx_gd32_uart_dma_initialize(nx_gd32_uart_dma_state_t* state,
                                        uint32_t baud,
                                        nx_uart_rx_profile_t profile,
                                        void* storage, size_t capacity,
                                        unsigned priority);
/**
 * \brief           Observe selected DMA memory terminal facts without waiting.
 * \param[in,out]   state: Exact generated state kept through successful stop.
 * \note            DMA completion does not establish UART wire completion.
 */
void nx_gd32_uart_dma_irq(nx_gd32_uart_dma_state_t* state);
/**
 * \brief           Observe IRQ reception and independently latched UART TC.
 * \param[in,out]   state: Same static instance as the DMA vector.
 */
void nx_gd32_uart_dma_uart_irq(nx_gd32_uart_dma_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
