/**
 * \file            h743_spi_dma_model.h
 * \brief           External finite DMA bus, SPI frames and level IRQ ports
 * \author          Nexus Team
 */
#ifndef NX_H743_SPI_DMA_MODEL_H
#define NX_H743_SPI_DMA_MODEL_H
#include "h743_model.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    SPI_TypeDef spi;
    DMA_TypeDef dma;
    DMA_Stream_TypeDef streams[4];
    DMAMUX_Channel_TypeDef mux[4];
    uint8_t tx_fifo[8];
    uint8_t rx_fifo[8];
    unsigned tx_count;
    unsigned rx_count;
    unsigned frames;
    unsigned transaction_frames;
    uint32_t initial[2];
    unsigned frame_ticks;
    unsigned starts;
    unsigned suspend_commands;
    unsigned unsafe_commands;
    unsigned resets;
    unsigned ordering_errors;
    unsigned gated_accesses;
    unsigned reads;
    unsigned writes;
    unsigned maximum_irq_reads;
    unsigned maximum_irq_writes;
    unsigned dsbs;
    unsigned dmbs;
    unsigned memory_commits;
    unsigned eot_clears_in_irq;
    unsigned exception;
    unsigned faults;
    bool suspend_pending;
    bool hold_wire;
    bool hold_suspend;
    bool hold_rx_commit;
    bool rx_pending;
    bool rx_stop_pending;
    bool hold_en[2];
    bool hold_mask[3];
    bool hold_w1c[3];
    bool hold_disable[150];
    bool hold_pending_clear[150];
    bool pending_on_disable[150];
    bool ignore_start;
    bool complete_on_cr1_read;
    bool cpu_pending;
    uintptr_t pending_rx_address;
    uint8_t pending_rx_value;
    uintptr_t pending_cpu_address;
    uint8_t old_cpu_value;
    void (*enable_observer)(void);
    volatile uint32_t* ignore;
    uint32_t ignore_mask;
} nx_h743_spi_dma_model_t;
extern nx_h743_spi_dma_model_t g_h743_spi_dma_model;
void nx_h743_spi_dma_model_reset(void);
uint32_t nx_h743_spi_dma_model_read(volatile uint32_t* reg);
void nx_h743_spi_dma_model_write(volatile uint32_t* reg, uint32_t value);
uint8_t nx_h743_spi_dma_model_read8(volatile uint32_t* reg);
void nx_h743_spi_dma_model_step(void);
void nx_h743_spi_dma_model_error(bool receive);
bool nx_h743_spi_dma_model_level(int irq);
bool nx_h743_spi_dma_model_enabled(int irq);
bool nx_h743_spi_dma_model_pending(int irq);
bool nx_h743_spi_dma_model_active(int irq);
void nx_h743_spi_dma_model_enable(int irq);
void nx_h743_spi_dma_model_disable(int irq);
void nx_h743_spi_dma_model_clear(int irq);
void nx_h743_spi_dma_model_priority(int irq, uint8_t priority);
void nx_h743_spi_dma_model_dsb(void);
void nx_h743_spi_dma_model_dmb(void);
void nx_h743_spi_dma_model_cpu_store(uint8_t* target, uint8_t value);
uint32_t nx_h743_spi_dma_model_address(const void* address);
#ifdef __cplusplus
}
#endif
#undef NX_H743_READ
#undef NX_H743_WRITE
#undef NX_H743_SPI_READ8
#define NX_H743_READ(reg)                nx_h743_spi_dma_model_read(reg)
#define NX_H743_WRITE(reg, value)        nx_h743_spi_dma_model_write((reg), (value))
#define NX_H743_SPI_READ8(reg)           nx_h743_spi_dma_model_read8(reg)
#define NX_H743_SPI_DMA1                 (&g_h743_spi_dma_model.dma)
#define NX_H743_SPI_DMA_RX               (&g_h743_spi_dma_model.streams[2])
#define NX_H743_SPI_DMA_TX               (&g_h743_spi_dma_model.streams[3])
#define NX_H743_SPI_DMA_RX_MUX           (&g_h743_spi_dma_model.mux[2])
#define NX_H743_SPI_DMA_TX_MUX           (&g_h743_spi_dma_model.mux[3])
#define NX_H743_SPI4                     (&g_h743_spi_dma_model.spi)
#define NX_H743_SPI_DMA_ADDRESS(p)       nx_h743_spi_dma_model_address(p)
#define NX_H743_SPI_DMA_IRQ_ENABLED(irq) nx_h743_spi_dma_model_enabled(irq)
#define NX_H743_SPI_DMA_IRQ_PENDING(irq) nx_h743_spi_dma_model_pending(irq)
#define NX_H743_SPI_DMA_IRQ_ACTIVE(irq)  nx_h743_spi_dma_model_active(irq)
#define NX_H743_SPI_DMA_IRQ_ENABLE(irq)  nx_h743_spi_dma_model_enable(irq)
#define NX_H743_SPI_DMA_IRQ_DISABLE(irq) nx_h743_spi_dma_model_disable(irq)
#define NX_H743_SPI_DMA_IRQ_CLEAR(irq)   nx_h743_spi_dma_model_clear(irq)
#define NX_H743_SPI_DMA_IRQ_PRIORITY(irq, priority)                            \
    nx_h743_spi_dma_model_priority((irq), (priority))
#define nx_arch_exception_number() (g_h743_spi_dma_model.exception)
#define nx_arch_in_isr()           (g_h743_spi_dma_model.exception != 0U)
#define nx_arch_dsb()              nx_h743_spi_dma_model_dsb()
#define nx_arch_dmb()              nx_h743_spi_dma_model_dmb()
#endif
