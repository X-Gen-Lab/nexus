/**
 * \file            u575_uart_gpdma_model.h
 * \brief           Draft external U575 GPDMA MMIO leaves, not provider logic
 * \author          Nexus Team
 */
#ifndef NX_U575_UART_GPDMA_MODEL_H
#define NX_U575_UART_GPDMA_MODEL_H

#include "u575_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    DMA_TypeDef* engine;
    DMA_Channel_TypeDef* channel;
    USART_TypeDef* uart;
    volatile uint32_t* reject_write;
    uint32_t refuse_ccr_clear;
    uint32_t late_error_at_ack;
    uint32_t ccr_writes;
    uint32_t dmat_falls;
    uint32_t reset_requests;
    uint32_t illegal_reset_requests;
    uint32_t suspend_requests;
    uint32_t ignored_enable_clears;
    uint32_t configuration_writes_while_enabled;
    uint32_t unclocked_dma_accesses;
    uint32_t global_reset_requests;
    bool refuse_enable;
    bool refuse_suspend;
    bool refuse_reset;
    bool refuse_ack;
    bool hold_dma_irq_enabled;
    bool hold_dma_irq_pending;
    bool refuse_dma_irq_enable;
    bool refuse_dma_irq_priority;
    void (*before_write)(void* context, volatile uint32_t* reg, uint32_t value);
    void* write_context;
} nx_u575_uart_gpdma_model_t;

extern nx_u575_uart_gpdma_model_t g_u575_uart_gpdma;

/** \brief Reset only modeled hardware; caller has mapped real NS addresses. */
void nx_u575_uart_gpdma_model_reset(void);
uint32_t nx_u575_uart_gpdma_model_read(volatile uint32_t* reg);
void nx_u575_uart_gpdma_model_write(volatile uint32_t* reg, uint32_t value);
nx_result_t nx_u575_uart_gpdma_model_irq(int irq, bool enabled,
                                         uint8_t priority);

/** \brief Explicit external bus events; never inspect a provider/request. */
void nx_u575_uart_gpdma_advance_bytes(size_t bytes);
void nx_u575_uart_gpdma_advance_suspended(void);
void nx_u575_uart_gpdma_fault(uint32_t flags, bool idle);
void nx_u575_uart_gpdma_wire_idle(void);

#undef NX_U575_READ
#undef NX_U575_WRITE
#undef NX_U575_IRQ_CONTROL
#define NX_U575_READ(reg)         nx_u575_uart_gpdma_model_read(reg)
#define NX_U575_WRITE(reg, value) nx_u575_uart_gpdma_model_write((reg), (value))
#define NX_U575_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_u575_uart_gpdma_model_irq((irq), (enabled), (priority))

/* Real builds use the official LL enable/suspend/reset leaves. The model
 * substitutes only their equivalent one-register bus operation, never drain. */
#define NX_U575_GPDMA_ENABLE(facts)                                            \
    NX_U575_WRITE(&(facts)->channel->CCR,                                      \
                  NX_U575_READ(&(facts)->channel->CCR) | DMA_CCR_EN)
#define NX_U575_GPDMA_SUSPEND(facts)                                           \
    NX_U575_WRITE(&(facts)->channel->CCR,                                      \
                  NX_U575_READ(&(facts)->channel->CCR) | DMA_CCR_SUSP)
#define NX_U575_GPDMA_RESET(facts)                                             \
    NX_U575_WRITE(&(facts)->channel->CCR,                                      \
                  NX_U575_READ(&(facts)->channel->CCR) | DMA_CCR_RESET)
#define NX_U575_USART_DMA_ENABLE(uart)                                         \
    NX_U575_WRITE(&(uart)->CR3, NX_U575_READ(&(uart)->CR3) | USART_CR3_DMAT)
#define NX_U575_USART_DMA_DISABLE(uart)                                        \
    NX_U575_WRITE(&(uart)->CR3,                                                \
                  NX_U575_READ(&(uart)->CR3) & ~(uint32_t)USART_CR3_DMAT)
#define NX_U575_IRQ_ACTIVE(irq) g_u575_model.irq_active[(irq)]

#ifdef __cplusplus
}
#endif
#endif
