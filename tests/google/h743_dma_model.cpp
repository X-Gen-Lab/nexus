/**
 * \file            h743_dma_model.cpp
 * \brief           Hardware W1C and cache-clean external seams
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_dma_model.h"
#include <cstring>
extern "C" {
DMA_TypeDef g_h743_dma_registers;
DMA_Stream_TypeDef g_h743_dma_stream;
DMAMUX_Channel_TypeDef g_h743_dma_mux;
void (*g_h743_dma_after_write)(volatile uint32_t*, uint32_t);
nx_arch_result_t (*g_h743_dma_clean)(uintptr_t, size_t);
void nx_h743_dma_model_reset(void) {
    nx_h743_model_reset();
    std::memset(&g_h743_dma_registers, 0, sizeof(g_h743_dma_registers));
    std::memset(&g_h743_dma_stream, 0, sizeof(g_h743_dma_stream));
    std::memset(&g_h743_dma_mux, 0, sizeof(g_h743_dma_mux));
    g_h743_dma_after_write = nullptr;
    g_h743_dma_clean = nullptr;
}
void nx_h743_dma_model_write(volatile uint32_t* reg, uint32_t value) {
    nx_h743_model_write(reg, value);
    if (reg == &g_h743_dma_registers.LIFCR && g_h743_model.ignore_write != reg)
        g_h743_dma_registers.LISR &= ~value;
    if (g_h743_dma_after_write != nullptr)
        g_h743_dma_after_write(reg, value);
}
nx_arch_result_t nx_h743_dma_model_clean(uintptr_t address, size_t bytes) {
    return g_h743_dma_clean == nullptr ? NX_ARCH_OK
                                       : g_h743_dma_clean(address, bytes);
}
}
