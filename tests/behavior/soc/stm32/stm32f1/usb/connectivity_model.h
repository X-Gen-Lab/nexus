/**
 * \file            connectivity_model.h
 * \brief           Connectivity clock, GPIO and shared USB register model.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_STM32F1_CONNECTIVITY_DWC_MODEL_H
#define NEXUS_STM32F1_CONNECTIVITY_DWC_MODEL_H

#include "stm32f1xx.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "usb_device_access.h"

#include "usb_device.h"
extern RCC_TypeDef nx_f1_dwc_model_rcc;
extern GPIO_TypeDef nx_f1_dwc_model_gpio;
extern nx_native_usb_device_t nx_f1_dwc_model;
void nx_f1_dwc_model_bsrr(GPIO_TypeDef* registers, uint32_t value);
bool nx_f1_dwc_model_irq_busy(void);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef GPIOA
#define RCC                              (&nx_f1_dwc_model_rcc)
#define GPIOA                            (&nx_f1_dwc_model_gpio)
#define NX_STM32F1_DWC_BASE              ((uintptr_t)&nx_f1_dwc_model.hardware)
#define NX_STM32F1_DWC_IRQ_BUSY()        nx_f1_dwc_model_irq_busy()
#define NX_STM32F1_DWC_IRQ_GROUP()       3U
#define NX_STM32F1_GPIO_BSRR(reg, value) nx_f1_dwc_model_bsrr((reg), (value))
#endif
