/**
 * \file            stm32f4_adc_encoding.h
 * \brief           Constant encoding of an admitted ADC resolution
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_ADC_ENCODING_H
#define NX_STM32F4_ADC_ENCODING_H

/** \brief Encode the reviewed 6/8/10/12-bit ADC CR1 resolution field.
 * \note Configuration admits the exact supported bit count first. This
 *       constant expression adds no object, register access or runtime work.
 */
#define NX_STM32_ADC_RESOLUTION_CODE(bits) ((12U - (bits)) / 2U)

#endif /* NX_STM32F4_ADC_ENCODING_H */
