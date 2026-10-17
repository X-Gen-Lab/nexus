/**
 * \file            u575_uart_rtscts_model.cpp
 * \brief           RM0456 FIFO queue and explicit wire bus side effects
 * \author          Nexus Team
 * \version         0.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_uart_rtscts_model.h"
#include <cstring>

nx_u575_uart_rtscts_model_t g_u575_uart_rtscts;

namespace {
constexpr uint32_t kEntryErrors = USART_ISR_PE | USART_ISR_FE | USART_ISR_NE;
constexpr uint32_t kLevels = USART_ISR_RXNE | USART_ISR_TXE | USART_ISR_RXFT |
                             USART_ISR_TXFT | USART_ISR_RXFF | USART_ISR_TXFE |
                             USART_ISR_TC | USART_ISR_CTS | kEntryErrors;
unsigned Threshold(uint32_t encoding) {
    constexpr unsigned values[8] = {1U, 2U, 4U, 6U, 7U, 8U, 0U, 0U};
    return values[encoding & 7U];
}
void Refresh() {
    auto& m = g_u575_uart_rtscts;
    auto* u = USART1_NS;
    uint32_t value = u->ISR & ~kLevels;
    if (m.received != 0U)
        value |= USART_ISR_RXNE | m.rx_error[0];
    if (m.received >= 9U)
        value |= USART_ISR_RXFF;
    const unsigned rx_threshold = Threshold(u->CR3 >> USART_CR3_RXFTCFG_Pos);
    if (rx_threshold != 0U && m.received >= rx_threshold)
        value |= USART_ISR_RXFT;
    if (m.queued < 8U)
        value |= USART_ISR_TXE;
    if (m.queued == 0U)
        value |= USART_ISR_TXFE;
    const unsigned tx_threshold = Threshold(u->CR3 >> USART_CR3_TXFTCFG_Pos);
    /* RM0456 ISR prose says strictly greater empty locations. The empty
     * encoding is an explicit empty-FIFO condition. Provider TDR access must
     * still check TXFNF; threshold is only a notification. */
    if ((tx_threshold == 8U && m.queued == 0U) ||
        (tx_threshold != 0U && 8U - m.queued > tx_threshold))
        value |= USART_ISR_TXFT;
    if ((m.saved_flags & USART_ISR_TC) != 0U)
        value |= USART_ISR_TC;
    if (!m.cts_high)
        value |= USART_ISR_CTS;
    u->ISR = value;
    const bool pending =
        (((value & USART_ISR_CTSIF) != 0U) &&
         ((u->CR3 & USART_CR3_CTSIE) != 0U)) ||
        (((value & USART_ISR_RXFT) != 0U) &&
         ((u->CR3 & USART_CR3_RXFTIE) != 0U)) ||
        (((value & USART_ISR_TXFT) != 0U) &&
         ((u->CR3 & USART_CR3_TXFTIE) != 0U)) ||
        (((value & USART_ISR_RXNE) != 0U) &&
         ((u->CR1 & USART_CR1_RXNEIE) != 0U)) ||
        (((value & USART_ISR_IDLE) != 0U) &&
         ((u->CR1 & USART_CR1_IDLEIE) != 0U)) ||
        (((value & USART_ISR_TC) != 0U) && ((u->CR1 & USART_CR1_TCIE) != 0U)) ||
        (((value & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) != 0U) &&
         ((u->CR3 & USART_CR3_EIE) != 0U)) ||
        (((value & USART_ISR_PE) != 0U) && ((u->CR1 & USART_CR1_PEIE) != 0U));
    if (pending && (u->CR1 & USART_CR1_UE) != 0U)
        g_u575_model.irq_pending[USART1_IRQn] = true;
}
} /* namespace */

void nx_u575_uart_rtscts_model_reset(void) {
    nx_u575_model_reset();
    std::memset(&g_u575_uart_rtscts, 0, sizeof(g_u575_uart_rtscts));
    std::memset(USART1_NS, 0, sizeof(*USART1_NS));
    g_u575_model.uart = USART1_NS;
    g_u575_uart_rtscts.saved_flags = USART_ISR_TC;
    Refresh();
}
uint32_t nx_u575_uart_rtscts_model_read(volatile uint32_t* reg) {
    auto& m = g_u575_uart_rtscts;
    if (m.before_read != nullptr)
        m.before_read(reg);
    Refresh();
    if (reinterpret_cast<uintptr_t>(reg) >=
            reinterpret_cast<uintptr_t>(USART1_NS) &&
        reinterpret_cast<uintptr_t>(reg) <
            reinterpret_cast<uintptr_t>(USART1_NS) + sizeof(*USART1_NS))
        m.uart_reads++;
    if (reg == &USART1_NS->ISR)
        m.isr_reads++;
    if (reg != &USART1_NS->RDR)
        return nx_u575_model_read(reg);
    m.rdr_reads++;
    if (m.received == 0U) {
        m.empty_reads++;
        return 0U;
    }
    const uint16_t result = m.rx[0];
    for (unsigned i = 1U; i < m.received; ++i) {
        m.rx[i - 1U] = m.rx[i];
        m.rx_error[i - 1U] = m.rx_error[i];
    }
    m.received--;
    Refresh();
    return result;
}
void nx_u575_uart_rtscts_model_write(volatile uint32_t* reg, uint32_t value) {
    auto& m = g_u575_uart_rtscts;
    auto* u = USART1_NS;
    if (m.before_write != nullptr)
        m.before_write(reg, value);
    if (reinterpret_cast<uintptr_t>(reg) >= reinterpret_cast<uintptr_t>(u) &&
        reinterpret_cast<uintptr_t>(reg) <
            reinterpret_cast<uintptr_t>(u) + sizeof(*u))
        m.uart_writes++;
    if (reg == m.reject_register)
        return;
    if (reg == &u->CR1) {
        if ((u->CR1 & USART_CR1_UE) != 0U &&
            ((value ^ u->CR1) & USART_CR1_FIFOEN) != 0U) {
            m.forbidden_config++;
            return;
        }
        if (m.reject_bits_register == nullptr ||
            m.reject_bits_register == reg) {
            value |= u->CR1 & m.reject_clear;
            value &= ~(m.reject_set & ~u->CR1);
        }
    }
    if (reg == &u->CR3) {
        constexpr uint32_t thresholds = USART_CR3_RXFTCFG | USART_CR3_TXFTCFG |
                                        USART_CR3_CTSE | USART_CR3_RTSE;
        if ((u->CR1 & USART_CR1_UE) != 0U &&
            ((value ^ u->CR3) & thresholds) != 0U) {
            m.forbidden_config++;
            return;
        }
        if (m.reject_bits_register == nullptr ||
            m.reject_bits_register == reg) {
            value |= u->CR3 & m.reject_clear;
            value &= ~(m.reject_set & ~u->CR3);
        }
    }
    if (reg == &u->ICR) {
        m.icr_writes++;
        m.saved_flags &= ~(value & USART_ISR_TC);
        /* W1C acts on the current RDR entry, before exposing the next. */
        if (m.received != 0U)
            m.rx_error[0] &= ~(value & kEntryErrors);
        u->ISR &= ~(value & (USART_ISR_IDLE | USART_ISR_ORE | USART_ISR_CTSIF));
        Refresh();
        return;
    }
    if (reg == &u->RQR) {
        /* Independent write-one requests may share one bus transaction. */
        if ((value & USART_RQR_TXFRQ) != 0U) {
            m.tx_flush_writes++;
            m.queued = 0U;
        }
        if ((value & USART_RQR_RXFRQ) != 0U) {
            m.flush_writes++;
            m.received = 0U;
        }
        Refresh();
        return;
    }
    if (reg == &u->TDR) {
        m.tdr_writes++;
        if (m.queued == 8U) {
            m.full_writes++;
            return;
        }
        m.saved_flags &= ~static_cast<uint32_t>(USART_ISR_TC);
        m.tx[m.queued++] = static_cast<uint8_t>(value);
        Refresh();
        return;
    }
    nx_u575_model_write(reg, value);
    Refresh();
}
void nx_u575_uart_rtscts_model_receive(uint8_t value, uint32_t errors) {
    auto& m = g_u575_uart_rtscts;
    if ((USART1_NS->CR1 & (USART_CR1_UE | USART_CR1_RE)) !=
        (USART_CR1_UE | USART_CR1_RE))
        return;
    if (m.received == 9U) {
        USART1_NS->ISR |= USART_ISR_ORE;
        m.lost_frames++;
    } else {
        m.rx[m.received] = value;
        m.rx_error[m.received++] = errors & kEntryErrors;
    }
    Refresh();
}
void nx_u575_uart_rtscts_model_idle(void) {
    USART1_NS->ISR |= USART_ISR_IDLE;
    Refresh();
}
void nx_u575_uart_rtscts_model_shift(void) {
    auto& m = g_u575_uart_rtscts;
    if (m.shift_busy || m.queued == 0U)
        return;
    if ((USART1_NS->CR3 & USART_CR3_CTSE) != 0U && m.cts_high) {
        m.blocked_shifts++;
        return;
    }
    m.shift_busy = true;
    for (unsigned i = 1U; i < m.queued; ++i)
        m.tx[i - 1U] = m.tx[i];
    m.queued--;
    Refresh();
}
void nx_u575_uart_rtscts_model_finish_frame(void) {
    auto& m = g_u575_uart_rtscts;
    if (m.shift_busy) {
        m.shift_busy = false;
        m.wire_frames++;
        if (m.queued == 0U)
            m.saved_flags |= USART_ISR_TC;
    }
    Refresh();
}

nx_result_t nx_u575_uart_rtscts_model_irq_control(int irq, bool enabled,
                                                  uint8_t priority) {
    nx_result_t result = nx_u575_model_irq_control(irq, enabled, priority);
    if (enabled) {
        g_u575_uart_rtscts.irq_enables++;
        if (g_u575_uart_rtscts.hold_priority)
            g_u575_model.priority[irq] = 0U;
    } else if (g_u575_uart_rtscts.hold_pending) {
        g_u575_model.irq_pending[irq] = true;
    }
    Refresh();
    return result;
}

void nx_u575_uart_rtscts_model_cts(bool high) {
    auto& m = g_u575_uart_rtscts;
    if (m.cts_high != high && (USART1_NS->CR3 & USART_CR3_CTSE) != 0U)
        USART1_NS->ISR |= USART_ISR_CTSIF;
    m.cts_high = high;
    Refresh();
}
bool nx_u575_uart_rtscts_model_rts_high(void) {
    /* This query describes only the admitted live peripheral. GPIO reset,
     * pinmux handoff and external propagation need separate qualification. */
    return (USART1_NS->CR3 & USART_CR3_RTSE) != 0U &&
           g_u575_uart_rtscts.received == 9U;
}
bool nx_u575_uart_rtscts_model_peer_receive(uint8_t value, uint32_t errors) {
    if (nx_u575_uart_rtscts_model_rts_high()) {
        g_u575_uart_rtscts.peer_blocked_frames++;
        return false;
    }
    nx_u575_uart_rtscts_model_receive(value, errors);
    return true;
}
