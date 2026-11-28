/**
 * \file            stm32f407_usb_host.h
 * \brief           F4 SDK and PHY selection for shared DWC Host transport.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_HOST_H
#define NEXUS_STM32F407_USB_HOST_H
#include "drivers/usb/stm32f407_usb_phy.h"
#ifndef USE_HAL_HCD_REGISTER_CALLBACKS
#define USE_HAL_HCD_REGISTER_CALLBACKS 1U
#endif
#include "stm32f4xx_hal_hcd.h"

#include "../../../common/dwc_otg_host/usb_host_transport.h"

#endif
