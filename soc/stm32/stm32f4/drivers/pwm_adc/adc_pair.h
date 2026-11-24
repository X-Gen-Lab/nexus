/**
 * \file            adc_pair.h
 * \brief           Granted ADC1/ADC2 injected-simultaneous register operations
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_ADC_PAIR_H
#define NEXUS_STM32F4_ADC_PAIR_H

#include "drivers/ip/av/stm32f407_av.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One injected rank per converter in the domain's ADC1/ADC2 pair.
 * \note The domain grants both readable converter gates and common mode,
 *       owns pins/IRQ/lifecycle and serializes these operations. Sampling times
 *       are equal LL encodings 0..7; channels are reviewed external 0..15.
 *       This leaf owns no common register, clock, IRQ or independent request.
 */
typedef struct {
    ADC_TypeDef* master;
    ADC_TypeDef* slave;
    ADC_Common_TypeDef* common;
    uint8_t channels[2];
    uint8_t sample_times[2];
} nx_stm32_adc_pair_config_t;

/** \brief Validate addresses and the exact selected profile without MMIO.
 * \param[in] config: Stable granted pair descriptor.
 * \return True only for distinct ADC1/ADC2 with equal legal sampling times.
 * \note Pure validation; the caller still proves physical routes and gates.
 */
bool nx_stm32_adc_pair_valid(const nx_stm32_adc_pair_config_t* config);

/** \brief Prepare one right-aligned 12-bit injected rank on each idle ADC.
 * \param[in] config: Granted pair with both gates and simultaneous common mode.
 * \return SUCCESS, INVALID, STATE, BUSY or IO on readback failure.
 * \note Serialized cold context. Both ADON must be clear. A partial write
 *       retains domain cleanup responsibility; this leaf never releases it.
 */
nx_result_t nx_stm32_adc_pair_prepare(const nx_stm32_adc_pair_config_t* config);

/** \brief Enable both converters while leaving every trigger/IRQ closed.
 * \param[in] config: Prepared granted pair in simultaneous injected mode.
 * \param[in] trigger_selector: Master-only ADC_CR2_JEXTSEL encoding.
 * \return SUCCESS, INVALID, STATE, BUSY or IO; partial effects remain owned.
 * \note Serialized cold context. Caller waits the real startup interval and
 *       publishes metadata before enabling master JEOCIE/JEXTEN separately.
 */
nx_result_t nx_stm32_adc_pair_enable(const nx_stm32_adc_pair_config_t* config,
                                     uint32_t trigger_selector);

/** \brief Read one complete simultaneous pair without mixing observations.
 * \param[in] config: Enabled pair with its source/IRQ controlled by the owner.
 * \param[out] out: Two independent uint16 values, unchanged on every failure.
 * \return SUCCESS, BUSY if either completion is absent, or exact error.
 * \note Bounded IRQ context. Once both JEOC exist, acknowledge old JEOC/JSTRT
 *       before reading JDR1, then reject any newly observed conversion/overrun.
 *       No result proves analog aperture precision; it is not a DMA transport.
 */
nx_result_t nx_stm32_adc_pair_read(const nx_stm32_adc_pair_config_t* config,
                                   uint16_t out[2]);

/** \brief Attempt to inhibit both granted converters even after a mode fault.
 * \param[in] config: Both converter gates remain readable by this caller.
 * \return SUCCESS or INVALID/IO; failure keeps the domain's cleanup duty.
 * \note Serialized withdrawal context. Disables trigger/IRQ/ADON and clears
 *       statuses. The domain separately joins IRQ and restores common/gates.
 */
nx_result_t nx_stm32_adc_pair_disable(const nx_stm32_adc_pair_config_t* config);

#ifdef __cplusplus
}
#endif
#endif
