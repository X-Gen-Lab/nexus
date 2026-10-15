/**
 * \file            stm32f407_stream.h
 *
 * \brief           Private fixed UART IRQ blocks and ADC timer DMA block
 *                  storage
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_STREAM_H
#define NEXUS_STM32F407_STREAM_H

#include "stm32f407_dma.h"

/** \brief One IRQ writer fills only the currently reserved caller block. */
typedef struct {
    nx_stm32_uart_state_t uart;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    size_t length;
    uint32_t block_flags;
    bool filling;
    bool stopped;
    bool loss_latched;
} nx_stm32_uart_stream_state_t;

/** \brief Discrete ADC1 scan blocks, DMA2 stream4 channel0 and TIM3 TRGO. */
typedef struct {
    nx_stm32_adc_state_t adc;
    TIM_TypeDef* timer;
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* memory;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    const nx_irq_wake_t* wake;
    nx_time_us_t ready_at;
    uint32_t timer_clock_hz;
    uint32_t adc_clock_hz;
    uint32_t trigger_hz;
    uint32_t block_flags;
    uint16_t prescaler;
    uint16_t period;
    bool filling;
    bool warming;
    bool terminal;
    bool stopping;
    bool fault;
} nx_stm32_adc_stream_state_t;

/**
 * \brief           Optional continuous ADC double-buffer storage.
 * \note            Default finite state and stream slots do not grow. The
 *                  spare loan in base.fill stays retained if a target write or
 *                  drain cannot be proved. Only this provider owns posted.
 */
typedef struct {
    nx_stm32_adc_stream_state_t base;
    nx_stream_producer_t posted;
    nx_stream_fill_t targets[2];
    nx_adc_observer_t* observer;
    nx_result_t fault_result;
    uint8_t expected_bank;
    bool halted;
} nx_stm32_adc_continuous_state_t;

#ifdef __cplusplus
extern "C" {
#endif

extern const nx_uart_ops_t nx_stm32_uart_stream_ops;
extern const nx_adc_ops_t nx_stm32_adc_stream_ops;
extern const nx_adc_ops_t nx_stm32_adc_continuous_ops;
/** \brief Initialize the same reviewed route, with independent optional state.
 *  \param[in,out] state: Fresh exact route and immutable channel/clock facts.
 *  \return Success or the finite cold initializer's checked failure.
 */
nx_result_t
nx_stm32_adc_continuous_initialize(nx_stm32_adc_continuous_state_t* state);
/** \brief Replace only a completed inactive bank, then publish proved memory.
 *  \param[in,out] state: Exact static instance alive through successful stop.
 *  \note Short DMA IRQ work; starvation stops instead of overwriting loans.
 */
void nx_stm32_adc_continuous_irq(nx_stm32_adc_continuous_state_t* state);
/** \brief Shared private complete-scan timing validation; no block admission.
 */
nx_result_t nx_stm32_adc_stream_rate(nx_stm32_adc_stream_state_t* state,
                                     uint32_t hz, uint16_t* prescaler,
                                     uint16_t* period);
/** \brief Shared private all-slot DMA and complete-scan validation. */
nx_result_t nx_stm32_adc_stream_blocks(nx_stm32_adc_stream_state_t* state,
                                       const nx_stream_t* stream);
/** \brief Shared private rank/sample register programming, engines stopped. */
void nx_stm32_adc_stream_sequence(nx_stm32_adc_stream_state_t* state);

/**
 * \brief           Initialize only the selected static ADC1/TIM3/DMA2 route.
 *
 * \param[in,out]   state: Fresh exact registers, channel/sample facts and
 *                  clocks.
 *
 * \return          Success or INVALID/BUSY/UNSUPPORTED before block admission.
 *
 * \note            Assembly owns clocks and reviewed analog pins. Common ADC
 *                  initialization selects PCLK2/4; adc_clock_hz must describe
 *                  it.
 */
nx_result_t nx_stm32_adc_stream_initialize(nx_stm32_adc_stream_state_t* state);
/**
 * \brief           Stop trigger and memory sources before publishing one block.
 *
 * \param[in,out]   state: Exact static instance kept through successful stop.
 *
 * \note            DMA2 stream4 vector only; explicit task stream_service
 *                  resumes the next free block. Trigger gaps are part of this
 *                  mode.
 */
void nx_stm32_adc_stream_irq(nx_stm32_adc_stream_state_t* state);

/**
 * \brief           Initialize UART TX IRQ and explicit block-only RX format.
 *
 * \param[in,out]   state: Static UART registers, IRQ, baud and block-only
 *                  profile.
 *
 * \param[in]       clock_hz: Exact selected UART peripheral clock.
 *
 * \return          Success or INVALID/UNSUPPORTED before a stream buffer loan.
 *
 * \note            RX block storage is supplied by rx_start, never allocated
 *                  here.
 */
nx_result_t nx_stm32_uart_stream_initialize(nx_stm32_uart_stream_state_t* state,
                                            uint32_t clock_hz);
/**
 * \brief           Fill one received byte and latch IDLE/full/loss boundaries.
 *
 * \param[in,out]   state: Exact fixed instance kept alive through successful
 *                  stop.
 *
 * \note            The generated USART1 vector calls this block-mode provider.
 *                  Backpressure never overwrites READY/BORROWED consumer
 *                  storage.
 */
void nx_stm32_uart_stream_irq(nx_stm32_uart_stream_state_t* state);

#ifdef __cplusplus
}
#endif

#endif
