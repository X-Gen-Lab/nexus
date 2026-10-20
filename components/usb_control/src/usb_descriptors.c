/**
 * \file            usb_descriptors.c
 * \brief           Bounded selected-FS descriptor structure validation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_descriptors.h"

/** \brief Decode descriptor fields without unaligned halfword loads. */
static uint16_t u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}
/** \brief Validate standard configuration structure with no registry/storage.
 */
static nx_result_t configuration(const nx_usb_device_profile_t* p,
                                 const uint8_t* b, size_t n) {
    if (n < 9U || b[0] != 9U || u16(b + 2U) != n || b[4] == 0U || b[5] == 0U ||
        (b[7] & 128U) == 0U || (b[7] & 31U) != 0U) {
        return NX_ERROR_INVALID;
    }
    if ((b[7] & 32U) != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t offset = 9U;
    unsigned interfaces = 0U;
    unsigned expected = 0U;
    unsigned actual = 0U;
    uint8_t interface_bits[32] = {0};
    uint16_t endpoint_bits = 0U;
    bool have_interface = false;
    while (offset < n) {
        if (n - offset < 2U || b[offset] < 2U || b[offset] > n - offset) {
            return NX_ERROR_INVALID;
        }
        const uint8_t* d = b + offset;
        if (d[1] == 4U) {
            if (d[0] != 9U || d[2] >= b[4] ||
                (have_interface && actual != expected)) {
                return NX_ERROR_INVALID;
            }
            if (d[3] != 0U) {
                return NX_ERROR_UNSUPPORTED;
            }
            unsigned index = d[2] / 8U;
            uint8_t bit = (uint8_t)(1U << (d[2] % 8U));
            if ((interface_bits[index] & bit) != 0U) {
                return NX_ERROR_INVALID;
            }
            interface_bits[index] |= bit;
            ++interfaces;
            have_interface = true;
            expected = d[4];
            actual = 0U;
        } else if (d[1] == 5U) {
            if (d[0] != 7U || !have_interface || (d[2] & 112U) != 0U ||
                (d[2] & 15U) == 0U || (d[3] & 252U) != 0U) {
                return NX_ERROR_INVALID;
            }
            if ((d[3] & 3U) != 2U) {
                return NX_ERROR_UNSUPPORTED;
            }
            unsigned number = d[2] & 15U;
            bool input = (d[2] & 128U) != 0U;
            const nx_usb_endpoint_profile_t* ep =
                nx_usb_profile_endpoint(p, (uint8_t)number, input);
            if (ep == NULL || ep->transfer != NX_USB_PROFILE_BULK ||
                u16(d + 4U) != ep->max_packet) {
                return NX_ERROR_INVALID;
            }
            uint16_t bit = (uint16_t)(1U << (number * 2U + (input ? 1U : 0U)));
            if ((endpoint_bits & bit) != 0U) {
                return NX_ERROR_INVALID;
            }
            endpoint_bits |= bit;
            ++actual;
        } else if (d[1] == 1U || d[1] == 2U || d[1] == 3U) {
            return NX_ERROR_INVALID;
        }
        offset += d[0];
    }
    return have_interface && actual == expected && interfaces == b[4]
               ? NX_SUCCESS
               : NX_ERROR_INVALID;
}
nx_result_t nx_usb_descriptor_validate(const nx_usb_device_profile_t* p,
                                       const nx_usb_descriptor_view_t* view) {
    if (nx_usb_profile_validate(p) != NX_SUCCESS || view == NULL ||
        (uintptr_t)view % _Alignof(nx_usb_descriptor_view_t) != 0U ||
        sizeof(*view) > UINTPTR_MAX - (uintptr_t)view || view->bytes == NULL ||
        view->length < 2U ||
        view->length > UINTPTR_MAX - (uintptr_t)view->bytes) {
        return NX_ERROR_INVALID;
    }
    const uint8_t* b = view->bytes;
    size_t n = view->length;
    if (b[1] == 1U) {
        return n == 18U && b[0] == 18U && b[7] == NX_USB_FS_MAX_PACKET &&
                       u16(b + 2U) >= 0x0100U && u16(b + 2U) <= 0x0200U &&
                       b[17] != 0U
                   ? NX_SUCCESS
                   : NX_ERROR_INVALID;
    }
    if (b[1] == 2U) {
        return configuration(p, b, n);
    }
    if (b[1] == 3U) {
        return b[0] == n && n % 2U == 0U ? NX_SUCCESS : NX_ERROR_INVALID;
    }
    return NX_ERROR_UNSUPPORTED;
}
