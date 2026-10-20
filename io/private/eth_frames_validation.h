/**
 * \file            eth_frames_validation.h
 * \brief           Whole-frame validation before any ownership transfer
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_ETH_FRAMES_VALIDATION_H
#define NEXUS_PRIVATE_ETH_FRAMES_VALIDATION_H
#include "nexus/io/ethernet_frames.h"

static inline bool nx_eth_frames_span(const void* pointer, size_t length) {
    return pointer != NULL && length != 0U &&
           (uintptr_t)pointer <= UINTPTR_MAX - length;
}
static inline bool nx_eth_frames_overlap(const void* first, size_t first_size,
                                         const void* last, size_t last_size) {
    return first_size != 0U && last_size != 0U &&
           (uintptr_t)first < (uintptr_t)last + last_size &&
           (uintptr_t)last < (uintptr_t)first + first_size;
}
static inline uint8_t nx_eth_frames_byte(const nx_eth_fragment_t* fragments,
                                         size_t count, size_t offset) {
    for (size_t i = 0U; i < count; ++i) {
        if (offset < fragments[i].length)
            return fragments[i].data[offset];
        offset -= fragments[i].length;
    }
    return 0U;
}
static inline bool nx_eth_frames_valid(const nx_eth_fragment_t* fragments,
                                       size_t count, size_t* length,
                                       nx_eth_vlan_t* vlan) {
    if (count == 0U || count > SIZE_MAX / sizeof(*fragments) ||
        !nx_eth_frames_span(fragments, count * sizeof(*fragments)))
        return false;
    size_t total = 0U;
    for (size_t i = 0U; i < count; ++i) {
        if (!nx_eth_frames_span(fragments[i].data, fragments[i].length) ||
            fragments[i].length > 1518U - total)
            return false;
        total += fragments[i].length;
    }
    if (total < 14U)
        return false;
    uint16_t type =
        (uint16_t)((uint16_t)nx_eth_frames_byte(fragments, count, 12U) << 8U) |
        nx_eth_frames_byte(fragments, count, 13U);
    nx_eth_vlan_t selected = {0};
    if (type == 0x8100U) {
        if (total < 18U)
            return false;
        selected.present = true;
        selected.tci =
            (uint16_t)((uint16_t)nx_eth_frames_byte(fragments, count, 14U)
                       << 8U) |
            nx_eth_frames_byte(fragments, count, 15U);
        type = (uint16_t)((uint16_t)nx_eth_frames_byte(fragments, count, 16U)
                          << 8U) |
               nx_eth_frames_byte(fragments, count, 17U);
    }
    if (type < 0x600U || type == 0x8100U || type == 0x88a8U ||
        (!selected.present && total > 1514U))
        return false;
    if (length != NULL)
        *length = total;
    if (vlan != NULL)
        *vlan = selected;
    return true;
}
#endif
