/**
 * \file            h743_dma_model.h
 * \brief           External DMA, cache and IRQ hardware boundaries
 * \author          Nexus Team
 */
#ifndef NX_H743_DMA_MODEL_H
#define NX_H743_DMA_MODEL_H
#include "h743_model.h"
#include "nexus/arch/cache.h"
#ifdef __cplusplus
extern "C" {
#endif
extern DMA_TypeDef g_h743_dma_registers;
extern DMA_Stream_TypeDef g_h743_dma_stream;
extern DMAMUX_Channel_TypeDef g_h743_dma_mux;
extern uint32_t g_h743_dma_tc_flag;
extern void (*g_h743_dma_after_write)(volatile uint32_t*, uint32_t);
extern nx_arch_result_t (*g_h743_dma_clean)(uintptr_t, size_t);
extern nx_arch_result_t (*g_h743_dma_invalidate)(uintptr_t, size_t);
void nx_h743_dma_model_reset(void);
void nx_h743_dma_model_write(volatile uint32_t* reg, uint32_t value);
nx_arch_result_t nx_h743_dma_model_clean(uintptr_t address, size_t bytes);
nx_arch_result_t nx_h743_dma_model_invalidate(uintptr_t address, size_t bytes);
#define NX_H743_IRQ_ENABLED(irq) g_h743_model.irq_enabled[(irq)]
#undef NX_H743_WRITE
#define NX_H743_WRITE(reg, value) nx_h743_dma_model_write((reg), (value))
#define NX_H743_DMA_CLEAN(address, bytes)                                      \
    nx_h743_dma_model_clean((address), (bytes))
#define NX_H743_DMA_INVALIDATE(address, bytes)                                 \
    nx_h743_dma_model_invalidate((address), (bytes))
#ifdef __cplusplus
}
#endif
#endif
