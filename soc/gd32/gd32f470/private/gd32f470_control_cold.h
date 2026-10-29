/**
 * \file            gd32f470_control_cold.h
 * \brief           Static PWM duty and ADC configuration construction in C
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_CONTROL_COLD_H
#define NEXUS_GD32F470_CONTROL_COLD_H
#include "gd32f470_adc_stream.h"
#include "nexus/arch/arch.h"

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact analog sequence and bounded startup wait, without stream state.
 * \note Channels and this immutable configuration outlive the selected owner.
 */
typedef struct {
    const uint8_t* channels;
    size_t channel_count;
    uint32_t reference_mv;
    uint32_t timeout_us;
} nx_gd32_adc_cold_config_t;
/** \brief Only DMA modes retain visibility and optional continuous samples. */
typedef struct {
    nx_gd32_adc_cold_config_t adc;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    const uint8_t* samples;
    uint8_t priority;
} nx_gd32_adc_stream_cold_config_t;

/** \brief Reject an inadmissible CPU context before state or time access. */
static inline bool nx_gd32_control_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Pure config admission before GPIO, ADC calibration or timer writes.
 */
static inline bool
nx_gd32_adc_cold_config_valid(const nx_gd32_adc_cold_config_t* config) {
    return config != NULL && config->channels != NULL &&
           config->channel_count != 0U && config->channel_count <= 16U &&
           config->reference_mv != 0U && config->reference_mv <= 3600U &&
           config->timeout_us != 0U && config->timeout_us <= 60000000U;
}
/** \brief Form the cold absolute deadline in the actual Core time domain.
 * \param[in,out] state: Fresh selected ADC storage, with no active conversion.
 * \param[in] config: Unchanged readonly sequence and startup wait.
 * \return Same checked context, calibration and local rollback as the driver.
 * \note No samples are borrowed by cold construction; no worker is started.
 */
static inline nx_result_t
nx_gd32_adc_poll_cold_prepare(nx_gd32_adc_state_t* state,
                              const nx_gd32_adc_cold_config_t* config) {
    if (!nx_gd32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || !nx_gd32_adc_cold_config_valid(config))
        return NX_ERROR_INVALID;
    return nx_gd32_adc_initialize(
        state, config->channels, config->channel_count, config->reference_mv,
        nx_deadline_after(nx_time_now_us(), config->timeout_us));
}
/** \brief Bind static DMA visibility before the real finite ADC initializer.
 * \param[in,out] state: Fresh exact finite owner; stream storage comes later.
 * \param[in] config: Static analog and DMA facts retained through stop.
 * \return Same actual driver admission and calibration rollback result.
 * \note No DMA target, timer trigger or caller stream is admitted here.
 */
static inline nx_result_t nx_gd32_adc_stream_cold_prepare(
    nx_gd32_adc_stream_state_t* state,
    const nx_gd32_adc_stream_cold_config_t* config) {
    if (!nx_gd32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL ||
        !nx_gd32_adc_cold_config_valid(&config->adc) ||
        config->regions == NULL || config->region_count == 0U ||
        config->priority > 15U)
        return NX_ERROR_INVALID;
    if (state->adc.initialized || state->stream != NULL)
        return NX_ERROR_BUSY;
    state->regions = config->regions;
    state->region_count = config->region_count;
    return nx_gd32_adc_stream_initialize(
        state, config->adc.channels, config->adc.channel_count,
        config->adc.reference_mv,
        nx_deadline_after(nx_time_now_us(), config->adc.timeout_us),
        config->priority);
}
/** \brief Bind continuous sample codes without allocating finite-mode state.
 * \param[in,out] state: Fresh selected continuous owner, without any loan.
 * \param[in] config: Static visibility, sequence and per-channel sample codes.
 * \return Same real continuous constructor and failed-start rollback result.
 * \note Caller supplies all stream blocks later; no hidden capture starts.
 */
static inline nx_result_t nx_gd32_adc_continuous_cold_prepare(
    nx_gd32_adc_continuous_state_t* state,
    const nx_gd32_adc_stream_cold_config_t* config) {
    if (!nx_gd32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL ||
        !nx_gd32_adc_cold_config_valid(&config->adc) ||
        config->regions == NULL || config->region_count == 0U ||
        config->samples == NULL || config->priority > 15U)
        return NX_ERROR_INVALID;
    if (state->base.adc.initialized || state->base.stream != NULL)
        return NX_ERROR_BUSY;
    state->base.regions = config->regions;
    state->base.region_count = config->region_count;
    state->sample_times = config->samples;
    return nx_gd32_adc_continuous_initialize(
        state, config->adc.channels, config->adc.channel_count,
        config->adc.reference_mv,
        nx_deadline_after(nx_time_now_us(), config->adc.timeout_us),
        config->priority);
}
/** \brief Stage a validated fixed duty without enabling waveform output.
 * \param[in,out] state: Fresh unique timer owner, retained until release.
 * \param[in] period: Fixed positive 16-bit period in timer ticks.
 * \param[in] tick_hz: Exact integer divisor of the reviewed 100 MHz timer.
 * \param[in] duty: Staged ticks, from zero through the complete period.
 * \return Actual constructor/set result; invalid duty has no hardware effect.
 * \note Task context and caller serialization follow the concrete PWM driver.
 */
static inline nx_result_t nx_gd32_pwm_cold_prepare(nx_gd32_pwm_state_t* state,
                                                   uint32_t period,
                                                   uint32_t tick_hz,
                                                   uint32_t duty) {
    if (!nx_gd32_control_cold_context())
        return NX_ERROR_CONTEXT;
    if (duty > period)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_gd32_pwm_initialize(state, period, tick_hz);
    if (result == NX_SUCCESS)
        result = nx_gd32_pwm_set(state, period, duty);
    return result;
}
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_GD32F470_CONTROL_COLD_H */
