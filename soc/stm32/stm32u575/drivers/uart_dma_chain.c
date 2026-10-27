/**
 * \file            uart_dma_chain.c
 * \brief           U575 finite UART hardware-linked DMA blocks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_uart_gpdma_chain.h"
#include <limits.h>

#define CHAIN_BYTES      (2U * sizeof(nx_stm32u575_uart_chain_node_t))
#define CHAIN_UPDATES    (DMA_CLLR_UB1 | DMA_CLLR_USA | DMA_CLLR_ULL)
#define CHAIN_SRAM_BEGIN ((uintptr_t)0x20000000U)
#define CHAIN_SRAM_END   ((uintptr_t)0x20030000U)
#ifndef NX_U575_CHAIN_NODE_BUS
#define NX_U575_CHAIN_NODE_BUS(node) ((uintptr_t)(node))
#endif

_Static_assert(sizeof(nx_stm32u575_uart_chain_node_t) == 12U,
               "GPDMA linear packed node must contain three 32 bit registers");
_Static_assert(offsetof(nx_stm32u575_uart_chain_facts_t, base) == 0U,
               "Private finite route facts must be the readonly prefix");

static const nx_stm32u575_uart_chain_facts_t*
chain_facts(const nx_stm32u575_uart_gpdma_state_t* s) {
    return (const nx_stm32u575_uart_chain_facts_t*)s->facts;
}

static bool chain_overlap(const void* a, size_t as, const void* b, size_t bs) {
    return b != NULL && bs != 0U && (uintptr_t)a < (uintptr_t)b + bs &&
           (uintptr_t)b < (uintptr_t)a + as;
}

static bool chain_protected(const nx_stm32u575_uart_gpdma_state_t* s,
                            const void* data, size_t bytes) {
    if (s->facts == NULL)
        return true;
    const nx_stm32u575_uart_chain_facts_t* facts = chain_facts(s);
    if (facts->nodes == NULL)
        return true;
    return chain_overlap(data, bytes, facts, sizeof(*facts)) ||
           chain_overlap(data, bytes, facts->nodes, CHAIN_BYTES) ||
           chain_overlap(data, bytes,
                         (const void*)NX_U575_CHAIN_NODE_BUS(facts->nodes),
                         CHAIN_BYTES);
}

static bool chain_disjoint(const nx_stm32u575_uart_gpdma_state_t* s,
                           const void* nodes) {
    const nx_stm32u575_uart_chain_facts_t* facts = chain_facts(s);
    return !chain_overlap(nodes, CHAIN_BYTES, s, sizeof(*s)) &&
           !chain_overlap(nodes, CHAIN_BYTES, facts, sizeof(*facts)) &&
           !chain_overlap(nodes, CHAIN_BYTES, s->uart.rx_storage,
                          s->uart.rx_capacity) &&
           !chain_overlap(nodes, CHAIN_BYTES, &g_nx_stm32u575_system,
                          sizeof(g_nx_stm32u575_system));
}

static bool chain_layout(const nx_stm32u575_uart_gpdma_state_t* s) {
    const nx_stm32u575_uart_chain_facts_t* facts = chain_facts(s);
    if (facts == NULL || facts->nodes == NULL ||
        (uintptr_t)facts->nodes % 32U != 0U)
        return false;
    const uintptr_t begin = NX_U575_CHAIN_NODE_BUS(facts->nodes);
    /*32 byte alignment puts this24 byte private array wholly in one 64 KiB
     * CLBAR window, without a 64 KiB linker alignment reservation. */
    if (begin < CHAIN_SRAM_BEGIN || begin >= CHAIN_SRAM_END ||
        begin % 32U != 0U || CHAIN_BYTES > CHAIN_SRAM_END - begin ||
        (begin >> 16U) != ((begin + CHAIN_BYTES - 1U) >> 16U))
        return false;
    return chain_disjoint(s, facts->nodes) &&
           chain_disjoint(s, (const void*)begin) &&
           !chain_overlap(s->uart.rx_storage, s->uart.rx_capacity, facts,
                          sizeof(*facts));
}

static bool chain_cold_layout(const nx_stm32u575_uart_gpdma_state_t* s,
                              const nx_stm32u575_uart_gpdma_config_t* config) {
    const nx_stm32u575_uart_chain_facts_t* facts =
        (const nx_stm32u575_uart_chain_facts_t*)config->facts;
    if (facts->nodes == NULL || (uintptr_t)facts->nodes % 32U != 0U ||
        (uintptr_t)facts->nodes > UINTPTR_MAX - CHAIN_BYTES)
        return false;
    const uintptr_t begin = NX_U575_CHAIN_NODE_BUS(facts->nodes);
    if (begin < CHAIN_SRAM_BEGIN || begin >= CHAIN_SRAM_END ||
        begin % 32U != 0U || CHAIN_BYTES > CHAIN_SRAM_END - begin ||
        (begin >> 16U) != ((begin + CHAIN_BYTES - 1U) >> 16U))
        return false;
    const void* spans[2] = {facts->nodes, (const void*)begin};
    for (unsigned i = 0U; i < 2U; ++i) {
        if (chain_overlap(spans[i], CHAIN_BYTES, s, sizeof(*s)) ||
            chain_overlap(spans[i], CHAIN_BYTES, config, sizeof(*config)) ||
            chain_overlap(spans[i], CHAIN_BYTES, facts, sizeof(*facts)) ||
            chain_overlap(spans[i], CHAIN_BYTES, config->rx_storage,
                          config->rx_capacity) ||
            chain_overlap(spans[i], CHAIN_BYTES, &g_nx_stm32u575_system,
                          sizeof(g_nx_stm32u575_system)))
            return false;
    }
    return true;
}

static uint32_t chain_address(const nx_stm32u575_uart_chain_node_t* node) {
    return (uint32_t)NX_U575_CHAIN_NODE_BUS(node) & DMA_CLLR_LA;
}

static uint32_t chain_link(const nx_stm32u575_uart_gpdma_state_t* s,
                           const nx_uart_tx_request_t* request) {
    return request->length > UINT16_MAX
               ? chain_address(chain_facts(s)->nodes) | CHAIN_UPDATES
               : 0U;
}

static bool chain_prepare(nx_stm32u575_uart_gpdma_state_t* s,
                          const nx_uart_tx_request_t* request) {
    nx_stm32u575_uart_chain_node_t* nodes = chain_facts(s)->nodes;
    size_t remaining = request->length;
    const size_t first = remaining > UINT16_MAX ? UINT16_MAX : remaining;
    remaining -= first;
    uintptr_t source = (uintptr_t)request->data + first;
    for (unsigned i = 0U; i < 2U; ++i) {
        const size_t block = remaining > UINT16_MAX ? UINT16_MAX : remaining;
        nodes[i].block = (uint32_t)block;
        nodes[i].source = block != 0U ? (uint32_t)source : 0U;
        remaining -= block;
        source += block;
        nodes[i].link = remaining != 0U
                            ? chain_address(&nodes[i + 1U]) | CHAIN_UPDATES
                            : 0U;
    }
    /* Source/request were admitted first. No descriptor may become visible
     * to the enabled DMA until all packed words are published. */
    nx_arch_dmb();
    return remaining == 0U;
}

static bool chain_clear_register(volatile uint32_t* reg) {
    NX_U575_WRITE(reg, 0U);
    nx_arch_dsb();
    return NX_U575_READ(reg) == 0U;
}

static bool chain_withdraw(nx_stm32u575_uart_gpdma_state_t* s) {
    DMA_Channel_TypeDef* channel = s->facts->channel;
    /* The common engine owner has already proved fresh EN0/IDLE/FIFO0,
     * legal suspend/reset where required, flag ACK and vector withdrawal.
     * Only now can it sever every future link/source reference. A refused
     * detach keeps the public request and all private descriptor loans. */
    if (!chain_clear_register(&channel->CLLR) ||
        !chain_clear_register(&channel->CLBAR) ||
        !chain_clear_register(&channel->CSAR))
        return false;
    nx_stm32u575_uart_chain_node_t* nodes = chain_facts(s)->nodes;
    for (unsigned i = 0U; i < 2U; ++i) {
        nodes[i].block = nodes[i].source = nodes[i].link = 0U;
    }
    nx_arch_dmb();
    return true;
}

#define NX_U575_GPDMA_MAX_BYTES ((size_t)3U * UINT16_MAX)
#define NX_U575_GPDMA_PROTECTED(s, data, bytes)                                \
    chain_protected((s), (data), (bytes))
#define NX_U575_GPDMA_LAYOUT_VALID(s)     chain_layout(s)
#define NX_U575_GPDMA_PREPARE(s, request) chain_prepare((s), (request))
#define NX_U575_GPDMA_BASE(s)                                                  \
    ((uint32_t)NX_U575_CHAIN_NODE_BUS(chain_facts(s)->nodes) & DMA_CLBAR_LBA)
#define NX_U575_GPDMA_LINK(s, request) chain_link((s), (request))
#define NX_U575_GPDMA_COUNT(request)                                           \
    ((uint32_t)((request)->length > UINT16_MAX ? UINT16_MAX                    \
                                               : (request)->length))
#define NX_U575_GPDMA_TCEM                  DMA_CTR2_TCEM
#define NX_U575_GPDMA_WITHDRAW(s)           chain_withdraw(s)
#define NX_U575_GPDMA_FACTS_BYTES           sizeof(nx_stm32u575_uart_chain_facts_t)
#define NX_U575_GPDMA_COLD_VALID(s, config) chain_cold_layout((s), (config))
#define nx_stm32u575_uart_gpdma_prepare     nx_stm32u575_uart_chain_prepare
#define nx_stm32u575_uart_gpdma_initialize  nx_stm32u575_uart_chain_initialize
#define nx_stm32u575_uart_gpdma_irq         nx_stm32u575_uart_chain_irq
#define nx_stm32u575_uart_gpdma_uart_irq    nx_stm32u575_uart_chain_uart_irq
#include "stm32u575_uart_gpdma_impl.h"

const nx_uart_ops_t nx_stm32u575_uart_chain_ops = {
    submit,     start_admitted, cancel,      service, NULL,
    read_bytes, stop,           attach_wake, NULL,    NULL};
