/**
 * \file            stm32f407_adc_group_blocks.h
 * \brief           Optional common-ADC CDR blocks without finite-state growth
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_ADC_GROUP_BLOCKS_H
#define NEXUS_STM32F407_ADC_GROUP_BLOCKS_H
#include "stm32f407_adc_group.h"

/** \brief Static regular CDR configuration and explicit service bound. */
typedef struct {
    nx_stm32_adc_group_config_t adc;
    uint32_t max_service_gap_us;
} nx_stm32_adc_group_blocks_config_t;
/** \brief Only selected blocks providers retain these posted producer tokens.
 * \note The base remains the sole cold ADC123/TIM/DMA owner. The optional
 *       observer belongs to the caller; no sample buffers are embedded here.
 */
typedef struct {
    nx_stm32_adc_group_state_t base;
    const nx_stm32_adc_group_blocks_config_t* config;
    nx_stream_t* stream;
    nx_adc_observer_t* observer;
    nx_stream_producer_t posted;
    nx_stream_fill_t targets[2];
    nx_stream_fill_t replacement;
    nx_time_us_t observed_at;
    uint8_t expected_bank;
    bool replacement_live;
    bool stopping;
    bool history_lost;
} nx_stm32_adc_group_blocks_state_t;
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Prepare the one cold domain without borrowing or starting DMA.
 * \param[in,out] state: Fresh static optional state, retained after failure.
 * \param[in] config: Immutable simultaneous CDR layout and observation bound.
 * \return SUCCESS or refusal retaining exact cold resource responsibility.
 */
nx_result_t nx_stm32_adc_group_blocks_prepare(
    nx_stm32_adc_group_blocks_state_t* state,
    const nx_stm32_adc_group_blocks_config_t* config);
/** \brief Join all producers/consumers before delegating the same cold close.
 * \param[in,out] state: Stable sole owner kept through any refused join.
 * \return SUCCESS only after every memory and observer reference detaches.
 */
nx_result_t
nx_stm32_adc_group_blocks_close(nx_stm32_adc_group_blocks_state_t* state);
/** \brief Latch bounded facts for the same static ADC123/DMA2 owner.
 * \param[in] port: Exact selected face, retained until both vectors join.
 */
void nx_stm32_adc_group_blocks_irq(const nx_adc_group_port_t* port);
/** \brief One optional shared table; finite instances keep their own table. */
extern const nx_adc_group_ops_t nx_stm32_adc_group_blocks_ops;
#ifdef __cplusplus
}
#endif
#endif
