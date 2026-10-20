/**
 * \file            h743_spi_dma_factory_model.h
 * \brief           One shared bus for generated SPI and independent UART DMA
 * \author          Nexus Team
 */
#ifndef NX_H743_SPI_DMA_FACTORY_MODEL_H
#define NX_H743_SPI_DMA_FACTORY_MODEL_H
#include "h743_spi_dma_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern USART_TypeDef g_h743_spi_dma_factory_uart;
typedef struct {
    uint8_t sent[128];
    unsigned sent_count;
    uint8_t input[128];
    unsigned input_count;
    unsigned input_index;
    uint32_t initial[2];
    unsigned tx_ticks;
    unsigned rx_ticks;
    uint8_t shift_byte;
    bool shift_active;
    bool hold_commit;
    bool rx_pending;
    bool rx_stop_pending;
    bool hold_en[2];
    uintptr_t pending_address;
    uint8_t pending_value;
    unsigned memory_commits;
    unsigned model_errors;
    bool trim_after_pins;
    unsigned trim_events;
    bool reset_on_dma_enable;
    uint32_t reset_mask;
    unsigned reset_events;
} nx_h743_spi_dma_factory_model_t;
extern nx_h743_spi_dma_factory_model_t g_h743_spi_dma_factory_model;
void nx_h743_spi_dma_factory_reset(void);
void nx_h743_spi_dma_factory_step(void);
void nx_h743_spi_dma_factory_write(volatile uint32_t* reg, uint32_t value);
bool nx_h743_spi_dma_factory_level(int irq);
void nx_h743_spi_dma_factory_feed(const uint8_t* data, size_t length);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOB
#undef GPIOD
#undef GPIOE
#undef USART3
#undef SPI4
#undef DMA1
#undef DMA1_Stream0
#undef DMA1_Stream1
#undef DMA1_Stream2
#undef DMA1_Stream3
#undef DMAMUX1_Channel0
#undef DMAMUX1_Channel1
#undef DMAMUX1_Channel2
#undef DMAMUX1_Channel3
#define RCC                      (&g_h743_model.rcc)
#define FLASH                    (&g_h743_model.flash)
#define PWR                      (&g_h743_model.power)
#define TIM2                     (&g_h743_model.timer)
#define GPIOB                    (&g_h743_model.gpio_b)
#define GPIOD                    (&g_h743_model.gpio_d)
#define GPIOE                    (&g_h743_model.gpio_e)
#define USART3                   (&g_h743_spi_dma_factory_uart)
#define SPI4                     (&g_h743_spi_dma_model.spi)
#define DMA1                     (&g_h743_spi_dma_model.dma)
#define DMA1_Stream0             (&g_h743_spi_dma_model.streams[0])
#define DMA1_Stream1             (&g_h743_spi_dma_model.streams[1])
#define DMA1_Stream2             (&g_h743_spi_dma_model.streams[2])
#define DMA1_Stream3             (&g_h743_spi_dma_model.streams[3])
#define DMAMUX1_Channel0         (&g_h743_spi_dma_model.mux[0])
#define DMAMUX1_Channel1         (&g_h743_spi_dma_model.mux[1])
#define DMAMUX1_Channel2         (&g_h743_spi_dma_model.mux[2])
#define DMAMUX1_Channel3         (&g_h743_spi_dma_model.mux[3])
#define NX_H743_IRQ_ENABLED(irq) g_h743_model.irq_enabled[(irq)]
#undef NX_H743_WRITE
#define NX_H743_WRITE(reg, value) nx_h743_spi_dma_factory_write((reg), (value))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_h743_spi_dma_factory_write(&(reg), (uint32_t)(value))
#endif
