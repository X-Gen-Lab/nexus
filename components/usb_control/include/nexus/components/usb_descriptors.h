/**
 * \file            usb_descriptors.h
 * \brief           Bounded descriptor views; product bytes stay external
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_USB_DESCRIPTORS_H
#define NEXUS_COMPONENTS_USB_DESCRIPTORS_H
#include "nexus/io/usb_profile.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const uint8_t* bytes;
    size_t length;
} nx_usb_descriptor_view_t;
/**
 * \brief           Validate complete device, configuration or string
 *                  descriptor bytes.
 *
 * \param[in]       profile: Immutable exact selected FS endpoint facts.
 *
 * \param[in]       descriptor: Complete external descriptor; never
 *                  copied/retained.
 *
 * \return          Success, INVALID or UNSUPPORTED, with no hardware or
 *                  output effects.
 *
 * \note            Configuration views require alternate zero, endpoint plans
 *                  matching selected bulk ports, consistent totals and unique
 *                  interface numbers. Class-specific subordinate bytes are
 *                  bounds-checked but not decoded. Class dispatch, VID/PID,
 *                  strings, policy, CDC/HID and descriptor lookup remain
 *                  external. Success is structural validation, not
 *                  enumeration or physical qualification. To send bytes,
 *                  preserve their lifetime through the control adapter's
 *                  acquire-observed outer SETTLED. The view struct requires
 *                  natural C alignment; rejection is not an arbitrary address
 *                  probe. Descriptor bytes need only byte alignment and remain
 *                  externally readable.
 */
nx_result_t
nx_usb_descriptor_validate(const nx_usb_device_profile_t* profile,
                           const nx_usb_descriptor_view_t* descriptor);
#ifdef __cplusplus
}
#endif
#endif
