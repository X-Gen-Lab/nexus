/**
 * \file            usb_device_soc_model.h
 * \brief           Host SDK model replaces grouping and clock-store leaves
 * \author          Nexus Team
 */
#ifndef NEXUS_USB_DEVICE_SOC_MODEL_H
#define NEXUS_USB_DEVICE_SOC_MODEL_H
#include "usb_clock_model.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t g_nx_usb_soc_hclk_hz;
#ifdef __cplusplus
}
#endif
#define NX_USB_SOC_IRQ_GROUP() 3U
#define NX_USB_SOC_HCLK_HZ     g_nx_usb_soc_hclk_hz
#endif
