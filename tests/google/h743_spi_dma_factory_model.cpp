/**
 * \file            h743_spi_dma_factory_model.cpp
 * \brief           Independent UART masters share the frozen SPI DMA bus
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_spi_dma_factory_model.h"
#include <cstring>
extern "C" {
USART_TypeDef g_h743_spi_dma_factory_uart;
nx_h743_spi_dma_factory_model_t g_h743_spi_dma_factory_model;
}
static uint32_t tc(unsigned lane) {
    return lane == 0U ? DMA_LISR_TCIF0 : DMA_LISR_TCIF1;
}
static bool axi(uintptr_t p) {
    return p >= 0x24000000U && p < 0x24080000U;
}
static void commit() {
    auto& m = g_h743_spi_dma_factory_model;
    if (!m.rx_pending || m.hold_commit)
        return;
    *reinterpret_cast<uint8_t*>(m.pending_address) = m.pending_value;
    m.rx_pending = false;
    ++m.memory_commits;
    --DMA1_Stream1->NDTR;
    if (DMA1_Stream1->NDTR == 0U || m.rx_stop_pending) {
        DMA1_Stream1->CR &= ~(uint32_t)DMA_SxCR_EN;
        DMA1->LISR |= tc(1U);
        m.rx_stop_pending = false;
    }
}
extern "C" {
void nx_h743_spi_dma_factory_reset(void) {
    nx_h743_spi_dma_model_reset();
    std::memset(&g_h743_spi_dma_factory_uart, 0,
                sizeof(g_h743_spi_dma_factory_uart));
    std::memset(&g_h743_spi_dma_factory_model, 0,
                sizeof(g_h743_spi_dma_factory_model));
    g_h743_model.uart = USART3;
    USART3->ISR = USART_ISR_TXE_TXFNF | USART_ISR_TC;
}
void nx_h743_spi_dma_factory_write(volatile uint32_t* reg, uint32_t value) {
    auto& m = g_h743_spi_dma_factory_model;
    for (unsigned lane = 0U; lane < 2U; ++lane) {
        auto& s = g_h743_spi_dma_model.streams[lane];
        if (reg == &s.CR) {
            ++g_h743_spi_dma_model.writes;
            if ((RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN) == 0U) {
                ++g_h743_spi_dma_model.gated_accesses;
                return;
            }
            if ((s.CR & DMA_SxCR_EN) != 0U && (value & DMA_SxCR_EN) == 0U) {
                if (m.hold_en[lane] || (lane == 1U && m.rx_pending)) {
                    value |= DMA_SxCR_EN;
                    if (lane == 1U && m.rx_pending)
                        m.rx_stop_pending = true;
                } else
                    DMA1->LISR |= tc(lane);
            }
            s.CR = value;
            return;
        }
        if (reg == &s.NDTR)
            m.initial[lane] = value;
    }
    nx_h743_spi_dma_model_write(reg, value);
    if (m.reset_on_dma_enable && reg == &RCC->AHB1ENR &&
        (value & RCC_AHB1ENR_DMA1EN) != 0U) {
        RCC->AHB1RSTR |= m.reset_mask;
        m.reset_on_dma_enable = false;
        ++m.reset_events;
    }
    if (m.trim_after_pins && reg == &GPIOE->AFR[1] &&
        (GPIOE->AFR[1] & 0x0fff0000U) == 0x05550000U &&
        (TIM2->CR1 & TIM_CR1_CEN) != 0U) {
        RCC->HSICFGR = (RCC->HSICFGR & ~(uint32_t)RCC_HSICFGR_HSITRIM) |
                       (65U << RCC_HSICFGR_HSITRIM_Pos);
        m.trim_after_pins = false;
        ++m.trim_events;
    }
}
void nx_h743_spi_dma_factory_feed(const uint8_t* data, size_t length) {
    auto& m = g_h743_spi_dma_factory_model;
    if (length > sizeof(m.input) || m.input_index != m.input_count) {
        ++m.model_errors;
        return;
    }
    std::memcpy(m.input, data, length);
    m.input_count = static_cast<unsigned>(length);
    m.input_index = 0U;
}
void nx_h743_spi_dma_factory_step(void) {
    nx_h743_spi_dma_model_step();
    auto& m = g_h743_spi_dma_factory_model;
    if ((RCC->APB1LENR & RCC_APB1LENR_USART3EN) == 0U)
        return;
    if (m.shift_active && ++m.tx_ticks >= 87U) {
        m.shift_active = false;
        if (m.sent_count < sizeof(m.sent))
            m.sent[m.sent_count++] = m.shift_byte;
        else
            ++m.model_errors;
        USART3->ISR |= USART_ISR_TXE_TXFNF;
        if (DMA1_Stream0->NDTR == 0U)
            USART3->ISR |= USART_ISR_TC;
    }
    if ((RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN) == 0U)
        return;
    auto& tx = *DMA1_Stream0;
    if (!m.shift_active && (USART3->CR3 & USART_CR3_DMAT) != 0U &&
        (tx.CR & DMA_SxCR_EN) != 0U && tx.NDTR != 0U &&
        DMAMUX1_Channel0->CCR == 46U) {
        uintptr_t address = tx.M0AR + m.initial[0] - tx.NDTR;
        if (!axi(address) ||
            tx.PAR != static_cast<uint32_t>(
                          reinterpret_cast<uintptr_t>(&USART3->TDR))) {
            ++m.model_errors;
            tx.CR &= ~(uint32_t)DMA_SxCR_EN;
            DMA1->LISR |= DMA_LISR_TEIF0;
        } else {
            m.shift_byte = *reinterpret_cast<const uint8_t*>(address);
            m.shift_active = true;
            m.tx_ticks = 0U;
            USART3->ISR &= ~(uint32_t)(USART_ISR_TC | USART_ISR_TXE_TXFNF);
            if (--tx.NDTR == 0U) {
                tx.CR &= ~(uint32_t)DMA_SxCR_EN;
                DMA1->LISR |= tc(0U);
            }
        }
    }
    commit();
    auto& rx = *DMA1_Stream1;
    if (!m.rx_pending && !m.rx_stop_pending && m.input_index < m.input_count &&
        ++m.rx_ticks >= 16U && (USART3->CR3 & USART_CR3_DMAR) != 0U &&
        (rx.CR & DMA_SxCR_EN) != 0U && rx.NDTR != 0U &&
        DMAMUX1_Channel1->CCR == 45U) {
        m.rx_ticks = 0U;
        m.pending_address = rx.M0AR + m.initial[1] - rx.NDTR;
        if (!axi(m.pending_address) ||
            rx.PAR != static_cast<uint32_t>(
                          reinterpret_cast<uintptr_t>(&USART3->RDR))) {
            ++m.model_errors;
            rx.CR &= ~(uint32_t)DMA_SxCR_EN;
            DMA1->LISR |= DMA_LISR_TEIF1;
        } else {
            USART3->RDR = m.input[m.input_index++];
            m.pending_value = static_cast<uint8_t>(USART3->RDR);
            m.rx_pending = true;
            commit();
        }
    }
}
bool nx_h743_spi_dma_factory_level(int irq) {
    if (irq == 13 || irq == 14 || irq == 84)
        return nx_h743_spi_dma_model_level(irq);
    if (irq == 39)
        return ((USART3->ISR & USART_ISR_TC) != 0U &&
                (USART3->CR1 & USART_CR1_TCIE) != 0U) ||
               ((USART3->ISR & USART_ISR_IDLE) != 0U &&
                (USART3->CR1 & USART_CR1_IDLEIE) != 0U);
    if (irq != 11 && irq != 12)
        return false;
    unsigned lane = irq == 11 ? 0U : 1U;
    unsigned shift = lane == 0U ? 0U : 6U;
    uint32_t flags = (DMA1->LISR >> shift) & 0x3dU;
    uint32_t control = g_h743_spi_dma_model.streams[lane].CR;
    return ((flags & 0x20U) != 0U && (control & DMA_SxCR_TCIE) != 0U) ||
           ((flags & 8U) != 0U && (control & DMA_SxCR_TEIE) != 0U) ||
           ((flags & 4U) != 0U && (control & DMA_SxCR_DMEIE) != 0U);
}
}
