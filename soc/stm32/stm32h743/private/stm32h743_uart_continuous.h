/**
 * \file            stm32h743_uart_continuous.h
 * \brief           Exact HSI64 USART3 double-target receive ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_UART_CONTINUOUS_H
#define NX_STM32H743_UART_CONTINUOUS_H
#include "nexus/io/dma.h"
#include "stm32h743_cold.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Selected DBM owner; finite and default UART contexts never grow.
 * \note A caller supplies at least three equal buffers of at least32bytes.
 *       Two stay FILLING
 *       behind M0AR/M1AR; service replaces an inactive target before publish.
 *       Cache-on buffers own complete exclusive32-byte lines. Cache work
 *       runs outside the IRQ mask and never in ISR. Fixed HSI64, PRESC1,
 *       OVER16,8N1 and TIM2/64 establish the conservative replace deadline.
 *       A missed deadline, uncertain target or no free replacement stops
 *       production. No implicit rearm or storage overwrite is permitted.
 *       Stream/control/wake remain live until RX stop and consumer join.
 */
typedef struct {
    nx_stm32h743_uart_state_t uart;
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* rx;
    DMAMUX_Channel_TypeDef* mux;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_stream_t* stream;
    nx_stream_producer_t producer;
    nx_stream_fill_t fill[3];
    size_t loans;
    nx_time_us_t replace_by;
    uint32_t boundary;
    uint32_t divisor;
    unsigned expected_bank;
    bool retained;
    bool cached;
    bool preparing;
    bool processing;
    bool launched;
    bool pending;
    bool draining;
    bool stopping;
    bool quarantined;
    bool fault;
    bool loss_noted;
    bool domain_lost;
    bool withdrawing;
} nx_stm32h743_uart_continuous_state_t;
extern const nx_uart_ops_t nx_stm32h743_uart_continuous_ops;
/**
 * \brief Cold-admit exact H743 V USART3/DMA1 stream1/request45.
 * \param[in,out] state: Exclusive selected static owner, no implicit buffers.
 * \param[in] clock_hz: Exact fixed HSI64 PCLK1/TIM2 input frequency.
 * \param[in] poll_limit: Nonzero bounded USART acknowledgement budget.
 * \return SUCCESS or error; retained flags preserve cleanup responsibility.
 * \note Privileged unmasked task. IRQ priority5/calls_os=false. TX uses
 *       independent finite IRQ transmission, not continuous TX DMA.
 */
nx_result_t nx_stm32h743_uart_continuous_initialize(
    nx_stm32h743_uart_continuous_state_t* state, uint32_t clock_hz,
    uint32_t poll_limit);
/** \brief Latch a DBM event and mask TC until bounded task service. */
void nx_stm32h743_uart_continuous_dma_irq(
    nx_stm32h743_uart_continuous_state_t* state);
/** \brief Observe IDLE/error facts and independent finite IRQ TX. */
void nx_stm32h743_uart_continuous_uart_irq(
    nx_stm32h743_uart_continuous_state_t* state);
/** \brief Bind exact static DMA resources before this mode initializes.
 * \note Task-only; Platform owns pads/gates and callers retain immutable
 *       configuration, regions and storage through successful public stop.
 */
nx_result_t
nx_stm32h743_uart_continuous_prepare(nx_stm32h743_uart_continuous_state_t* port,
                                     const nx_stm32h743_uart_config_t* config,
                                     const nx_dma_memory_region_t* regions,
                                     size_t region_count);

#ifdef __cplusplus
}
#endif
#endif
