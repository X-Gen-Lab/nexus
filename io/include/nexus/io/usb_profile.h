/**
 * \file            usb_profile.h
 * \brief           Selected-only immutable USB facts
 * \author          Nexus Team
 * \note            Selection facts are not observed physical qualification.
 */
#ifndef NEXUS_IO_USB_PROFILE_H
#define NEXUS_IO_USB_PROFILE_H
#include "nexus/io/usb_device.h"
#ifdef __cplusplus
extern "C" {
#endif
enum nx_usb_fs_limit {
    NX_USB_FS_PHY_CLOCK_HZ = 48000000U,
    NX_USB_FS_FIFO_WORDS = 320U,
    NX_USB_FS_ENDPOINT_NUMBERS = 4U,
    NX_USB_FS_MAX_PACKET = 64U
};
typedef enum { NX_USB_PROFILE_DEVICE } nx_usb_profile_role_t;
typedef enum { NX_USB_PROFILE_FULL_SPEED } nx_usb_profile_speed_t;
typedef enum { NX_USB_PROFILE_INTERNAL_FS } nx_usb_profile_phy_t;
typedef enum { NX_USB_PROFILE_PIO } nx_usb_profile_engine_t;
typedef enum {
    NX_USB_PROFILE_CONTROL,
    NX_USB_PROFILE_BULK
} nx_usb_profile_transfer_t;
typedef struct {
    const nx_usb_endpoint_port_t* port;
    uint16_t max_packet;
    uint8_t number;
    bool in;
    nx_usb_profile_transfer_t transfer;
} nx_usb_endpoint_profile_t;
/** \brief Optional epoch-bound EP0 rejection, separate from generic ops. */
typedef struct {
    nx_result_t (*reject)(void*, uint64_t);
} nx_usb_setup_decision_ops_t;
typedef struct {
    const nx_usb_setup_decision_ops_t* ops;
    void* context;
} nx_usb_setup_decision_port_t;
typedef struct {
    const nx_usb_device_port_t* port;
    const nx_usb_endpoint_profile_t* endpoints;
    size_t endpoint_count;
    uint32_t phy_clock_hz;
    uint16_t fifo_words;
    uint16_t rx_fifo_words;
    uint8_t endpoint_numbers;
    nx_usb_profile_role_t role;
    nx_usb_profile_speed_t speed;
    nx_usb_profile_phy_t phy;
    nx_usb_profile_engine_t engine;
    nx_usb_setup_decision_port_t setup_decision;
} nx_usb_device_profile_t;
/**
 * \brief           Validate exact selected FS facts without observing the
 *                  controller.
 *
 * \param[in]       profile: Stable readable immutable facts, faces and
 *                  endpoint array.
 *
 * \return          Success or INVALID, including misaligned public metadata;
 *                  no allocation, MMIO, discovery or lifecycle effect.
 *
 * \note            Task/ISR bounded and read-only. Facts identify selected
 *                  software support; they never imply PCB routing, VBUS
 *                  wiring or physical qualification. Public metadata,
 *                  endpoint arrays, faces and operations tables require their
 *                  natural C alignment and nonwrapping extents. Numeric
 *                  rejection is not a probe of arbitrary addresses or opaque
 *                  private contexts.
 */
nx_result_t nx_usb_profile_validate(const nx_usb_device_profile_t* profile);
/**
 * \brief           Find a direction in a previously validated immutable
 *                  profile.
 *
 * \param[in]       profile: Stable validated selected facts and endpoint
 *                  array.
 *
 * \param[in]       number: Endpoint number, independent of direction.
 *
 * \param[in]       in: IN direction when true; OUT when false.
 *
 * \return          Selected stable member or NULL, including misaligned
 *                  profile/array metadata. No new endpoint can be discovered.
 *
 * \note            Task/ISR read-only bounded lookup; all input lifetimes
 *                  remain external.
 */
const nx_usb_endpoint_profile_t*
nx_usb_profile_endpoint(const nx_usb_device_profile_t* profile, uint8_t number,
                        bool in);
/**
 * \brief           Reject one published SETUP epoch by a checked EP0 IN STALL.
 *
 * \param[in]       port: Stable selected decision face, same controller owner.
 *
 * \param[in]       epoch: Nonzero software-published SETUP identity to reject.
 *
 * \return          Success after guarded STALL readback; STATE for a superseded
 *                  epoch/closing source, BUSY while either EP0 packet is live,
 *                  INVALID for malformed metadata, CONTEXT for an invalid
 *                  executor, IO for failed STALL readback, or UNSUPPORTED
 *                  without the optional operation. Rejection retains no loan.
 *
 * \note            Same privileged unmasked single task executor. The provider
 *                  checks its published epoch and performs STALL in one local
 *                  IRQ critical section, preserving the incoming mask. This
 *                  excludes an already software-published replacement SETUP;
 *                  it does not promise atomicity with physical bus packets
 *                  not yet collected by the IRQ. No cancellation, global stop,
 *                  allocation, class policy or address commit is implicit.
 */
nx_result_t nx_usb_setup_reject(const nx_usb_setup_decision_port_t* port,
                                uint64_t epoch);
/* Generated only when factory.capabilities=true and USB is selected:
 * const nx_usb_device_profile_t* nx_factory_usb_profile(nx_usb_device_id_t);
 * Existing generic *_capability getters keep their original types.
 */
#ifdef __cplusplus
}
#endif
#endif
