/**
 * \file            stm32u575_uart_gpdma.h
 * \brief           Private USART1 finite GPDMA1_CH0 owner contract
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32U575_UART_GPDMA_H
#define NX_STM32U575_UART_GPDMA_H

#include "stm32u575_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Immutable exact-route facts; generated in read-only storage.
 * \note First mode admits USART1/GPDMA1/channel0/request25/IRQ29 only.
 *       A channel identity is not a request selector or a PCB pin route.
 */
typedef struct {
    DMA_TypeDef* engine;
    DMA_Channel_TypeDef* channel;
    IRQn_Type irq;
    uint8_t request;
    uint8_t priority;
} nx_stm32u575_uart_gpdma_facts_t;

/** \brief Immutable cold assembly; no descriptor is retained at runtime.
 * \note Facts and RX storage outlive the owner. The descriptor itself is read
 *       only during prepare; it must not alias mutable state or RX storage.
 */
typedef struct {
    const nx_stm32u575_uart_gpdma_facts_t* facts;
    uint8_t* rx_storage;
    size_t rx_capacity;
    uint32_t baud;
    nx_irq_policy_t irq_policy;
    uint32_t clock_hz;
    uint32_t poll_limit;
} nx_stm32u575_uart_gpdma_config_t;

/** \brief Selected-only extension, one UART hardware owner and wake sink.
 * \note The ordinary IRQ-byte state/ops are unchanged. The prefix owns IRQ RX
 *       metadata but ordinary byte TX IRQ/service must not process DMA TX.
 *       uart.retained marks every partial effect, including a DMA-only failure.
 *       Only a proved whole stop clears it; a stale stopped object writes none.
 *       observed_flags monotonically preserves DMA errors until settlement.
 *       No HAL handle, linked-list descriptor, cache hook or runtime registry.
 */
typedef struct {
    nx_stm32u575_uart_state_t uart;
    const nx_stm32u575_uart_gpdma_facts_t* facts;
    nx_irq_policy_t dma_irq_policy;
    uint32_t observed_flags;
    bool dma_complete;
    bool aborting;
    bool clock_added;
    uint8_t effects;
} nx_stm32u575_uart_gpdma_state_t;

extern const nx_uart_ops_t nx_stm32u575_uart_gpdma_ops;

/** \brief Bind a cold owner and prepare its exact USART1 pins and hardware.
 * \param[in,out] port: Serialized privileged task owner without retained loans.
 * \param[in] config: Explicit resources, byte ring and finite ACK budget.
 * \return Software rejection leaves binding unchanged. Hardware failure may
 *         retain effects and requires the ordinary checked stop path.
 * \note Platform owns the enclosing pin lease. Keep it until checked whole
 *       stop; prepare does not revoke a request or an outstanding publisher.
 */
nx_result_t
nx_stm32u575_uart_gpdma_prepare(nx_stm32u575_uart_gpdma_state_t* port,
                                const nx_stm32u575_uart_gpdma_config_t* config);

/** \brief Cold exclusive TZEN0 USART1 owner with persistent DMAT request gate.
 * \param[in,out] port: Static state, exact facts and disjoint byte RX ring.
 * \param[in] clock_hz: Existing fixed PCLK4MHz profile only.
 * \param[in] poll_limit: Positive bounded ACK/readback phase budget.
 * \return Success, or an error with uart.retained/closing when cleanup remains.
 * \note Frame is8N1, USART FIFO disabled. DMAT becomes1 before the first UE
 *       enable and stays1 across every request/cancel/reset (ES0499 2.21.2).
 *       Direct admission validates the entire source within the actual
 *       nonsecure SRAM1 interval [0x20000000,0x20030000),1..65535 bytes.
 *       Acquire/read back GPDMA clock accessibility before GPDMA MMIO; all
 *       pre-effect software checks precede that retained clock acquisition.
 *       Every ACCEPTED descriptor/source remains borrowed until SETTLED after
 *       engine/reset/IRQ/wake withdrawal AND independently observed USART.TC.
 *       GPDMA EN write0 is ignored: active withdrawal requires SUSP, fresh
 *       SUSPF+IDLEF and a legal RESET followed by EN0/IDLE readback.
 *       Whole stop first joins RX and observes UE0/TEACK0/REACK0, then clears
 *       DMAT. Default request cancellation never cycles the receiver's UE.
 *       No linked list, low-power operation or TrustZone broker is admitted.
 */
nx_result_t
nx_stm32u575_uart_gpdma_initialize(nx_stm32u575_uart_gpdma_state_t* port,
                                   uint32_t clock_hz, uint32_t poll_limit);

/** \brief Latch CH0 facts; never settle or clear DMAT in configurable IRQ. */
void nx_stm32u575_uart_gpdma_irq(nx_stm32u575_uart_gpdma_state_t* port);

/** \brief Latch independent RX/USART.TC facts; no ordinary IRQ-TX execution. */
void nx_stm32u575_uart_gpdma_uart_irq(nx_stm32u575_uart_gpdma_state_t* port);

#ifdef __cplusplus
}
#endif
#endif
