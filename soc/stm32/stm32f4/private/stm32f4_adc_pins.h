/**
 * \file            stm32f4_adc_pins.h
 * \brief           Selected ADC pad connections and shared cold analog setup
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

#ifndef NEXUS_STM32_ADC_PINS
#error "ADC modes require the selected package pad connections"
#endif

/** \brief Resolve only selected physical inputs, without a second pad map. */
static inline bool nx_stm32_adc_pad(const ADC_TypeDef* converter,
                                    uint8_t channel,
                                    nx_stm32_adc_pad_t* output) {
    if (output == NULL || channel > 15U)
        return false;
#define NX_ADC_PAD(owner, input, port, number)                                 \
    if (converter == (owner) && channel == (input)) {                          \
        output->gpio = (port);                                                 \
        output->pin = (number);                                                \
        return true;                                                           \
    }
    NEXUS_STM32_ADC_PINS(NX_ADC_PAD)
#undef NX_ADC_PAD
    (void)converter;
    return false;
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
