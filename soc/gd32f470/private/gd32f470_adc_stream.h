/**
 * \file            gd32f470_adc_stream.h
 * \brief           Private ADC0 TIMER2-triggered discrete DMA block storage
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_ADC_STREAM_H
#define NEXUS_GD32F470_ADC_STREAM_H

#include "gd32f470_provider.h"
#include "nexus/io/dma.h"

/**
 * \brief           Exact ADC0 / DMA1 channel0 selector0 / TIMER2 instance.
 * \note            Assembly owns DMA1 clock and analog Board wiring. Every
 *                  block stops trigger, conversion and DMA before publication.
 *                  Explicit service resumes acquisition after consumer drain,
 *                  including stabilization and bounded recalibration; gaps are
 *                  expected and no missed-trigger count is invented.
 */
typedef struct {
    nx_gd32_adc_state_t adc;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    const nx_irq_wake_t* wake;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    nx_time_us_t ready_at;
    nx_result_t fault;
    uint16_t prescaler;
    uint16_t reload;
    uint8_t phase;
    bool filling;
    bool completed;
    bool stopping;
} nx_gd32_adc_stream_state_t;

#ifdef __cplusplus
extern "C" {
#endif
extern const nx_adc_ops_t nx_gd32_adc_stream_ops;
/**
 * \brief           Acquire ADC0 and the unique TIMER2 before block admission.
 * \param[in,out]   state: Static state with immutable DMA domains supplied.
 * \param[in]       channels: Stable external channel sequence, zero to fifteen.
 * \param[in]       count: One to sixteen channels in complete-scan order.
 * \param[in]       reference_mv: Declared nominal reference, not calibration.
 * \param[in]       deadline: Absolute deadline for initial ADC calibration.
 * \param[in]       priority: Unshifted DMA publisher priority, zero to fifteen.
 * \return          Success or INVALID/BUSY/CONTEXT/TIMEOUT before block loans.
 * \note            Fixed ADC clock is 12.5 MHz; sample time is fifteen cycles
 *                  plus twelve conversion cycles per channel. Timer source is
 *                  100 MHz. Electrical accuracy and source impedance require
 *                  physical qualification.
 */
nx_result_t nx_gd32_adc_stream_initialize(nx_gd32_adc_stream_state_t* state,
                                          const uint8_t* channels, size_t count,
                                          uint32_t reference_mv,
                                          nx_time_us_t deadline,
                                          unsigned priority);
/**
 * \brief           Join all block loans before releasing ADC and timer clocks.
 * \param[in,out]   state: Same storage retained through every BUSY result.
 * \return          Success proves source detach and consumer drain; BUSY
 *                  retains storage. Shared DMA1 clock belongs to assembly.
 */
nx_result_t nx_gd32_adc_stream_stop(nx_gd32_adc_stream_state_t* state);
/**
 * \brief           Latch selected DMA facts and publish only quiescent blocks.
 * \param[in,out]   state: Generated static state for DMA1 channel0 vector.
 * \note            No wait, implicit rearm or consumer overwrite occurs.
 */
void nx_gd32_adc_stream_irq(nx_gd32_adc_stream_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
