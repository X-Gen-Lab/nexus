/**
 * \file            h743_uart_continuous_model.cpp
 * \brief           Independent double-target receive memory bus
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_uart_continuous_model.h"
#include <cstring>
extern "C" {
nx_h743_uart_continuous_model_t g_h743_uart_continuous_model;
}
static bool inside(volatile uint32_t* reg, const void* base, size_t bytes) {
    uintptr_t p = reinterpret_cast<uintptr_t>(reg);
    uintptr_t begin = reinterpret_cast<uintptr_t>(base);
    return p >= begin && p - begin < bytes;
}
static bool unavailable(volatile uint32_t* reg) {
    auto& m = g_h743_uart_continuous_model;
    bool dma = inside(reg, &m.dma, sizeof(m.dma)) ||
               inside(reg, &m.rx, sizeof(m.rx)) ||
               inside(reg, &m.mux, sizeof(m.mux));
    bool uart = inside(reg, &m.uart, sizeof(m.uart));
    return (dma && ((g_h743_model.rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN) == 0U ||
                    (g_h743_model.rcc.AHB1RSTR &
                     (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U)) ||
           (uart && (g_h743_model.rcc.APB1LENR & RCC_APB1LENR_USART3EN) == 0U);
}
static bool range(uintptr_t address, size_t bytes) {
    return address >= 0x24000000U && address < 0x24080000U &&
           bytes <= 0x24080000U - address;
}
static void commit(void) {
    auto& m = g_h743_uart_continuous_model;
    if (!m.pending || m.hold_commit)
        return;
    if (!range(m.pending_address, 1U)) {
        ++m.target_errors;
        return;
    }
    if ((g_h743_model.core->CCR & SCB_CCR_DC_Msk) != 0U)
        m.bus[m.pending_address - 0x24000000U] = m.pending_byte;
    else
        *reinterpret_cast<uint8_t*>(m.pending_address) = m.pending_byte;
    m.pending = false;
    ++m.commits;
    --m.rx.NDTR;
    if (m.rx.NDTR == 0U) {
        m.dma.LISR |= DMA_LISR_TCIF1;
        if ((m.rx.CR & DMA_SxCR_DBM) != 0U) {
            m.rx.CR ^= DMA_SxCR_CT;
            m.rx.NDTR = m.initial;
        } else
            m.rx.CR &= ~(uint32_t)DMA_SxCR_EN;
    }
    if (m.stopping && !m.hold_en) {
        m.rx.CR &= ~(uint32_t)DMA_SxCR_EN;
        m.dma.LISR |= DMA_LISR_TCIF1;
    }
}
extern "C" void nx_h743_uart_continuous_model_reset(void) {
    nx_h743_model_reset();
    std::memset(&g_h743_uart_continuous_model, 0,
                sizeof(g_h743_uart_continuous_model));
    g_h743_uart_continuous_model.rx.FCR = 0x21U;
    g_h743_model.uart = USART3;
    USART3->ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
}
extern "C" uint32_t nx_h743_uart_continuous_model_read(volatile uint32_t* reg) {
    if (unavailable(reg)) {
        ++g_h743_uart_continuous_model.forbidden;
        return 0U;
    }
    return nx_h743_model_read(reg);
}
extern "C" void nx_h743_uart_continuous_model_write(volatile uint32_t* reg,
                                                    uint32_t value) {
    auto& m = g_h743_uart_continuous_model;
    if (unavailable(reg)) {
        ++m.forbidden;
        return;
    }
    if (reg == m.refuse_write)
        return;
    if (reg == &m.dma.LIFCR) {
        m.dma.LISR &= ~value;
        return;
    }
    if (reg == &m.rx.CR) {
        if ((value & DMA_SxCR_EN) == 0U && (m.rx.CR & DMA_SxCR_EN) != 0U) {
            m.stopping = true;
            if (m.hold_en || m.pending)
                value |= DMA_SxCR_EN;
            else
                m.dma.LISR |= DMA_LISR_TCIF1;
        }
        if ((value & DMA_SxCR_EN) != 0U && (m.rx.CR & DMA_SxCR_EN) == 0U) {
            ++m.enabled_transitions;
            m.stopping = false;
        }
        m.rx.CR = value;
        return;
    }
    if (reg == &m.rx.M0AR || reg == &m.rx.M1AR) {
        if (m.race_target && (m.rx.CR & DMA_SxCR_EN) != 0U) {
            m.race_target = false;
            m.rx.CR ^= DMA_SxCR_CT;
            m.rx.NDTR = m.initial;
            m.dma.LISR |= DMA_LISR_TCIF1;
        }
        bool active = (m.rx.CR & DMA_SxCR_CT) != 0U;
        if ((m.rx.CR & DMA_SxCR_EN) != 0U && ((reg == &m.rx.M1AR) == active)) {
            ++m.target_errors;
            return;
        }
        *reg = value;
        return;
    }
    if (inside(reg, &m.rx, sizeof(m.rx))) {
        if ((m.rx.CR & DMA_SxCR_EN) != 0U) {
            ++m.target_errors;
            return;
        }
        if (reg == &m.rx.NDTR)
            m.initial = value;
        *reg = reg == &m.rx.FCR ? value | 0x20U : value;
        return;
    }
    nx_h743_model_write(reg, value);
}
extern "C" void nx_h743_uart_continuous_model_step(void) {
    auto& m = g_h743_uart_continuous_model;
    ++g_h743_model.timer.CNT;
    commit();
    if (m.input_index == m.input_count ||
        (USART3->CR1 & (USART_CR1_UE | USART_CR1_RE)) !=
            (USART_CR1_UE | USART_CR1_RE))
        return;
    m.frame_cycles += 64U;
    uint32_t frame_cycles =
        m.peer_frame_cycles != 0U ? m.peer_frame_cycles : 10U * USART3->BRR;
    if (m.frame_cycles < frame_cycles)
        return;
    m.frame_cycles -= frame_cycles;
    uint8_t byte = m.input[m.input_index++];
    ++m.frames;
    if ((USART3->CR3 & USART_CR3_DMAR) != 0U && (m.rx.CR & DMA_SxCR_EN) != 0U &&
        m.rx.NDTR != 0U && m.mux.CCR == 45U && !m.pending) {
        uintptr_t address =
            (m.rx.CR & DMA_SxCR_CT) != 0U ? m.rx.M1AR : m.rx.M0AR;
        m.pending_address = address + m.initial - m.rx.NDTR;
        m.pending_byte = byte;
        m.pending = true;
        commit();
    } else {
        if ((USART3->ISR & USART_ISR_RXNE_RXFNE) != 0U)
            USART3->ISR |= USART_ISR_ORE;
        USART3->RDR = byte;
        USART3->ISR |= USART_ISR_RXNE_RXFNE;
    }
}
extern "C" void nx_h743_uart_continuous_model_feed(const uint8_t* data,
                                                   size_t bytes) {
    auto& m = g_h743_uart_continuous_model;
    if (bytes > sizeof(m.input) || m.input_count != m.input_index) {
        ++m.target_errors;
        return;
    }
    std::memcpy(m.input, data, bytes);
    m.input_count = bytes;
    m.input_index = 0U;
}
extern "C" bool nx_h743_uart_continuous_model_level(int irq) {
    auto& m = g_h743_uart_continuous_model;
    if (irq == DMA1_Stream1_IRQn)
        return ((m.dma.LISR & DMA_LISR_TCIF1) != 0U &&
                (m.rx.CR & DMA_SxCR_TCIE) != 0U) ||
               ((m.dma.LISR & DMA_LISR_TEIF1) != 0U &&
                (m.rx.CR & DMA_SxCR_TEIE) != 0U);
    return ((USART3->ISR & USART_ISR_IDLE) != 0U &&
            (USART3->CR1 & USART_CR1_IDLEIE) != 0U) ||
           ((USART3->ISR & USART_ISR_ORE) != 0U &&
            (USART3->CR3 & USART_CR3_EIE) != 0U);
}
extern "C" nx_arch_result_t
nx_h743_uart_continuous_model_invalidate(uintptr_t address, size_t bytes) {
    auto& m = g_h743_uart_continuous_model;
    ++m.cache_calls;
    bool hardware = (m.rx.CR & DMA_SxCR_EN) != 0U;
    bool aliases =
        hardware &&
        ((address < m.rx.M0AR + m.initial && m.rx.M0AR < address + bytes) ||
         (address < m.rx.M1AR + m.initial && m.rx.M1AR < address + bytes));
    if (nx_arch_irq_is_masked() || nx_arch_in_isr() || !range(address, bytes) ||
        address % 32U != 0U || bytes == 0U || bytes % 32U != 0U || aliases ||
        (m.pending && m.pending_address >= address &&
         m.pending_address - address < bytes)) {
        ++m.cache_errors;
        return NX_ARCH_INVALID;
    }
    if (m.cache_observer != nullptr)
        m.cache_observer();
    if (m.refuse_cache)
        return NX_ARCH_UNSUPPORTED;
    std::memcpy(reinterpret_cast<void*>(address), m.bus + address - 0x24000000U,
                bytes);
    return NX_ARCH_OK;
}
