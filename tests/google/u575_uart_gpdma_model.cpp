/**
 * \file            u575_uart_gpdma_model.cpp
 * \brief           Draft RM0456 register side effects and explicit bus events
 * \author          Nexus Team
 * \version         0.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_uart_gpdma_model.h"
#include <cstring>

nx_u575_uart_gpdma_model_t g_u575_uart_gpdma;

namespace {
constexpr uint32_t kFlags = DMA_CSR_TCF | DMA_CSR_HTF | DMA_CSR_DTEF |
                            DMA_CSR_ULEF | DMA_CSR_USEF | DMA_CSR_SUSPF |
                            DMA_CSR_TOF;
constexpr uintptr_t kSramBegin = 0x20000000U;
constexpr uintptr_t kSramEnd = 0x20030000U;

bool ConfigurationRegister(volatile uint32_t* reg) {
    const auto* channel = g_u575_uart_gpdma.channel;
    return reg == &channel->CLBAR || reg == &channel->CTR1 ||
           reg == &channel->CTR2 || reg == &channel->CBR1 ||
           reg == &channel->CSAR || reg == &channel->CDAR ||
           reg == &channel->CTR3 || reg == &channel->CBR2 ||
           reg == &channel->CLLR;
}

bool ClockedDmaAccess(volatile uint32_t* reg) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    const uintptr_t begin = reinterpret_cast<uintptr_t>(GPDMA1_NS);
    if (address >= begin && address < begin + 0x850U &&
        (g_u575_model.rcc.AHB1ENR & RCC_AHB1ENR_GPDMA1EN) == 0U) {
        g_u575_uart_gpdma.unclocked_dma_accesses++;
        return false;
    }
    return true;
}
} /* namespace */

void nx_u575_uart_gpdma_model_reset(void) {
    nx_u575_model_reset();
    std::memset(&g_u575_uart_gpdma, 0, sizeof(g_u575_uart_gpdma));
    g_u575_uart_gpdma.engine = GPDMA1_NS;
    g_u575_uart_gpdma.channel = GPDMA1_Channel0_NS;
    g_u575_uart_gpdma.uart = USART1_NS;
    std::memset(GPDMA1_NS, 0, sizeof(*GPDMA1_NS));
    std::memset(GPDMA1_Channel0_NS, 0, sizeof(*GPDMA1_Channel0_NS));
    std::memset(USART1_NS, 0, sizeof(*USART1_NS));
    GPDMA1_Channel0_NS->CSR = DMA_CSR_IDLEF;
    USART1_NS->ISR = USART_ISR_TXE | USART_ISR_TC;
    g_u575_model.uart = USART1_NS;
}

uint32_t nx_u575_uart_gpdma_model_read(volatile uint32_t* reg) {
    if (!ClockedDmaAccess(reg)) {
        g_u575_model.reads++;
        return 0U;
    }
    auto* channel = g_u575_uart_gpdma.channel;
    if (reg == &channel->CCR) {
        g_u575_model.reads++;
        /* RESET is write-only, not a retained completion bit. */
        return channel->CCR & ~static_cast<uint32_t>(DMA_CCR_RESET);
    }
    if (reg == &channel->CFCR) {
        g_u575_model.reads++;
        return 0U;
    }
    return nx_u575_model_read(reg);
}

void nx_u575_uart_gpdma_model_write(volatile uint32_t* reg, uint32_t value) {
    auto& model = g_u575_uart_gpdma;
    auto* channel = model.channel;
    if (model.before_write != nullptr)
        model.before_write(model.write_context, reg, value);
    if (!ClockedDmaAccess(reg)) {
        g_u575_model.writes++;
        return;
    }
    if (reg == model.reject_write) {
        g_u575_model.writes++;
        return;
    }
    if (reg == &channel->CSR) {
        g_u575_model.writes++;
        return; /* CSR is read-only, including IDLE and FIFO level. */
    }
    if (reg == &model.engine->RCFGLOCKR) {
        g_u575_model.writes++;
        model.engine->RCFGLOCKR |= value & 0xffffU;
        return;
    }
    if (reg == &model.engine->SECCFGR || reg == &model.engine->PRIVCFGR) {
        g_u575_model.writes++;
        const uint32_t locked = model.engine->RCFGLOCKR & 0xffffU;
        *reg = (*reg & locked) | (value & ~locked);
        return;
    }
    if (reg == &channel->CFCR) {
        g_u575_model.writes++;
        channel->CSR |= model.late_error_at_ack;
        model.late_error_at_ack = 0U;
        if (!model.refuse_ack)
            channel->CSR &= ~(value & kFlags);
        return;
    }
    if (ConfigurationRegister(reg) && (channel->CCR & DMA_CCR_EN) != 0U) {
        g_u575_model.writes++;
        model.configuration_writes_while_enabled++;
        return; /* Channel configuration is read-only while enabled. */
    }
    if (reg == &channel->CCR) {
        g_u575_model.writes++;
        model.ccr_writes++;
        const uint32_t old = channel->CCR;
        const bool enabled = (old & DMA_CCR_EN) != 0U;
        const bool idle = (channel->CSR & DMA_CSR_IDLEF) != 0U;
        const bool suspended = (channel->CSR & DMA_CSR_SUSPF) != 0U;
        if ((value & DMA_CCR_RESET) != 0U) {
            model.reset_requests++;
            if (!idle || (enabled && !suspended)) {
                model.illegal_reset_requests++;
                return;
            }
            if (model.refuse_reset)
                return;
            channel->CCR =
                value & ~static_cast<uint32_t>(DMA_CCR_RESET | DMA_CCR_SUSP |
                                               DMA_CCR_EN);
            channel->CSR &= ~static_cast<uint32_t>(DMA_CSR_FIFOL);
            channel->CSR |= DMA_CSR_IDLEF;
            return;
        }
        uint32_t next = value;
        if (enabled) {
            if ((value & DMA_CCR_EN) == 0U)
                model.ignored_enable_clears++;
            next |= DMA_CCR_EN;
            const uint32_t cold = DMA_CCR_PRIO | DMA_CCR_LAP | DMA_CCR_LSM;
            next = (next & ~cold) | (old & cold);
        } else if ((value & DMA_CCR_EN) != 0U) {
            if (model.refuse_enable)
                next &= ~static_cast<uint32_t>(DMA_CCR_EN);
            else
                channel->CSR &= ~static_cast<uint32_t>(DMA_CSR_IDLEF);
        }
        if ((value & DMA_CCR_SUSP) != 0U && (old & DMA_CCR_SUSP) == 0U)
            model.suspend_requests++;
        next |= old & model.refuse_ccr_clear;
        channel->CCR = next;
        return;
    }
    if (reg == &model.uart->CR3 && (model.uart->CR3 & USART_CR3_DMAT) != 0U &&
        (value & USART_CR3_DMAT) == 0U)
        model.dmat_falls++;
    if (reg == &g_u575_model.rcc.AHB1RSTR &&
        (value & RCC_AHB1RSTR_GPDMA1RST) != 0U) {
        model.global_reset_requests++;
        std::memset(model.engine, 0, sizeof(*model.engine));
        for (uintptr_t index = 0U; index < 16U; ++index) {
            auto* target = reinterpret_cast<DMA_Channel_TypeDef*>(
                reinterpret_cast<uintptr_t>(model.engine) + 0x50U +
                0x80U * index);
            std::memset(target, 0, sizeof(*target));
            target->CSR = DMA_CSR_IDLEF;
        }
    }
    nx_u575_model_write(reg, value);
}

nx_result_t nx_u575_uart_gpdma_model_irq(int irq, bool enabled,
                                         uint8_t priority) {
    auto& model = g_u575_uart_gpdma;
    if (irq != GPDMA1_Channel0_IRQn)
        return nx_u575_model_irq_control(irq, enabled, priority);
    if (priority >= 16U)
        return NX_ERROR_INVALID;
    if (enabled && !model.refuse_dma_irq_enable) {
        g_u575_model.irq_enabled[irq] = true;
        if (!model.refuse_dma_irq_priority)
            g_u575_model.priority[irq] = priority;
        g_u575_model.irq_pending[irq] = false;
    }
    if (!enabled) {
        if (!model.hold_dma_irq_enabled)
            g_u575_model.irq_enabled[irq] = false;
        if (!model.hold_dma_irq_pending)
            g_u575_model.irq_pending[irq] = false;
    }
    return g_u575_model.irq_enabled[irq] == enabled &&
                   (enabled ? g_u575_model.priority[irq] == priority
                            : !g_u575_model.irq_pending[irq])
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

void nx_u575_uart_gpdma_advance_bytes(size_t bytes) {
    auto& model = g_u575_uart_gpdma;
    auto* channel = model.channel;
    while (bytes-- != 0U && (channel->CCR & DMA_CCR_EN) != 0U &&
           (channel->CCR & DMA_CCR_SUSP) == 0U &&
           (model.uart->CR3 & USART_CR3_DMAT) != 0U &&
           (model.uart->CR1 & (USART_CR1_UE | USART_CR1_TE)) ==
               (USART_CR1_UE | USART_CR1_TE)) {
        const uint32_t remaining = channel->CBR1 & DMA_CBR1_BNDT;
        const uintptr_t source = channel->CSAR;
        if (remaining == 0U)
            return;
        if (source < kSramBegin || source >= kSramEnd ||
            channel->CDAR != reinterpret_cast<uintptr_t>(&model.uart->TDR)) {
            nx_u575_uart_gpdma_fault(DMA_CSR_DTEF, true);
            return;
        }
        const uint8_t byte = *reinterpret_cast<const uint8_t*>(source);
        nx_u575_model_write(&model.uart->TDR, byte);
        if ((channel->CTR1 & DMA_CTR1_SINC) != 0U)
            channel->CSAR++;
        if ((channel->CTR1 & DMA_CTR1_DINC) != 0U)
            channel->CDAR++;
        channel->CBR1 =
            (channel->CBR1 & ~static_cast<uint32_t>(DMA_CBR1_BNDT)) |
            (remaining - 1U);
        model.uart->ISR |= USART_ISR_TXE;
        if (remaining == 1U) {
            channel->CCR &= ~static_cast<uint32_t>(DMA_CCR_EN);
            channel->CSR |= DMA_CSR_TCF | DMA_CSR_IDLEF;
            return;
        }
    }
}

void nx_u575_uart_gpdma_advance_suspended(void) {
    auto& model = g_u575_uart_gpdma;
    if ((model.channel->CCR & (DMA_CCR_EN | DMA_CCR_SUSP)) ==
            (DMA_CCR_EN | DMA_CCR_SUSP) &&
        !model.refuse_suspend)
        model.channel->CSR |= DMA_CSR_SUSPF | DMA_CSR_IDLEF;
}

void nx_u575_uart_gpdma_fault(uint32_t flags, bool idle) {
    auto* channel = g_u575_uart_gpdma.channel;
    channel->CSR |= flags & kFlags;
    if ((flags & (DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF)) != 0U) {
        channel->CCR &= ~static_cast<uint32_t>(DMA_CCR_EN);
        if (idle)
            channel->CSR |= DMA_CSR_IDLEF;
        else
            channel->CSR &= ~static_cast<uint32_t>(DMA_CSR_IDLEF);
    }
    /* A trigger overrun reports a missed trigger; it does not clear EN. */
}

void nx_u575_uart_gpdma_wire_idle(void) {
    g_u575_uart_gpdma.uart->ISR |= USART_ISR_TC | USART_ISR_TXE;
}
