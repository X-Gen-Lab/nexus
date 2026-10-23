/**
 * \file            ethernet_access.h
 * \brief           Native-only external hardware access compilation boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_ETHERNET_ACCESS_H
#define NEXUS_NATIVE_ETHERNET_ACCESS_H
#include "ethernet.h"
#undef NX_ETH_READ
#undef NX_ETH_WRITE
#undef NX_ETH_POLL
#undef NX_ETH_DESCRIPTOR_STATUS
#define NX_ETH_DESCRIPTOR_STATUS(descriptor, value)                            \
    ((descriptor)->status = (uint32_t)(value))
#define NX_ETH_READ(base, offset)                                              \
    nx_native_eth_read((uintptr_t)(base), (offset))
#define NX_ETH_WRITE(base, offset, value)                                      \
    nx_native_eth_write((uintptr_t)(base), (offset), (uint32_t)(value))
#define NX_ETH_POLL(mac) nx_native_eth_poll(mac)
#undef NX_ETH_HOST_ADDRESSES
#define NX_ETH_HOST_ADDRESSES 1
#endif
