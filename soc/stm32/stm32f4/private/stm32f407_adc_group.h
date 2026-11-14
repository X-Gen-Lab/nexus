/**
 * \file            stm32f407_adc_group.h
 * \brief           Sole F407 common-ADC owner for finite integer groups
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_ADC_GROUP_H
#define NEXUS_STM32F407_ADC_GROUP_H
#include "nexus/io/adc_group.h"
#include "nexus/io/dma.h"
#include "stm32f407_provider.h"
#include "stm32f4_adc_encoding.h"

/** \brief The selected common sequencer; never inferred from sample count. */
typedef enum {
    NX_STM32_ADC_DUAL_REGULAR,
    NX_STM32_ADC_TRIPLE_REGULAR,
    NX_STM32_ADC_DUAL_INTERLEAVED,
    NX_STM32_ADC_TRIPLE_INTERLEAVED,
    NX_STM32_ADC_SINGLE_INJECTED,
    NX_STM32_ADC_DUAL_INJECTED,
    NX_STM32_ADC_TRIPLE_INJECTED
} nx_stm32_adc_group_mode_t;

/** \brief Static rank-major converter order and exact physical resource facts.
 * \note All three converters/common registers are exclusively leased even in
 *       dual mode. Repeated interleaved triples form one six-sample frame;
 *       all other formats contain one sequence. No data repacking occurs.
 */
typedef struct {
    ADC_TypeDef* adc[3];
    ADC_Common_TypeDef* common;
    TIM_TypeDef* timer;
    DMA_TypeDef* dma;
    DMA_Stream_TypeDef* memory;
    RCC_TypeDef* rcc;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    const uint8_t* channels;
    const uint8_t* sample_times;
    const uint8_t* converters;
    uint32_t adc_clock_hz;
    uint32_t timer_clock_hz;
    uint32_t period_us;
    uint32_t trigger_identity;
    uint32_t drain_timeout_us;
    uint16_t lower;
    uint16_t upper;
    uint8_t ranks;
    uint8_t resolution;
    uint8_t trigger;
    uint8_t delay_cycles;
    uint8_t priority;
    nx_stm32_adc_group_mode_t mode;
    bool watchdog;
} nx_stm32_adc_group_config_t;

/** \brief Only selected instances retain runtime state; no sample storage. */
typedef struct {
    const nx_stm32_adc_group_config_t* config;
    nx_adc_group_request_t* active;
    nx_time_us_t warm_until;
    nx_time_us_t drain_deadline;
    uint64_t time_epoch;
    uint32_t saved_common;
    uint32_t gates_added;
    volatile uint32_t events;
    nx_result_t terminal;
    uint16_t prescaler;
    uint16_t period;
    uint8_t effects;
    uint8_t phase;
    bool initialized;
    bool closing;
    bool fault;
} nx_stm32_adc_group_state_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Validate facts, acquire idle resources and leave every source off.
 * \param[in,out] state: Zeroed or fully closed caller-owned static storage.
 * \param[in] config: Immutable static facts retained through joined close.
 * \return SUCCESS, pure admission failure, or IO with a retained receipt.
 * \note Privileged serialized task, interrupts enabled. Platform has already
 *       acquired authorized pads and the shared DMA clock. No sampling starts.
 */
nx_result_t
nx_stm32_adc_group_prepare(nx_stm32_adc_group_state_t* state,
                           const nx_stm32_adc_group_config_t* config);
/** \brief Drain request/IRQs, restore common facts and release acquired gates.
 * \param[in,out] state: Same live owner, including partial preparation.
 * \return SUCCESS only after full withdrawal; failure retains responsibility.
 * \note Same task context as prepare. Timeout cannot revoke payload borrowing.
 */
nx_result_t nx_stm32_adc_group_close(nx_stm32_adc_group_state_t* state);
/** \brief Latch actual ADC/DMA facts without touching request or sample data.
 * \param[in] port: Same static face, valid until every vector is joined.
 * \note Called by either exact selected vector; no kernel calls or callback.
 */
void nx_stm32_adc_group_irq(const nx_adc_group_port_t* port);
/** \brief Shared immutable finite group methods. */
extern const nx_adc_group_ops_t nx_stm32_adc_group_ops;
#ifdef __cplusplus
}
#endif
#endif
