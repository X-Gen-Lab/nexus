/**
 * \file            usb_profile.c
 * \brief           Pure selected USB facts and endpoint membership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/usb_profile.h"

/** \brief Check numeric extent/alignment before typed reads, not memory
 * mapping. */
static bool metadata(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t start = (uintptr_t)pointer;
    return pointer != NULL && start % alignment == 0U &&
           bytes <= UINTPTR_MAX - start;
}

/** \brief Exact core and PHY tuples keep HS internal FS distinct from ULPI. */
static bool facts(const nx_usb_device_profile_t* p) {
    const bool fs = p->fifo_words == 320U && p->endpoint_numbers == 4U;
    const bool hs = p->fifo_words == 1024U && p->endpoint_numbers == 6U;
    if ((!fs && !hs) || p->endpoint_count > (size_t)p->endpoint_numbers * 2U ||
        p->rx_fifo_words < 48U || p->rx_fifo_words >= p->fifo_words ||
        (fs && p->rx_fifo_words > 256U) || p->role != NX_USB_PROFILE_DEVICE ||
        (p->engine != NX_USB_PROFILE_PIO &&
         p->engine != NX_USB_PROFILE_AHB_DMA) ||
        (p->engine == NX_USB_PROFILE_AHB_DMA && !hs)) {
        return false;
    }
    if (p->phy == NX_USB_PROFILE_EXTERNAL_ULPI) {
        return hs && p->phy_clock_hz == 60000000U &&
               (p->speed == NX_USB_PROFILE_FULL_SPEED ||
                p->speed == NX_USB_PROFILE_HIGH_SPEED);
    }
    return p->phy == NX_USB_PROFILE_INTERNAL_FS &&
           p->phy_clock_hz == 48000000U &&
           p->speed == NX_USB_PROFILE_FULL_SPEED;
}

/** \brief Validate the selected FS subset, without inferring PCB readiness. */
nx_result_t nx_usb_profile_validate(const nx_usb_device_profile_t* p) {
    if (!metadata(p, sizeof(*p), _Alignof(nx_usb_device_profile_t)) ||
        !metadata(p->port, sizeof(*p->port), _Alignof(nx_usb_device_port_t)) ||
        p->port->context == NULL ||
        !metadata(p->port->ops, sizeof(*p->port->ops),
                  _Alignof(nx_usb_device_ops_t)) ||
        !metadata(p->setup_decision.ops, sizeof(*p->setup_decision.ops),
                  _Alignof(nx_usb_setup_decision_ops_t)) ||
        p->setup_decision.context != p->port->context ||
        p->setup_decision.ops->reject == NULL ||
        p->port->ops->service == NULL || p->port->ops->state == NULL ||
        p->endpoints == NULL || p->endpoint_count < 2U ||
        p->endpoint_count > 12U ||
        !metadata(p->endpoints, p->endpoint_count * sizeof(*p->endpoints),
                  _Alignof(nx_usb_endpoint_profile_t)) ||
        !facts(p)) {
        return NX_ERROR_INVALID;
    }
    uint16_t seen = 0U;
    uint16_t largest_out = 64U;
    size_t out_count = 0U, iso_out_count = 0U;
    for (size_t i = 0U; i < p->endpoint_count; ++i) {
        const nx_usb_endpoint_profile_t* ep = &p->endpoints[i];
        if (ep->number >= p->endpoint_numbers ||
            !metadata(ep->port, sizeof(*ep->port),
                      _Alignof(nx_usb_endpoint_port_t)) ||
            ep->port->context == NULL ||
            !metadata(ep->port->ops, sizeof(*ep->port->ops),
                      _Alignof(nx_usb_endpoint_ops_t)) ||
            ep->port->ops->submit == NULL || ep->port->ops->cancel == NULL ||
            ep->port->ops->stall == NULL) {
            return NX_ERROR_INVALID;
        }
        uint16_t bit = (uint16_t)(1U << (ep->number * 2U + (ep->in ? 1U : 0U)));
        if ((seen & bit) != 0U) {
            return NX_ERROR_INVALID;
        }
        seen |= bit;
        if (ep->number == 0U) {
            if (ep->transfer != NX_USB_PROFILE_CONTROL ||
                ep->max_packet != NX_USB_FS_MAX_PACKET) {
                return NX_ERROR_INVALID;
            }
        } else if (ep->transfer == NX_USB_PROFILE_BULK) {
            const bool valid =
                p->speed == NX_USB_PROFILE_HIGH_SPEED
                    ? ep->max_packet == 512U
                    : ep->max_packet == 8U || ep->max_packet == 16U ||
                          ep->max_packet == 32U || ep->max_packet == 64U;
            if (!valid) {
                return NX_ERROR_INVALID;
            }
        } else if (ep->transfer == NX_USB_PROFILE_ISOCHRONOUS) {
            if (ep->max_packet == 0U ||
                ep->max_packet >
                    (p->speed == NX_USB_PROFILE_HIGH_SPEED ? 1024U : 1023U)) {
                return NX_ERROR_INVALID;
            }
        } else if (ep->transfer != NX_USB_PROFILE_INTERRUPT ||
                   ep->max_packet == 0U ||
                   ep->max_packet >
                       (p->speed == NX_USB_PROFILE_HIGH_SPEED ? 1024U : 64U)) {
            return NX_ERROR_INVALID;
        }
        if (!ep->in) {
            ++out_count;
            if (ep->transfer == NX_USB_PROFILE_ISOCHRONOUS)
                ++iso_out_count;
            if (ep->max_packet > largest_out)
                largest_out = ep->max_packet;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (p->endpoints[j].port == ep->port) {
                return NX_ERROR_INVALID;
            }
        }
    }
    size_t packet_words = ((size_t)largest_out + 3U) / 4U + 1U;
    size_t minimum_rx =
        11U + packet_words * (iso_out_count > 1U ? 2U : 1U) + out_count;
    return (seen & 3U) == 3U && p->rx_fifo_words >= minimum_rx
               ? NX_SUCCESS
               : NX_ERROR_INVALID;
}

/** \brief Bounded lookup of one direction within already validated facts. */
const nx_usb_endpoint_profile_t*
nx_usb_profile_endpoint(const nx_usb_device_profile_t* p, uint8_t number,
                        bool in) {
    if (!metadata(p, sizeof(*p), _Alignof(nx_usb_device_profile_t)) ||
        p->endpoints == NULL || p->endpoint_count > 12U ||
        !metadata(p->endpoints, p->endpoint_count * sizeof(*p->endpoints),
                  _Alignof(nx_usb_endpoint_profile_t))) {
        return NULL;
    }
    for (size_t i = 0U; i < p->endpoint_count; ++i) {
        if (p->endpoints[i].number == number && p->endpoints[i].in == in) {
            return &p->endpoints[i];
        }
    }
    return NULL;
}

/** \brief Dispatch the versioned decision; the provider owns atomic admission.
 */
nx_result_t nx_usb_setup_reject(const nx_usb_setup_decision_port_t* port,
                                uint64_t epoch) {
    if (!metadata(port, sizeof(*port),
                  _Alignof(nx_usb_setup_decision_port_t)) ||
        !metadata(port->ops, sizeof(*port->ops),
                  _Alignof(nx_usb_setup_decision_ops_t)) ||
        port->context == NULL || epoch == 0U) {
        return NX_ERROR_INVALID;
    }
    return port->ops->reject == NULL ? NX_ERROR_UNSUPPORTED
                                     : port->ops->reject(port->context, epoch);
}
