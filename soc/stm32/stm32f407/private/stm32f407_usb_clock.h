/**
 * \file            stm32f407_usb_clock.h
 * \brief           Caller-owned fixed-profile USBFS clock lease
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_CLOCK_H
#define NEXUS_STM32F407_USB_CLOCK_H
#include "nexus/core/time.h"
#include "usb_device_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Optional clock control; zero-initialize, never edit fields.
 * \note            One privileged Thread owner serializes all methods. No
 *                  global registry or state exists when USB is unselected.
 */
typedef struct {
    uint32_t consumers;
    uint8_t phase;
} nx_stm32_usb_clock_state_t;
extern const nx_usb_clock_ops_t nx_stm32_usb_clock_ops;
/**
 * \brief           Validate the established 8 MHz/M8/N336/Q7 PLL and own USBFS.
 * \param[in,out]   state: Zero-initialized storage outliving every consumer.
 * \param[in]       exclusive: Explicit USBFS gate ownership authorization.
 * \param[in]       deadline: Finite absolute deadline in the running Core
 * clock. \return          SUCCESS, INVALID, CONTEXT, PERMISSION, BUSY,
 * UNSUPPORTED, TIMEOUT or IO. A failed rollback retains cleanup ownership.
 * \note            Never changes the shared PLL. Actual crystal tolerance and
 *                  USB waveforms require physical qualification. Cold only.
 */
nx_result_t nx_stm32_usb_clock_start(nx_stm32_usb_clock_state_t* state,
                                     bool exclusive, nx_time_us_t deadline);
/**
 * \brief           Disable and read back the owned gate after all consumers.
 * \param[in,out]   state: Stable owner storage; failed cleanup retains it.
 * \param[in]       deadline: Finite absolute time in the unchanged Core domain.
 * \return          SUCCESS, INVALID, CONTEXT, BUSY or IO.
 * \note            Consumers release only after actual USB/IRQ detachment.
 *                  Release is not evidence of detachment by itself. No shared
 *                  PLL is stopped. An already idle owner succeeds.
 */
nx_result_t nx_stm32_usb_clock_stop(nx_stm32_usb_clock_state_t* state,
                                    nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
