/**
 * \file            connectivity_host_transport.h
 * \brief           Exact F105/F107 SDK contract for shared DWC FS Host.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_CONNECTIVITY_HOST_TRANSPORT_H
#define NEXUS_STM32F1_CONNECTIVITY_HOST_TRANSPORT_H
#ifndef USE_HAL_HCD_REGISTER_CALLBACKS
#define USE_HAL_HCD_REGISTER_CALLBACKS 1U
#endif
#include "stm32f1xx_hal_hcd.h"

typedef enum {
    NX_STM32_USB_FS_INTERNAL,
    NX_STM32_USB_HS_INTERNAL_FS,
    NX_STM32_USB_HS_ULPI
} nx_stm32_usb_phy_t;

#include "usb_host_transport.h"

#ifdef __cplusplus
extern "C" {
#endif
bool nx_stm32f1_usb_host_clock_ready(void);
bool nx_stm32f1_usb_host_power_ready(void);
#ifdef __cplusplus
}
#endif

#endif
