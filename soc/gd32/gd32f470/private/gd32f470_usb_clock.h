/**
 * \file            gd32f470_usb_clock.h
 * \brief           Caller-owned PLLSAIP 48 MHz and USBFS clock lease
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_USB_CLOCK_H
#define NEXUS_GD32F470_USB_CLOCK_H
#include "nexus/core/time.h"
#include "usb_device_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Selected clock control; zero-initialize, never edit fields.
 * \note            One privileged Thread owner serializes all calls. Saved
 *                  fields restore only owned PLLSAI N/P and CK48 mux bits.
 *                  Failed cleanup retains storage; no default global state.
 */
typedef struct {
    uint32_t saved_pll;
    uint32_t saved_mux;
    uint32_t consumers;
    uint8_t phase;
} nx_gd32_usb_clock_state_t;
extern const nx_usb_clock_ops_t nx_gd32_usb_clock_ops;
/**
 * \brief           Own idle CK48/PLLSAI/USBFS and establish PLLSAIN192/P4.
 * \param[in,out]   state: Zero-initialized storage outliving every consumer.
 * \param[in]       exclusive: Authorization from the single resource owner.
 * \param[in]       deadline: Finite absolute time in the established Core
 * clock. \return          SUCCESS, INVALID, CONTEXT, PERMISSION, BUSY,
 * UNSUPPORTED, TIMEOUT or IO. A failure may retain cleanup ownership. \note
 * Requires the reviewed 25 MHz/M25/N400/P2/Q9 system profile; never changes
 * SYSCLK. USBHS/TRNG/SDIO/TLI and PLLSAI must be idle before mutation.
 * Clock-gated external users also need explicit quiescence and the same
 * resource owner; enabled gates cannot discover them. No IRC48M tolerance
 * assumption.
 */
nx_result_t nx_gd32_usb_clock_start(nx_gd32_usb_clock_state_t* state,
                                    bool exclusive, nx_time_us_t deadline);
/**
 * \brief           Detach USBFS, restore CK48 and stop/read back owned PLLSAI.
 * \param[in,out]   state: Stable owner storage, retained until SUCCESS.
 * \param[in]       deadline: Finite absolute deadline in the same Core domain.
 * \return          SUCCESS, INVALID, CONTEXT, BUSY, TIMEOUT or IO.
 * \note            Consumer count must be zero after actual USB/IRQ detach.
 *                  A new shared owner rejects before mutation. An expired
 *                  deadline still permits immediate readback-proved cleanup;
 *                  failed PLL drain retains ownership for an explicit retry.
 */
nx_result_t nx_gd32_usb_clock_stop(nx_gd32_usb_clock_state_t* state,
                                   nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
