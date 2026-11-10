/**
 * \file            stm32f4_adc_pins.h
 * \brief           Shared F407 ADC input-to-pad facts and cold analog setup
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_ADC_PINS_H
#define NEXUS_STM32F4_ADC_PINS_H
#include "stm32f407_provider.h"

/** \brief One DS8626 external analog input, without acquiring a resource. */
typedef struct {
    GPIO_TypeDef* gpio;
    uint8_t pin;
} nx_stm32_adc_pad_t;

/** \brief Pure pad lookup; package and Board authorization remain in facts. */
static inline bool nx_stm32_adc_pad(const ADC_TypeDef* converter,
                                    uint8_t channel,
                                    nx_stm32_adc_pad_t* output) {
    if (output == NULL || channel > 15U ||
        (converter != ADC1 && converter != ADC2 && converter != ADC3))
        return false;
    nx_stm32_adc_pad_t pad;
    if (converter == ADC3) {
        static const uint8_t pads[16] = {
            0x00U, 0x01U, 0x02U, 0x03U, 0x56U, 0x57U, 0x58U, 0x59U,
            0x5AU, 0x53U, 0x20U, 0x21U, 0x22U, 0x23U, 0x54U, 0x55U};
        uint8_t value = pads[channel];
        pad.gpio = value < 0x20U ? GPIOA : value < 0x50U ? GPIOC : GPIOF;
        pad.pin = value & 15U;
    } else {
        pad.gpio = channel < 8U ? GPIOA : channel < 10U ? GPIOB : GPIOC;
        pad.pin = channel < 8U    ? channel
                  : channel < 10U ? (uint8_t)(channel - 8U)
                                  : (uint8_t)(channel - 10U);
    }
    *output = pad;
    return true;
}

/** \brief Configure a leased/powered analog pad without a second snapshot. */
static inline nx_result_t nx_stm32_adc_pin(ADC_TypeDef* converter,
                                           uint8_t channel) {
    if (converter == ADC1 && channel >= 16U && channel <= 18U)
        return NX_SUCCESS;
    nx_stm32_adc_pad_t pad;
    if (!nx_stm32_adc_pad(converter, channel, &pad))
        return NX_ERROR_INVALID;
    return nx_stm32_pin_configure(pad.gpio, pad.pin, 3U, 0U, 0U, false);
}
#endif
