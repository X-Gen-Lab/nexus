/**
 * \file            u575_uart_chain_model.cpp
 * \brief           Independent external GPDMA linear linked-item bus model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_uart_chain_model.h"

uint32_t g_u575_chain_fetches;
bool g_u575_chain_fetch_pending;
bool g_u575_chain_hold_fetch;
bool g_u575_chain_refuse_fetch;

namespace {
constexpr uint32_t kUpdates = DMA_CLLR_UB1 | DMA_CLLR_USA | DMA_CLLR_ULL;
constexpr uintptr_t kBegin = 0x20000000U;
constexpr uintptr_t kEnd = 0x20030000U;
constexpr uintptr_t kNodeBus = kBegin + 0x1000U;
const uint8_t* mapped_nodes;
} /* namespace */

void nx_u575_uart_chain_reset(void) {
    nx_u575_uart_gpdma_model_reset();
    g_u575_chain_fetches = 0U;
    g_u575_chain_fetch_pending = false;
    g_u575_chain_hold_fetch = false;
    g_u575_chain_refuse_fetch = false;
    mapped_nodes = nullptr;
}

uintptr_t nx_u575_uart_chain_node_bus(const void* node) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(node);
    if (node == nullptr || (address >= kBegin && address < kEnd) ||
        address < 0x100000000ULL)
        return address;
    if (mapped_nodes == nullptr ||
        address < reinterpret_cast<uintptr_t>(mapped_nodes) ||
        address >= reinterpret_cast<uintptr_t>(mapped_nodes) + 24U)
        mapped_nodes = static_cast<const uint8_t*>(node);
    return kNodeBus + address - reinterpret_cast<uintptr_t>(mapped_nodes);
}

uint32_t nx_u575_uart_chain_model_read(volatile uint32_t* reg) {
    uint32_t value = nx_u575_uart_gpdma_model_read(reg);
    if (reg == &GPDMA1_Channel0_NS->CSR && g_u575_chain_fetch_pending)
        value &= ~static_cast<uint32_t>(DMA_CSR_IDLEF);
    return value;
}

void nx_u575_uart_chain_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &GPDMA1_Channel0_NS->CCR && g_u575_chain_fetch_pending)
        GPDMA1_Channel0_NS->CSR &= ~static_cast<uint32_t>(DMA_CSR_IDLEF);
    nx_u575_uart_gpdma_model_write(reg, value);
}

void nx_u575_uart_chain_fetch(void) {
    if (!g_u575_chain_fetch_pending || g_u575_chain_hold_fetch)
        return;
    auto* channel = GPDMA1_Channel0_NS;
    const uint32_t link = channel->CLLR;
    const uintptr_t address =
        (channel->CLBAR & DMA_CLBAR_LBA) | (link & DMA_CLLR_LA);
    g_u575_chain_fetch_pending = false;
    if (g_u575_chain_refuse_fetch || (link & ~DMA_CLLR_LA) != kUpdates ||
        address < kBegin || address > kEnd - 12U) {
        nx_u575_uart_gpdma_fault(DMA_CSR_ULEF, true);
        return;
    }
    /* The selected dynamic linked item contains CBR1, CSAR then CLLR. Its
     * register-selection bits belong to the preceding item's CLLR. */
    const auto* words = reinterpret_cast<const uint32_t*>(
        mapped_nodes != nullptr && address >= kNodeBus &&
                address <= kNodeBus + 12U
            ? mapped_nodes + address - kNodeBus
            : reinterpret_cast<const uint8_t*>(address));
    channel->CBR1 = words[0];
    channel->CSAR = words[1];
    channel->CLLR = words[2];
    g_u575_chain_fetches++;
    if ((channel->CBR1 & DMA_CBR1_BNDT) == 0U)
        nx_u575_uart_gpdma_fault(DMA_CSR_USEF, true);
}

void nx_u575_uart_chain_advance(size_t bytes) {
    auto* channel = GPDMA1_Channel0_NS;
    while (bytes != 0U) {
        nx_u575_uart_chain_fetch();
        if (g_u575_chain_fetch_pending || (channel->CCR & DMA_CCR_EN) == 0U ||
            (channel->CCR & DMA_CCR_SUSP) != 0U ||
            (USART1_NS->CR3 & USART_CR3_DMAT) == 0U ||
            (USART1_NS->CR1 & (USART_CR1_UE | USART_CR1_TE)) !=
                (USART_CR1_UE | USART_CR1_TE))
            return;
        const uint32_t remaining = channel->CBR1 & DMA_CBR1_BNDT;
        const uintptr_t source = channel->CSAR;
        if (remaining == 0U || source < kBegin || source >= kEnd ||
            channel->CDAR != reinterpret_cast<uintptr_t>(&USART1_NS->TDR)) {
            nx_u575_uart_gpdma_fault(DMA_CSR_DTEF, true);
            return;
        }
        nx_u575_model_write(&USART1_NS->TDR,
                            *reinterpret_cast<const uint8_t*>(source));
        if ((channel->CTR1 & DMA_CTR1_SINC) != 0U)
            channel->CSAR++;
        channel->CBR1 = remaining - 1U;
        USART1_NS->ISR |= USART_ISR_TXE;
        bytes--;
        if (remaining == 1U) {
            if (channel->CLLR != 0U) {
                g_u575_chain_fetch_pending = true;
                nx_u575_uart_chain_fetch();
            } else {
                channel->CCR &= ~static_cast<uint32_t>(DMA_CCR_EN);
                channel->CSR |= DMA_CSR_TCF | DMA_CSR_IDLEF;
                return;
            }
        }
    }
}
