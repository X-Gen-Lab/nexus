/**
 * \file            stm32u575_uart_gpdma_chain.h
 * \brief           Private finite USART1 GPDMA hardware-chain construction
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32U575_UART_GPDMA_CHAIN_H
#define NX_STM32U575_UART_GPDMA_CHAIN_H

#include "stm32u575_uart_gpdma.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Packed dynamic node: registers are fetched in increasing order.
 * \note RM0456 GPDMA linear channel: UB1/USA/ULL select exactly these words.
 *       No repeated block, circular link, scatter/gather or payload copy.
 */
typedef struct {
    uint32_t block;
    uint32_t source;
    uint32_t link;
} nx_stm32u575_uart_chain_node_t;

/** \brief Selected readonly facts; base must remain the first member.
 * \note nodes references exactly two private mutable nodes aligned to 32 bytes
 *       in nonsecure SRAM1. The 24 byte span therefore cannot cross a 64 KiB
 * CLBAR window. Construction and every admission check the actual linked span.
 *       Owner/facts/nodes remain live until complete checked Platform stop.
 */
typedef struct {
    nx_stm32u575_uart_gpdma_facts_t base;
    nx_stm32u575_uart_chain_node_t* nodes;
} nx_stm32u575_uart_chain_facts_t;

extern const nx_uart_ops_t nx_stm32u575_uart_chain_ops;

/** \brief Cold TZEN0/MSIS4 USART1 finite hardware-chain owner.
 * \param[in,out] port: Same private prefix; facts points to chain facts.base.
 * \param[in] clock_hz: Existing 4 MHz PCLK profile.
 * \param[in] poll_limit: Positive bounded cold UART acknowledgement budget.
 * \return Same retained-effect initialization contract as the finite provider.
 * \note Public requests borrow one contiguous 1..196605 byte SRAM1 source.
 *       Up to three 65535 byte blocks use at most two hardware-fetched nodes.
 *       The full request, source and private nodes stay owned until actual
 *       channel/fetch/IRQ drain and independently observed USART.TC. Cancel or
 *       timeout never revokes a loan; failed drain remains QUARANTINED.
 *       DMAT remains high across every request under ES0499 2.21.2.
 */
nx_result_t
nx_stm32u575_uart_chain_initialize(nx_stm32u575_uart_gpdma_state_t* port,
                                   uint32_t clock_hz, uint32_t poll_limit);

/** \brief Latch actual channel facts; settlement stays in owner service. */
void nx_stm32u575_uart_chain_irq(nx_stm32u575_uart_gpdma_state_t* port);
/** \brief Independent RX and USART wire facts; never ordinary IRQ TX. */
void nx_stm32u575_uart_chain_uart_irq(nx_stm32u575_uart_gpdma_state_t* port);

#ifdef __cplusplus
}
#endif
#endif
