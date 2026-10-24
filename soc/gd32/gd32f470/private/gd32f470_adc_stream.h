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

/** \brief Optional switch-buffer storage; finite objects remain unchanged. */
typedef struct {
    nx_gd32_adc_stream_state_t base;
    nx_stream_producer_t posted;
    nx_stream_fill_t targets[2];
    const uint8_t* sample_times;
    nx_adc_observer_t* observer;
    uint8_t expected_bank;
    bool halted;
} nx_gd32_adc_continuous_state_t;

#ifdef __cplusplus
extern "C" {
#endif
extern const nx_adc_ops_t nx_gd32_adc_stream_ops;
extern const nx_adc_ops_t nx_gd32_adc_continuous_ops;
/** \brief Acquire the existing reviewed route and exact sample-time codes.
 *  \param[in,out] state: Fresh state with domains and stable sample_times.
 *  \param[in] channels: Stable external channel sequence.
 *  \param[in] count: Positive sequence length, at most sixteen.
 *  \param[in] reference_mv: Nominal reference, not physical calibration.
 *  \param[in] deadline: Absolute initial calibration deadline.
 *  \param[in] priority: Unshifted DMA publisher priority.
 *  \return Success or a checked rejection before buffer admission.
 */
nx_result_t
nx_gd32_adc_continuous_initialize(nx_gd32_adc_continuous_state_t* state,
                                  const uint8_t* channels, size_t count,
                                  uint32_t reference_mv, nx_time_us_t deadline,
                                  unsigned priority);
/** \brief Stop and join every loan before releasing unique peripherals.
 *  \param[in,out] state: Same retained static instance.
 *  \return Success or BUSY retaining live storage.
 */
nx_result_t nx_gd32_adc_continuous_stop(nx_gd32_adc_continuous_state_t* state);
/** \brief Replace a completed inactive target before release-publication.
 *  \param[in,out] state: Live exact DMA1 channel0 instance.
 */
void nx_gd32_adc_continuous_irq(nx_gd32_adc_continuous_state_t* state);
/** \brief Shared private exact timer and complete-scan cost validation. */
nx_result_t nx_gd32_adc_stream_timing(size_t channels,
                                      const uint8_t* sample_times, uint32_t hz,
                                      uint16_t* prescaler, uint16_t* reload);
/** \brief Shared private rank programming with explicit or fixed sample time.
 */
void nx_gd32_adc_stream_sequence(const nx_gd32_adc_state_t* adc,
                                 const uint8_t* sample_times);

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
