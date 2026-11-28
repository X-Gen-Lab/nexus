/**
 * \file            instance.h
 * \brief           Private STM32F103xB ADC1 regular single-shot owner
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_ADC_INSTANCE_H
#define NX_STM32F1_ADC_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/adc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Zero-initialized owner of the dedicated PA0 analog and ADC1 route. */
typedef struct {
    nx_stm32f1_pads_t pads;
    uint16_t reference_mv;
    uint8_t conversion_ticks;
    uint8_t effects;
} nx_stm32f1_adc_t;

extern const nx_adc_ops_t nx_stm32f1_adc_ops;

/** \brief Copy selected physical format for either ADC1 acquisition mode. */
nx_result_t nx_stm32f1_adc_info(const void* context, nx_adc_info_t* output,
                                const nx_adc_ops_t* ops);
nx_result_t nx_stm32f1_adc_sequence_info(const void* context,
                                         nx_adc_sequence_info_t* output,
                                         const nx_adc_ops_t* ops);

/** \brief Acquire and calibrate ADC1 under HSI8 with APB2/1, ADC /2.
 * \param[in,out] state: Single zero-initialized cold owner.
 * \param[in] apb2_hz: Declared APB2 rate; must match the fixed HSI8 route.
 * \param[in] reference_mv: Declared LQFP VDDA/VREF+ in millivolts.
 * \param[in] sample_code: Reviewed STM32F103 ADC sample-time encoding.
 * \note Privileged unmasked Thread. Retain a failed acquisition receipt until
 * close completes.
 */
nx_result_t nx_stm32f1_adc_prepare(nx_stm32f1_adc_t* state, uint32_t apb2_hz,
                                   uint32_t reference_mv, uint32_t sample_code);

/** \brief Prove ADC power-down before restoring the PA0 and clock owners.
 * \param[in,out] state: Same owner after joining the polling executor.
 * \note Privileged unmasked Thread. A failed stop retains its resources.
 */
nx_result_t nx_stm32f1_adc_close(nx_stm32f1_adc_t* state);

#ifdef __cplusplus
}
#endif
#endif
