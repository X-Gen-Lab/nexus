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
 * \brief           Select an exact USART0 or USART1 finite TX DMA binding.
 * \param[in,out]   state: Zero-initialized storage with immutable DMA domains.
 * \param[in]       controller: One maintained readonly controller identity.
 * \param[in]       baud: Positive supported 8N1 baud.
 * \param[in]       profile: Independent IRQ receive representation.
 * \param[in,out]   storage: Caller-owned receive ring until successful stop.
 * \param[in]       capacity: Receive ring capacity in selected profile units.
 * \param[in]       priority: Unshifted UART and DMA publisher priority.
 * \return          Success or INVALID/CONTEXT/BUSY without request admission.
 * \note            USART0 uses DMA1 channel7; USART1 uses DMA0 channel6,
 *                  both selector4. Assembly owns the selected DMA clock and
 *                  pins. No RX DMA or continuous receive support is implied.
 */
nx_result_t
nx_gd32_uart_dma_initialize_at(nx_gd32_uart_dma_state_t* state,
                               const nx_gd32_uart_controller_t* controller,
                               uint32_t baud, nx_uart_rx_profile_t profile,
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
/**
 * \brief           Side-effect-free checks shared with this exact TX start.
 * \param[in]       context: Exact immutable face's selected provider state.
 * \param[in]       request: Live descriptor, not admitted or retained here.
 * \return          Same payload/domain/context/state/deadline rejection.
 * \note            Task-only; reads facts but performs no MMIO store or borrow.
 *                  RS485 binding must use this provider's matching validator.
 */
nx_result_t nx_gd32_uart_dma_tx_validate(void* context,
                                         const nx_uart_tx_request_t* request);

/** \brief Direct selected TX-DMA method; same request borrow contract. */
nx_result_t nx_gd32_uart_dma_submit(void* context,
                                    nx_uart_tx_request_t* request);
/** \brief Direct selected TX-DMA method; same request borrow contract. */
nx_result_t nx_gd32_uart_dma_start_admitted(void* context,
                                            nx_uart_tx_request_t* request);
/** \brief Direct selected TX-DMA method; same request borrow contract. */
nx_result_t nx_gd32_uart_dma_cancel(void* context,
                                    nx_uart_tx_request_t* request);
/** \brief Owner-only bounded service; failed drain retains quarantine. */
void nx_gd32_uart_dma_service(void* context);
/** \brief Close TX admission and prove both memory and wire drain. */
nx_result_t nx_gd32_uart_dma_stop(void* context);

#ifdef __cplusplus
}
#endif
#endif
