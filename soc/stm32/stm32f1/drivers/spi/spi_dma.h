/**
 * \file            spi_dma.h
 * \brief           Private STM32F103 finite SPI2 paired DMA owner
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_SPI_DMA_H
#define NEXUS_STM32F1_SPI_DMA_H
#include "drivers/spi/instance.h"
#include "nexus/io/dma.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_stm32f1_spi_config_t spi;
    DMA_TypeDef* dma;
    DMA_Channel_TypeDef* rx;
    DMA_Channel_TypeDef* tx;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    uint8_t priority;
} nx_stm32f1_spi_dma_config_t;

typedef struct {
    nx_stm32f1_spi_t spi;
    const nx_stm32f1_spi_dma_config_t* config;
    nx_spi_request_t* active;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    uint8_t rx_priority;
    uint8_t tx_priority;
    bool gate_added;
    bool lanes_owned;
    bool vectors_owned;
    bool ready;
    bool closing;
    bool draining;
} nx_stm32f1_spi_dma_t;

typedef struct {
    nx_stm32f1_spi_dma_t* port;
    GPIO_TypeDef* cs_gpio;
    uint32_t max_hz;
    uint8_t cs_pin;
    uint8_t mode;
    bool lsb_first;
} nx_stm32f1_spi_dma_endpoint_t;

nx_result_t nx_stm32f1_spi_dma_admit(const nx_stm32f1_spi_dma_t* state,
                                     const nx_stm32f1_spi_dma_config_t* config);
nx_result_t
nx_stm32f1_spi_dma_prepare(nx_stm32f1_spi_dma_t* state,
                           const nx_stm32f1_spi_dma_config_t* config);
nx_result_t nx_stm32f1_spi_dma_close(nx_stm32f1_spi_dma_t* state);
nx_result_t nx_stm32f1_spi_dma_endpoint_prepare(
    const nx_stm32f1_spi_dma_endpoint_t* endpoint);
void nx_stm32f1_spi_dma_irq(nx_stm32f1_spi_dma_t* state);

extern const nx_spi_ops_t nx_stm32f1_spi_dma_ops;
extern const nx_spi_endpoint_ops_t nx_stm32f1_spi_dma_endpoint_ops;
#ifdef __cplusplus
}
#endif
#endif
