/**
 * \file            gd32f470_usb_device.h
 * \brief           Private actual FS device/48MHz static binding
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_USB_DEVICE_H
#define NEXUS_GD32F470_USB_DEVICE_H
#include "gd32f470_usb_clock.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Bind the actual internal-PHY FS core after reviewed pin/electrical
 *        handoff and an explicitly started 48MHz clock owner.
 * \note Caller-owned controller/endpoint/ring/clock state outlive successful
 *       stop. Failure may retain hardware and clock responsibility. No Host,
 *       HS, class protocol, Board route or PHY tolerance qualification.
 */
nx_result_t nx_gd32_usb_device_initialize(nx_usb_controller_t* controller,
                                          nx_gd32_usb_clock_state_t* clock);
#ifdef __cplusplus
}
#endif
#endif
