/**
 * \file            stm32h743_uart_receive.h
 * \brief           Exact USART3 DMA1 stream1 finite RX block ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_UART_RECEIVE_H
#define NX_STM32H743_UART_RECEIVE_H
#include "nexus/io/dma.h"
#include "stm32h743_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Single normal-mode RX lane with explicit caller-owned block loans.
 * \note Cache-on loans own complete exclusive 32-byte lines. Hardware drain
 *       and post-DMA invalidation precede publication. A refused withdrawal
 *       retains the producer loan; consumers never lose their acquired loans.
 *       Restart is owner-service driven, with possible inter-block gaps.
 */
typedef struct {
    nx_stm32h743_uart_state_t uart;
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* rx;
    DMAMUX_Channel_TypeDef* mux;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    uint32_t boundary;
    bool retained;
    bool cached;
    bool filling;
    bool preparing;
    bool processing;
    bool launched;
    bool draining;
    bool stopping;
    bool quarantined;
    bool gap_error;
} nx_stm32h743_uart_receive_state_t;
extern const nx_uart_ops_t nx_stm32h743_uart_receive_ops;
/**
 * \brief Cold-admit USART3 RX DMA1 stream1, DMAMUX1 channel1/request45.
 * \param[in,out] state: Exclusive exact static instance, no implicit RX ring.
 * \param[in] clock_hz: Selected reviewed fixed APB1 clock.
 * \param[in] poll_limit: Finite UART acknowledgement observation budget.
 * \return Success or error; retained records unsuccessful cleanup.
 * \note Privileged unmasked task; no stream, request or foreign IRQ owner.
 */
nx_result_t
nx_stm32h743_uart_receive_initialize(nx_stm32h743_uart_receive_state_t* state,
                                     uint32_t clock_hz, uint32_t poll_limit);
/** \brief Latch stream1 terminal facts and attempt bounded source withdrawal.
 */
void nx_stm32h743_uart_receive_dma_irq(
    nx_stm32h743_uart_receive_state_t* state);
/** \brief Share ordinary IRQ TX; latch IDLE/error RX boundaries without RDR. */
void nx_stm32h743_uart_receive_uart_irq(
    nx_stm32h743_uart_receive_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
