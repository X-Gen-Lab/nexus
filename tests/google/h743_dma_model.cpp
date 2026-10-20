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
uint32_t g_h743_dma_tc_flag;
uint32_t g_h743_dma_forbidden_reads;
uint32_t g_h743_dma_forbidden_writes;
uint32_t g_h743_dma_irq_disables[150];
bool g_h743_dma_watch_domain_mask;
uint32_t g_h743_dma_unmasked_domain_reads;
uint32_t g_h743_dma_domain_reads;
nx_result_t nx_h743_dma_model_irq_control(int irq, bool enabled,
                                          uint8_t priority) {
    if (!enabled)
        ++g_h743_dma_irq_disables[irq];
    return nx_h743_model_irq_control(irq, enabled, priority);
}
static bool in_range(volatile uint32_t* reg, const void* base, size_t bytes) {
    uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    uintptr_t begin = reinterpret_cast<uintptr_t>(base);
    return address >= begin && address - begin < bytes;
}
static bool engine(volatile uint32_t* reg) {
    return in_range(reg, &g_h743_dma_registers, sizeof(g_h743_dma_registers)) ||
           in_range(reg, &g_h743_dma_stream, sizeof(g_h743_dma_stream));
}
static bool mux(volatile uint32_t* reg) {
    return in_range(reg, &g_h743_dma_mux, sizeof(g_h743_dma_mux));
}
static void reset_domains(void) {
    uint32_t reset = g_h743_model.rcc.AHB1RSTR;
    if ((reset & RCC_AHB1RSTR_DMA1RST) != 0U) {
        std::memset(&g_h743_dma_registers, 0, sizeof(g_h743_dma_registers));
        std::memset(&g_h743_dma_stream, 0, sizeof(g_h743_dma_stream));
        g_h743_dma_stream.FCR = 0x21U;
    }
    if ((reset & (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U)
        std::memset(&g_h743_dma_mux, 0, sizeof(g_h743_dma_mux));
}
static bool unavailable(volatile uint32_t* reg) {
    return (engine(reg) || mux(reg)) &&
           ((g_h743_model.rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN) == 0U ||
            (g_h743_model.rcc.AHB1RSTR &
             (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U);
}
uint32_t nx_h743_dma_model_read(volatile uint32_t* reg) {
    if (g_h743_dma_watch_domain_mask && (reg == &g_h743_model.rcc.AHB1ENR ||
                                         reg == &g_h743_model.rcc.AHB1RSTR)) {
        ++g_h743_dma_domain_reads;
        if (!nx_arch_irq_is_masked())
            ++g_h743_dma_unmasked_domain_reads;
    }
    reset_domains();
    if (unavailable(reg))
        ++g_h743_dma_forbidden_reads;
    return nx_h743_model_read(reg);
}
void nx_h743_dma_model_domain(unsigned kind, bool lost) {
    if (kind == 0U) {
        if (lost)
            g_h743_model.rcc.AHB1ENR &= ~(uint32_t)RCC_AHB1ENR_DMA1EN;
        else
            g_h743_model.rcc.AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    } else {
        uint32_t bit = kind == 1U ? RCC_AHB1RSTR_DMA1RST : RCC_AHB1RSTR_DMA2RST;
        if (lost)
            g_h743_model.rcc.AHB1RSTR |= bit;
        else
            g_h743_model.rcc.AHB1RSTR &= ~bit;
    }
    reset_domains();
}
bool nx_h743_dma_model_dispatch(int irq, void (*handler)(void*), void* owner) {
    if (nx_arch_irq_is_masked() || !g_h743_model.irq_enabled[irq])
        return false;
    g_h743_model.irq_pending[irq] = true;
    g_h743_model.irq_active[irq] = true;
    g_h743_model.irq_pending[irq] = false;
    handler(owner);
    g_h743_model.irq_active[irq] = false;
    return true;
}
void (*g_h743_dma_after_write)(volatile uint32_t*, uint32_t);
nx_arch_result_t (*g_h743_dma_clean)(uintptr_t, size_t);
nx_arch_result_t (*g_h743_dma_invalidate)(uintptr_t, size_t);
void nx_h743_dma_model_reset(void) {
    nx_h743_model_reset();
    std::memset(&g_h743_dma_registers, 0, sizeof(g_h743_dma_registers));
    std::memset(&g_h743_dma_stream, 0, sizeof(g_h743_dma_stream));
    std::memset(&g_h743_dma_mux, 0, sizeof(g_h743_dma_mux));
    g_h743_dma_after_write = nullptr;
    g_h743_dma_clean = nullptr;
    g_h743_dma_invalidate = nullptr;
    g_h743_dma_tc_flag = DMA_LISR_TCIF0;
    g_h743_dma_stream.FCR = 0x21U;
    g_h743_dma_forbidden_reads = g_h743_dma_forbidden_writes = 0U;
    g_h743_dma_watch_domain_mask = false;
    g_h743_dma_unmasked_domain_reads = g_h743_dma_domain_reads = 0U;
    std::memset(g_h743_dma_irq_disables, 0, sizeof(g_h743_dma_irq_disables));
}
void nx_h743_dma_model_write(volatile uint32_t* reg, uint32_t value) {
    reset_domains();
    if (unavailable(reg)) {
        ++g_h743_dma_forbidden_writes;
        bool held_engine = engine(reg) && (g_h743_model.rcc.AHB1RSTR &
                                           RCC_AHB1RSTR_DMA1RST) != 0U;
        bool held_mux =
            mux(reg) && (g_h743_model.rcc.AHB1RSTR &
                         (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U;
        if (held_engine || held_mux)
            return;
    }
    bool interrupt_stream = reg == &g_h743_dma_stream.CR &&
                            (g_h743_dma_stream.CR & DMA_SxCR_EN) != 0U &&
                            (value & DMA_SxCR_EN) == 0U;
    nx_h743_model_write(reg, value);
    reset_domains();
    if (interrupt_stream && g_h743_model.ignore_write != reg)
        g_h743_dma_registers.LISR |= g_h743_dma_tc_flag;
    if (reg == &g_h743_dma_registers.LIFCR && g_h743_model.ignore_write != reg)
        g_h743_dma_registers.LISR &= ~value;
    if (g_h743_dma_after_write != nullptr)
        g_h743_dma_after_write(reg, value);
}
nx_arch_result_t nx_h743_dma_model_clean(uintptr_t address, size_t bytes) {
    return g_h743_dma_clean == nullptr ? NX_ARCH_OK
                                       : g_h743_dma_clean(address, bytes);
}
nx_arch_result_t nx_h743_dma_model_invalidate(uintptr_t address, size_t bytes) {
    return g_h743_dma_invalidate == nullptr
               ? NX_ARCH_OK
               : g_h743_dma_invalidate(address, bytes);
}
}
