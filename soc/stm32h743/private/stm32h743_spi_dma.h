/**
 * \file            stm32h743_spi_dma.h
 * \brief           Optional exact H743 SPI4 finite byte DMA ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32H743_SPI_DMA_H
#define NX_STM32H743_SPI_DMA_H
#include "nexus/io/dma.h"
#include "stm32h743_spi_wire.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_stm32h743_spi_dma_endpoint nx_stm32h743_spi_dma_endpoint_t;
/**
 * \brief Mode-only static owner; ordinary polling state never grows.
 * \note Trusted constructor holds GPIOE/SPI4/DMA1 clocks, pins and exact
 *       stream/vector leases until successful stop. No reset/gate adoption.
 *       State, faces, regions and endpoints outlive all retained requests.
 */
typedef struct {
    nx_stm32h743_spi_state_t wire;
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* rx;
    DMA_Stream_TypeDef* tx;
    DMAMUX_Channel_TypeDef* rx_mux;
    DMAMUX_Channel_TypeDef* tx_mux;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    const nx_spi_port_t* face;
    nx_spi_request_t* active;
    nx_stm32h743_spi_dma_endpoint_t* endpoint;
    nx_time_us_t quiet_until;
    nx_time_us_t drain_until;
    nx_result_t terminal;
    uint32_t rx_seen;
    uint32_t tx_seen;
    uint32_t wire_seen;
    uint32_t divisor;
    uint8_t stage;
    uint8_t irq_owned;
    uint8_t isolated;
    bool retained;
    bool initialized;
    bool cstart_attempted;
    bool tainted;
} nx_stm32h743_spi_dma_state_t;
/** \brief One caller-owned CS and byte-format binding, no hidden DMA cells. */
struct nx_stm32h743_spi_dma_endpoint {
    nx_stm32h743_spi_endpoint_state_t wire;
    nx_stm32h743_spi_dma_state_t* dma;
    const nx_spi_endpoint_t* face;
    const nx_gpio_port_t* cs_face;
};
extern const nx_spi_ops_t nx_stm32h743_spi_dma_ops;
extern const nx_spi_endpoint_ops_t nx_stm32h743_spi_dma_endpoint_ops;
/**
 * \brief Admit cold exact V/trim64/cache-off SPI4 and its two DMA1 streams.
 * \return SUCCESS or error; retained preserves partial cleanup responsibility.
 * \note Task-only. Static construction/lookup do not call this hardware API.
 */
nx_result_t
nx_stm32h743_spi_dma_initialize(nx_stm32h743_spi_dma_state_t* state);
/** \brief Observe only RX IRQ13 or TX IRQ14; never release caller loans. */
void nx_stm32h743_spi_dma_irq(nx_stm32h743_spi_dma_state_t* state,
                              bool receive);
/** \brief Mask SPI4 IRQ84 without clearing task-owned EOT/TXC/SUSP evidence. */
void nx_stm32h743_spi_dma_wire_irq(nx_stm32h743_spi_dma_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
