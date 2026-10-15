/**
 * \file            usb_factory_model.h
 * \brief           External generated cold MMIO store failure boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_USB_FACTORY_MODEL_H
#define NEXUS_USB_FACTORY_MODEL_H
#ifdef NX_USB_FACTORY_STM32
#include "stm32f407xx.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Preserve enabled, pending and active cold NVIC observations. */
extern NVIC_Type g_nx_usb_factory_nvic;
#ifdef __cplusplus
}
#endif
#undef NVIC
#define NVIC (&g_nx_usb_factory_nvic)
#endif
typedef __UINT32_TYPE__ nx_usb_factory_register_t;
#ifdef __cplusplus
extern "C" {
#endif
void nx_usb_factory_register_write(volatile nx_usb_factory_register_t* address,
                                   nx_usb_factory_register_t value);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_usb_factory_register_write(&(reg), (nx_usb_factory_register_t)(value))
#endif
