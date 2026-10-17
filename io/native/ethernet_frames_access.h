/**
 * \file            ethernet_frames_access.h
 * \brief           Selected Native enhanced MAC hardware access boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_ETHERNET_FRAMES_ACCESS_H
#define NEXUS_NATIVE_ETHERNET_FRAMES_ACCESS_H
#include "ethernet_frames.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_native_eth_frames_read(uintptr_t base, unsigned offset);
void nx_native_eth_frames_write(uintptr_t base, unsigned offset,
                                uint32_t value);
void nx_native_eth_frames_poll(nx_eth_frames_mac_t* mac);
#ifdef __cplusplus
}
#endif
#undef NX_ETH_FRAMES_READ
#undef NX_ETH_FRAMES_WRITE
#undef NX_ETH_FRAMES_POLL
#undef NX_ETH_FRAMES_HOST_ADDRESSES
#define NX_ETH_FRAMES_READ(base, offset)                                       \
    nx_native_eth_frames_read((uintptr_t)(base), (offset))
#define NX_ETH_FRAMES_WRITE(base, offset, value)                               \
    nx_native_eth_frames_write((uintptr_t)(base), (offset), (uint32_t)(value))
#define NX_ETH_FRAMES_POLL(mac)      nx_native_eth_frames_poll(mac)
#define NX_ETH_FRAMES_HOST_ADDRESSES 1
#endif
