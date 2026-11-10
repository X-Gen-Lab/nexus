/**
 * \file            stm32f407_usb_device.h
 * \brief           Private actual FS device/48MHz static binding
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_DEVICE_H
#define NEXUS_STM32F407_USB_DEVICE_H
#include "stm32f407_usb_clock.h"
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
nx_result_t nx_stm32_usb_device_initialize(nx_usb_controller_t* controller,
                                           nx_stm32_usb_clock_state_t* clock);
/**
 * \brief Bind OTG_HS to an explicitly owned internal-FS or external-ULPI PHY.
 * \param[in,out] controller: Unborrowed exact endpoint/event storage.
 * \param[in] clock: Explicit gate/PHY clock owner, retained until full stop.
 * \param[in] ulpi: External 60 MHz ULPI when true; internal FS otherwise.
 * \return SUCCESS or failure retaining any partial transport responsibility.
 * \note Privileged cold task after pin/VBUS/PHY power handover. Internal HS
 *       PHY still runs at full speed; ULPI wiring is never inferred.
 */
nx_result_t
nx_stm32_usb_device_hs_initialize(nx_usb_controller_t* controller,
                                  const nx_usb_clock_domain_t* clock,
                                  bool ulpi);
#ifdef __cplusplus
}
#endif
#endif
