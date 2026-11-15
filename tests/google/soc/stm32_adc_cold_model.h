/**
 * \file            stm32_adc_cold_model.h
 * \brief           ADC conversion completion at the existing cold boundaries
 * \author          Nexus Team
 */
#ifndef NX_STM32_ADC_COLD_MODEL_H
#define NX_STM32_ADC_COLD_MODEL_H
#include "nexus_config.h"
#include "stm32_control_cold_model.h"
/* ADC uses the existing CPU model without the EXTI IRQ fault callbacks. */
#undef NVIC_SetPriority
#undef NVIC_EnableIRQ
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Complete only the converter selected by the actual C driver. */
void nx_stm32_adc_cold_model_poll(unsigned kind, void* port);
#ifdef __cplusplus
}
#endif
#define NX_STM32_IO_POLL(kind, port)                                           \
    nx_stm32_adc_cold_model_poll((kind), (port))
#endif
