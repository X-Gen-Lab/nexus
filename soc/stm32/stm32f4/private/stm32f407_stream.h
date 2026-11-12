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

/** \brief Reviewed regular triggers; zero preserves the TIM3 TRGO profile. */
typedef enum {
    NX_STM32_ADC_TIM3_TRGO,
    NX_STM32_ADC_TIM1_CH1,
    NX_STM32_ADC_TIM1_CH2,
    NX_STM32_ADC_TIM1_CH3,
    NX_STM32_ADC_TIM3_CH1,
    NX_STM32_ADC_TIM4_CH4,
    NX_STM32_ADC_TIM5_CH1,
    NX_STM32_ADC_TIM5_CH2,
    NX_STM32_ADC_TIM5_CH3,
    NX_STM32_ADC_TIM8_CH1,
    NX_STM32_ADC_TIM8_TRGO
} nx_stm32_adc_trigger_t;

/** \brief Discrete independent ADC scan blocks on an exclusive timer. */
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
    uint8_t
        trigger; /**< Immutable selected source; shared by both DMA modes. */
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

/** \brief Exact RM0090 Table44 routes, without per-instance route storage. */
static inline DMA_Stream_TypeDef*
nx_stm32_adc_dma_memory(const ADC_TypeDef* adc) {
#if !defined(ADC2) && !defined(ADC3)
    (void)adc;
#endif
    return
#if defined(ADC2)
        adc == ADC2 ? DMA2_Stream2 :
#endif
#if defined(ADC3)
        adc == ADC3 ? DMA2_Stream1
                    :
#endif
                    DMA2_Stream4;
}

/** \brief The selected converter has one fixed DMA publisher. */
static inline IRQn_Type nx_stm32_adc_dma_irq(const ADC_TypeDef* adc) {
#if !defined(ADC2) && !defined(ADC3)
    (void)adc;
#endif
    return
#if defined(ADC2)
        adc == ADC2 ? DMA2_Stream2_IRQn :
#endif
#if defined(ADC3)
        adc == ADC3 ? DMA2_Stream1_IRQn
                    :
#endif
                    DMA2_Stream4_IRQn;
}

/** \brief Channel selectors are register encodings, not stream indexes. */
static inline uint32_t nx_stm32_adc_dma_channel(const ADC_TypeDef* adc) {
#if !defined(ADC2) && !defined(ADC3)
    (void)adc;
#endif
    return
#if defined(ADC2)
        adc == ADC2 ? DMA_SxCR_CHSEL_0 :
#endif
#if defined(ADC3)
        adc == ADC3 ? DMA_SxCR_CHSEL_1
                    :
#endif
                    0U;
}

/** \brief Normalize one stream's status before interpreting completion. */
static inline uint32_t
nx_stm32_adc_dma_flags(const nx_stm32_adc_stream_state_t* state) {
    return
#if defined(ADC2)
        state->adc.registers == ADC2 ? (state->dma->LISR >> 16U) & 0x3DU :
#endif
#if defined(ADC3)
        state->adc.registers == ADC3 ? (state->dma->LISR >> 6U) & 0x3DU
                                     :
#endif
                                     state->dma->HISR & 0x3DU;
}

/** \brief W1C only this lane, preserving every neighbouring DMA owner. */
static inline void
nx_stm32_adc_dma_ack(const nx_stm32_adc_stream_state_t* state, uint32_t flags) {
#if defined(ADC2)
    if (state->adc.registers == ADC2)
        state->dma->LIFCR = flags << 16U;
    else
#endif
#if defined(ADC3)
        if (state->adc.registers == ADC3)
        state->dma->LIFCR = flags << 6U;
    else
#endif
        state->dma->HIFCR = flags;
}

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
/** \brief Validate all DMA slots against the complete concrete provider.
 * \note state_bytes includes optional continuous fields beyond the base.
 */
nx_result_t nx_stm32_adc_stream_blocks(nx_stm32_adc_stream_state_t* state,
                                       const nx_stream_t* stream,
                                       size_t state_bytes);
/** \brief Shared private rank/sample register programming, engines stopped. */
void nx_stm32_adc_stream_sequence(nx_stm32_adc_stream_state_t* state);

/**
 * \brief           Initialize only the selected static ADC/TIM3/DMA2 route.
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
 * \note            The selected DMA2 vector only; explicit task stream_service
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
