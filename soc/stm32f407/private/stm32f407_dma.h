/**
 * \file            stm32f407_dma.h
 *
 * \brief           Private STM32F407 finite UART DMA storage and static routes
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_DMA_H
#define NEXUS_STM32F407_DMA_H

#include "nexus/io/dma.h"
#include "stm32f407_provider.h"

/**
 * \brief           Exact caller-owned DMA TX instance; no stream allocator.
 *
 * \note            USART1 uses DMA2 stream7 channel4. Clock and pin resources
 *                  remain owned by the platform assembly. Buffer domains
 *                  exclude CPU-only CCM. RX uses the existing independent IRQ
 *                  ring.
 */
typedef struct {
    nx_stm32_uart_state_t uart;
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* tx;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    IRQn_Type dma_irq;
    uint8_t stream;
    uint8_t channel;
    bool dma_complete;
    bool aborting;
} nx_stm32_uart_dma_state_t;

typedef struct nx_stm32_spi_dma_endpoint_state
    nx_stm32_spi_dma_endpoint_state_t;

/** \brief Exact SPI1 full-duplex DMA2 stream3 TX and stream0 RX storage. */
typedef struct {
    nx_stm32_spi_state_t spi;
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* tx;
    DMA_Stream_TypeDef* rx;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_spi_request_t* active;
    nx_stm32_spi_dma_endpoint_state_t* endpoint;
    const nx_irq_wake_t* wake;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    bool tx_complete;
    bool rx_complete;
    bool draining;
    bool stopping;
    bool initialized;
} nx_stm32_spi_dma_state_t;

/** \brief Per-device CS/mode/rate identity is independent of controller DMA. */
struct nx_stm32_spi_dma_endpoint_state {
    nx_stm32_spi_endpoint_state_t endpoint;
    nx_stm32_spi_dma_state_t* dma;
};

#ifdef __cplusplus
extern "C" {
#endif

extern const nx_uart_ops_t nx_stm32_uart_dma_ops;
extern const nx_spi_ops_t nx_stm32_spi_dma_ops;
extern const nx_spi_endpoint_ops_t nx_stm32_spi_dma_endpoint_ops;

/**
 * \brief           Initialize one static SPI1 full-duplex DMA controller.
 *
 * \param[in,out]   state: Exact pointers, clocks, domains and caller storage.
 *
 * \return          Success or INVALID/BUSY/UNSUPPORTED before buffer admission.
 *
 * \note            Assembly owns DMA2/SPI1 clocks and reviewed pin/CS
 *                  resources. Only nonoverlapping complete TX/RX buffers are
 *                  supported.
 */
nx_result_t nx_stm32_spi_dma_initialize(nx_stm32_spi_dma_state_t* state);

/**
 * \brief           Observe one selected finite DMA source without polling
 *                  waits.
 *
 * \param[in,out]   state: Live exact controller kept through successful stop.
 *
 * \param[in]       receive: True for DMA2 stream0 RX; false for stream3 TX.
 *
 * \note            DMA completion latches memory facts; task service proves
 *                  wire idle before withdrawing CS and publishing settlement.
 */
void nx_stm32_spi_dma_irq(nx_stm32_spi_dma_state_t* state, bool receive);

/**
 * \brief           Initialize a static USART1 TX DMA and IRQ RX provider.
 *
 * \param[in,out]   state: Fresh instance with reviewed pointers and fixed
 *                  route.
 *
 * \param[in]       clock_hz: Exact USART clock used by the ordinary UART init.
 *
 * \return          Success or INVALID/BUSY/UNSUPPORTED before TX admission.
 *
 * \note            Task-only; board assembly enables DMA2 and UART clocks
 *                  before calling. No DMA operation starts until a valid
 *                  request admits.
 */
nx_result_t nx_stm32_uart_dma_initialize(nx_stm32_uart_dma_state_t* state,
                                         uint32_t clock_hz);

/**
 * \brief           Observe bounded DMA terminal flags and detach memory source.
 *
 * \param[in,out]   state: Live fixed state kept through successful stop.
 *
 * \note            Generated DMA2 stream7 vector calls this exact instance; DMA
 *                  TC only enables independent UART TC drain observation.
 */
void nx_stm32_uart_dma_irq(nx_stm32_uart_dma_state_t* state);

/**
 * \brief           Observe IRQ RX and independent TX wire-idle completion.
 *
 * \param[in,out]   state: Same live state as the DMA vector.
 *
 * \note            Generated USART1 vector uses this mode-specific thunk.
 */
void nx_stm32_uart_dma_uart_irq(nx_stm32_uart_dma_state_t* state);

#ifdef __cplusplus
}
#endif

#endif
