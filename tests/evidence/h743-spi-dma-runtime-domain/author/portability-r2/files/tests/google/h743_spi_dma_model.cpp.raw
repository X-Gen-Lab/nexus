/**
 * \file            h743_spi_dma_model.cpp
 * \brief           Hardware-only finite DMA commits, SPI frames and IRQ levels
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_spi_dma_model.h"
#include "stm32h743_spi_wire.h"
#include <cstring>
#undef nx_arch_dsb
#undef nx_arch_dmb
extern "C" {
nx_h743_spi_dma_model_t g_h743_spi_dma_model;
}
static uint32_t dma_flags(bool receive) {
    return receive ? 0x003d0000U : 0x0f400000U;
}
static uint32_t dma_tc(bool receive) {
    return receive ? DMA_LISR_TCIF2 : DMA_LISR_TCIF3;
}
static bool within(volatile uint32_t* p, const void* base, size_t bytes) {
    uintptr_t address = reinterpret_cast<uintptr_t>(p);
    uintptr_t begin = reinterpret_cast<uintptr_t>(base);
    return address >= begin && address - begin < bytes;
}
static bool clocked(volatile uint32_t* p) {
    auto& m = g_h743_spi_dma_model;
    if (within(p, &m.spi, sizeof(m.spi)))
        return (g_h743_model.rcc.APB2ENR & RCC_APB2ENR_SPI4EN) != 0U;
    if (within(p, &m.dma, sizeof(m.dma)) ||
        within(p, m.streams, sizeof(m.streams)) ||
        within(p, m.mux, sizeof(m.mux)))
        return (g_h743_model.rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN) != 0U;
    return true;
}
/** \brief Reset ownership is independent of the still-enabled bus gate. */
static bool reset_domain_access(volatile uint32_t* p) {
    auto& m = g_h743_spi_dma_model;
    return (g_h743_model.rcc.AHB1RSTR &
            (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U &&
           (within(p, &m.dma, sizeof(m.dma)) ||
            within(p, m.streams, sizeof(m.streams)) ||
            within(p, m.mux, sizeof(m.mux)));
}
static bool held_in_reset(volatile uint32_t* p) {
    auto& m = g_h743_spi_dma_model;
    uint32_t reset = g_h743_model.rcc.AHB1RSTR;
    return ((reset & RCC_AHB1RSTR_DMA1RST) != 0U &&
            (within(p, &m.dma, sizeof(m.dma)) ||
             within(p, m.streams, sizeof(m.streams)))) ||
           ((reset & (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U &&
            within(p, m.mux, sizeof(m.mux)));
}
static void fifo_status() {
    auto& m = g_h743_spi_dma_model;
    m.spi.SR &= ~(uint32_t)(NX_STM32H743_SPI_FIFO_LEVELS | SPI_SR_TXP);
    if (m.rx_count != 0U)
        m.spi.SR |= static_cast<uint32_t>(
            SPI_SR_RXP | ((m.rx_count % 4U) << SPI_SR_RXPLVL_Pos));
    if (m.rx_count >= 4U)
        m.spi.SR |= SPI_SR_RXWNE;
    if (m.tx_count < 8U)
        m.spi.SR |= SPI_SR_TXP;
}
static uint8_t pop(uint8_t* data, unsigned& count) {
    uint8_t result = data[0];
    for (unsigned n = 1U; n < count; ++n)
        data[n - 1U] = data[n];
    --count;
    return result;
}
static void finish_rx_commit() {
    auto& m = g_h743_spi_dma_model;
    if (!m.rx_pending || m.hold_rx_commit)
        return;
    *reinterpret_cast<uint8_t*>(m.pending_rx_address) = m.pending_rx_value;
    ++m.memory_commits;
    m.rx_pending = false;
    --m.streams[2].NDTR;
    if (m.streams[2].NDTR == 0U || m.rx_stop_pending) {
        m.streams[2].CR &= ~(uint32_t)DMA_SxCR_EN;
        m.dma.LISR |= DMA_LISR_TCIF2;
        m.rx_stop_pending = false;
    }
}
extern "C" {
void nx_h743_spi_dma_model_reset(void) {
    nx_h743_model_reset();
    std::memset(&g_h743_spi_dma_model, 0, sizeof(g_h743_spi_dma_model));
    auto& m = g_h743_spi_dma_model;
    for (auto& stream : m.streams)
        stream.FCR = 0x21U;
    m.spi.CFG1 = NX_STM32H743_SPI_BYTE_CONFIG;
    m.spi.SR = SPI_SR_TXP;
    g_h743_model.spi = &m.spi;
}
/** \brief External reset event; SPI and NVIC are independent domains.
 * \note These cases inject before any pending RX memory transaction. No
 * outstanding-bus reset completion or subsequent electrical behavior is
 * modeled.
 */
void nx_h743_spi_dma_model_assert_reset(uint32_t mask) {
    auto& m = g_h743_spi_dma_model;
    g_h743_model.rcc.AHB1RSTR |= mask;
    if ((mask & RCC_AHB1RSTR_DMA1RST) != 0U) {
        std::memset(&m.dma, 0, sizeof(m.dma));
        std::memset(m.streams, 0, sizeof(m.streams));
        for (auto& stream : m.streams)
            stream.FCR = 0x21U;
    }
    if ((mask & (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_DMA2RST)) != 0U)
        std::memset(m.mux, 0, sizeof(m.mux));
}
uint32_t nx_h743_spi_dma_model_read(volatile uint32_t* reg) {
    auto& m = g_h743_spi_dma_model;
    ++m.reads;
    if (reset_domain_access(reg))
        ++m.reset_domain_accesses;
    if (!clocked(reg)) {
        ++m.gated_accesses;
        return 0U;
    }
    if (reg == &m.spi.CR1 && m.complete_on_cr1_read &&
        (m.spi.CR1 & SPI_CR1_CSTART) != 0U) {
        m.complete_on_cr1_read = false;
        for (unsigned n = 0U; n < 32U && (m.spi.CR1 & SPI_CR1_CSTART) != 0U;
             ++n)
            nx_h743_spi_dma_model_step();
    }
    if (within(reg, &m.spi, sizeof(m.spi)) ||
        within(reg, &m.dma, sizeof(m.dma)) ||
        within(reg, m.streams, sizeof(m.streams)) ||
        within(reg, m.mux, sizeof(m.mux)))
        return *reg;
    return nx_h743_model_read(reg);
}
void nx_h743_spi_dma_model_write(volatile uint32_t* reg, uint32_t value) {
    auto& m = g_h743_spi_dma_model;
    ++m.writes;
    if (reset_domain_access(reg))
        ++m.reset_domain_accesses;
    if (held_in_reset(reg))
        return;
    if (!clocked(reg)) {
        ++m.gated_accesses;
        return;
    }
    if (reg == m.ignore &&
        (m.ignore_mask == 0U || (value & m.ignore_mask) != 0U))
        return;
    for (unsigned n = 0U; n < 2U; ++n) {
        auto& stream = m.streams[n + 2U];
        if (reg == &stream.CR) {
            if ((value & DMA_SxCR_EN) != 0U &&
                (stream.CR & DMA_SxCR_EN) == 0U && m.enable_observer != nullptr)
                m.enable_observer();
            if ((value & DMA_SxCR_EN) == 0U &&
                (stream.CR & DMA_SxCR_EN) != 0U) {
                if (m.hold_en[n] || (n == 0U && m.rx_pending)) {
                    if (n == 0U && m.rx_pending)
                        m.rx_stop_pending = true;
                    value |= DMA_SxCR_EN;
                } else {
                    m.dma.LISR |= dma_tc(n == 0U);
                }
            }
            if (m.hold_mask[n])
                value |= stream.CR & (uint32_t)(DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                                DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
            stream.CR = value;
            return;
        }
        if (within(reg, &stream, sizeof(stream))) {
            if ((stream.CR & DMA_SxCR_EN) != 0U &&
                (reg != &stream.FCR || ((value ^ stream.FCR) & 7U) != 0U)) {
                ++m.ordering_errors;
                return;
            }
            if (reg == &stream.NDTR)
                m.initial[n] = value;
            if (reg == &stream.FCR) {
                stream.FCR = (value & 0x87U) | 0x20U;
            } else {
                *reg = value;
            }
            return;
        }
    }
    if (reg == &m.dma.LIFCR) {
        uint32_t permitted = value;
        if (m.hold_w1c[0])
            permitted &= ~dma_flags(true);
        if (m.hold_w1c[1])
            permitted &= ~dma_flags(false);
        m.dma.LISR &= ~permitted;
        return;
    }
    if (reg == &m.spi.CR1) {
        if ((value & SPI_CR1_CSTART) != 0U) {
            if ((value & SPI_CR1_CSUSP) != 0U)
                ++m.unsafe_commands;
            if ((m.spi.CR1 & SPI_CR1_CSTART) == 0U) {
                ++m.starts;
                if (m.ignore_start)
                    value &= ~(uint32_t)SPI_CR1_CSTART;
                else {
                    m.transaction_frames = 0U;
                    m.spi.SR &=
                        ~(uint32_t)(SPI_SR_EOT | SPI_SR_TXC | SPI_SR_SUSP);
                }
            }
        }
        if ((value & SPI_CR1_CSUSP) != 0U) {
            ++m.suspend_commands;
            if ((m.spi.CR1 & SPI_CR1_CSTART) != 0U)
                m.suspend_pending = true;
        }
        value &= ~(uint32_t)SPI_CR1_CSUSP;
        if ((m.spi.CR1 & SPI_CR1_CSTART) != 0U)
            value |= SPI_CR1_CSTART;
        if ((value & SPI_CR1_SPE) == 0U) {
            if ((m.spi.CR1 & SPI_CR1_CSTART) != 0U)
                ++m.ordering_errors;
            m.tx_count = m.rx_count = 0U;
            value &= ~(uint32_t)SPI_CR1_CSTART;
        }
        m.spi.CR1 = value;
        fifo_status();
        return;
    }
    if (reg == &m.spi.IER) {
        m.spi.IER = value | (m.hold_mask[2] ? m.spi.IER : 0U);
        return;
    }
    if (reg == &m.spi.IFCR) {
        if (m.exception != 0U)
            ++m.eot_clears_in_irq;
        if (!m.hold_w1c[2]) {
            m.spi.SR &= ~value;
            if ((value & SPI_IFCR_EOTC) != 0U && m.spi.CR2 != 0U)
                m.spi.SR &= ~(uint32_t)SPI_SR_TXC;
        }
        return;
    }
    if (within(reg, &m.spi, sizeof(m.spi))) {
        *reg = value;
        return;
    }
    nx_h743_model_write(reg, value);
    if (reg == &g_h743_model.rcc.APB2RSTR &&
        (value & RCC_APB2RSTR_SPI4RST) != 0U && !g_h743_model.hold_spi_reset) {
        ++m.resets;
        m.tx_count = m.rx_count = 0U;
        fifo_status();
    }
}
uint8_t nx_h743_spi_dma_model_read8(volatile uint32_t* reg) {
    auto& m = g_h743_spi_dma_model;
    ++m.reads;
    if (reg != &m.spi.RXDR || m.rx_count == 0U) {
        ++m.ordering_errors;
        return 0U;
    }
    uint8_t result = pop(m.rx_fifo, m.rx_count);
    fifo_status();
    return result;
}
void nx_h743_spi_dma_model_step(void) {
    auto& m = g_h743_spi_dma_model;
    ++g_h743_model.timer.CNT;
    if ((g_h743_model.rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN) == 0U ||
        (g_h743_model.rcc.APB2ENR & RCC_APB2ENR_SPI4EN) == 0U)
        return;
    finish_rx_commit();
    if ((m.streams[3].CR & DMA_SxCR_EN) != 0U &&
        m.streams[3].PAR != SPI4_BASE + 0x20U)
        nx_h743_spi_dma_model_error(false);
    if ((m.streams[2].CR & DMA_SxCR_EN) != 0U &&
        m.streams[2].PAR != SPI4_BASE + 0x30U)
        nx_h743_spi_dma_model_error(true);
    auto& tx = m.streams[3];
    auto& rx = m.streams[2];
    if ((m.spi.CFG1 & SPI_CFG1_TXDMAEN) != 0U && (tx.CR & DMA_SxCR_EN) != 0U &&
        tx.NDTR != 0U && m.mux[3].CCR == 84U && m.tx_count < 8U) {
        /* NDTR accounts DMA writes into FIFO, not completed wire frames. */
        uintptr_t address = tx.M0AR + m.initial[1] - tx.NDTR;
        uint8_t value = *reinterpret_cast<uint8_t*>(address);
        if (m.cpu_pending && address == m.pending_cpu_address)
            value = m.old_cpu_value;
        m.tx_fifo[m.tx_count++] = value;
        --tx.NDTR;
        if (tx.NDTR == 0U) {
            tx.CR &= ~(uint32_t)DMA_SxCR_EN;
            m.dma.LISR |= DMA_LISR_TCIF3;
        }
    }
    const unsigned divisor =
        2U << ((m.spi.CFG1 & SPI_CFG1_MBR) >> SPI_CFG1_MBR_Pos);
    if ((m.spi.CR1 & (SPI_CR1_SPE | SPI_CR1_CSTART)) ==
        (SPI_CR1_SPE | SPI_CR1_CSTART)) {
        if (m.suspend_pending && !m.hold_suspend && m.frame_ticks == 0U) {
            m.suspend_pending = false;
            m.spi.CR1 &= ~(uint32_t)SPI_CR1_CSTART;
            m.spi.SR |= SPI_SR_SUSP;
            g_h743_model.spi_tail_until =
                g_h743_model.timer.CNT + (divisor + 63U) / 64U;
        } else if (!m.hold_wire && m.tx_count != 0U &&
                   ++m.frame_ticks >= (divisor * 8U + 63U) / 64U) {
            m.frame_ticks = 0U;
            uint8_t value = pop(m.tx_fifo, m.tx_count);
            ++m.frames;
            ++m.transaction_frames;
            if (m.rx_count < 8U)
                m.rx_fifo[m.rx_count++] = static_cast<uint8_t>(~value);
            else
                m.spi.SR |= SPI_SR_OVR;
            if (m.transaction_frames == (m.spi.CR2 & SPI_CR2_TSIZE)) {
                m.spi.CR1 &= ~(uint32_t)SPI_CR1_CSTART;
                m.spi.SR |= SPI_SR_EOT | SPI_SR_TXC;
                m.suspend_pending = false;
                g_h743_model.spi_tail_until =
                    g_h743_model.timer.CNT + (divisor + 63U) / 64U;
            }
        }
    }
    if (!m.rx_pending && !m.rx_stop_pending && m.rx_count != 0U &&
        (m.spi.CFG1 & SPI_CFG1_RXDMAEN) != 0U && (rx.CR & DMA_SxCR_EN) != 0U &&
        rx.NDTR != 0U && m.mux[2].CCR == 83U) {
        m.pending_rx_address = rx.M0AR + m.initial[0] - rx.NDTR;
        m.pending_rx_value = pop(m.rx_fifo, m.rx_count);
        m.rx_pending = true;
        finish_rx_commit();
    }
    fifo_status();
}
void nx_h743_spi_dma_model_error(bool receive) {
    auto& m = g_h743_spi_dma_model;
    m.dma.LISR |= receive ? DMA_LISR_TEIF2 : DMA_LISR_TEIF3;
    if (receive && m.rx_pending)
        m.rx_stop_pending = true;
    else
        m.streams[receive ? 2U : 3U].CR &= ~(uint32_t)DMA_SxCR_EN;
}
bool nx_h743_spi_dma_model_level(int irq) {
    auto& m = g_h743_spi_dma_model;
    if (irq == 84)
        return (m.spi.SR & m.spi.IER &
                (SPI_SR_EOT | SPI_SR_OVR | SPI_SR_MODF | SPI_SR_TIFRE)) != 0U;
    const unsigned n = irq == 13 ? 0U : 1U;
    const unsigned shift = n == 0U ? 16U : 22U;
    uint32_t flags = (m.dma.LISR >> shift) & 0x3dU;
    uint32_t control = m.streams[n + 2U].CR;
    return ((flags & 0x20U) != 0U && (control & DMA_SxCR_TCIE) != 0U) ||
           ((flags & 0x08U) != 0U && (control & DMA_SxCR_TEIE) != 0U) ||
           ((flags & 0x04U) != 0U && (control & DMA_SxCR_DMEIE) != 0U) ||
           ((flags & 0x01U) != 0U &&
            (m.streams[n + 2U].FCR & DMA_SxFCR_FEIE) != 0U);
}
bool nx_h743_spi_dma_model_enabled(int irq) {
    ++g_h743_spi_dma_model.reads;
    return g_h743_model.irq_enabled[irq];
}
bool nx_h743_spi_dma_model_pending(int irq) {
    ++g_h743_spi_dma_model.reads;
    return g_h743_model.irq_pending[irq];
}
bool nx_h743_spi_dma_model_active(int irq) {
    ++g_h743_spi_dma_model.reads;
    return g_h743_model.irq_active[irq];
}
void nx_h743_spi_dma_model_enable(int irq) {
    ++g_h743_spi_dma_model.writes;
    g_h743_model.irq_enabled[irq] = true;
}
void nx_h743_spi_dma_model_disable(int irq) {
    ++g_h743_spi_dma_model.writes;
    if (!g_h743_spi_dma_model.hold_disable[irq]) {
        g_h743_model.irq_enabled[irq] = false;
        if (g_h743_spi_dma_model.pending_on_disable[irq]) {
            g_h743_spi_dma_model.pending_on_disable[irq] = false;
            g_h743_model.irq_pending[irq] = true;
        }
    }
}
void nx_h743_spi_dma_model_clear(int irq) {
    ++g_h743_spi_dma_model.writes;
    if (!g_h743_spi_dma_model.hold_pending_clear[irq])
        g_h743_model.irq_pending[irq] = false;
}
void nx_h743_spi_dma_model_priority(int irq, uint8_t priority) {
    ++g_h743_spi_dma_model.writes;
    g_h743_model.priority[irq] = priority;
}
void nx_h743_spi_dma_model_dsb(void) {
    ++g_h743_spi_dma_model.dsbs;
    g_h743_spi_dma_model.cpu_pending = false;
    nx_arch_dsb();
}
void nx_h743_spi_dma_model_dmb(void) {
    ++g_h743_spi_dma_model.dmbs;
    nx_arch_dmb();
}
uint32_t nx_h743_spi_dma_model_address(const void* address) {
    if (address == (const void*)&g_h743_spi_dma_model.spi.TXDR)
        return SPI4_BASE + 0x20U;
    if (address == (const void*)&g_h743_spi_dma_model.spi.RXDR)
        return SPI4_BASE + 0x30U;
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(address));
}
void nx_h743_spi_dma_model_cpu_store(uint8_t* target, uint8_t value) {
    g_h743_spi_dma_model.cpu_pending = true;
    g_h743_spi_dma_model.pending_cpu_address =
        reinterpret_cast<uintptr_t>(target);
    g_h743_spi_dma_model.old_cpu_value = *target;
    *target = value;
}
}
