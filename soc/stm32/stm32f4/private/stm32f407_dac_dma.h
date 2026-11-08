/**
 * \file            stm32f407_dac_dma.h
 * \brief           Exact F407 DAC DMA extension without default CPU cost
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_DAC_DMA_H
#define NEXUS_STM32F407_DAC_DMA_H
#include "nexus/io/dac_tx.h"
#include "nexus/io/dma.h"
#include "stm32f407_dac.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Frozen selected DAC and memory facts; the basic timer is exclusive.
 * \note The base is first so one static controller retains one cold receipt.
 *       CH1/dual uses DMA1 stream5 selector7; CH2 uses stream6 selector7.
 */
typedef struct {
    nx_stm32_dac_config_t dac;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    uint8_t priority;
    bool circular;
    bool kernel_bound;
} nx_stm32_dac_dma_config_t;
/** \brief Only a selected DMA mode owns this finite extension. */
typedef struct {
    nx_stm32_dac_state_t dac;
    nx_dac_tx_request_t* active;
    const nx_irq_wake_t* wake;
    nx_time_us_t tail_deadline;
    nx_time_us_t drain_deadline;
    size_t transferred;
    uint32_t last_frame;
    uint32_t events;
    nx_result_t terminal;
    uint8_t phase;
    uint8_t saved_priorities[2];
    uint8_t vector_leases;
    uint8_t frame_bytes;
    bool clock_added;
    bool route_owned;
    bool initialized;
    bool closing;
} nx_stm32_dac_dma_state_t;
/** \brief Acquire only the selected route; failures keep partial receipts. */
nx_result_t nx_stm32_dac_dma_prepare(nx_stm32_dac_dma_state_t* state,
                                     const nx_stm32_dac_dma_config_t* config);
/** \brief Stop and join TX before reversing only owned clocks and vectors. */
nx_result_t nx_stm32_dac_dma_close(nx_stm32_dac_dma_state_t* state);
/** \brief Fixed DMA/DAC ISR latches facts and hints, without reading frames. */
void nx_stm32_dac_dma_irq(nx_stm32_dac_dma_state_t* state);
extern const nx_dac_ops_t nx_stm32_dac_dma_ops;
extern const nx_dac_tx_ops_t nx_stm32_dac_dma_tx_ops;
#ifdef __cplusplus
}
#endif
#endif
