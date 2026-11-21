/**
 * \file            lwip_services_storage.h
 * \brief           Exact selected lwIP service storage for static composition
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_SERVICES_STORAGE_H
#define NEXUS_NETWORK_LWIP_SERVICES_STORAGE_H
#include "lwip/dhcp.h"
#include "lwip/udp.h"
#include "nexus/network/lwip_services.h"
/** \brief Selected copied UDP storage, private after open. */
struct nx_lwip_udp {
    nx_lwip_adapter_t* network;
    struct udp_pcb* pcb;
    uint8_t* buffer;
    size_t capacity;
    size_t received;
    nx_ipv4_peer_t peer;
    uint32_t dropped;
    bool pending;
    bool active;
};
#if LWIP_DHCP
/** \brief Managed-profile DHCP owns explicit static protocol state. */
struct nx_lwip_dhcp {
    nx_lwip_adapter_t* network;
    struct dhcp state;
};
#endif
/** \brief DNS callback borrow persists to callback retirement, even on expiry.
 */
struct nx_lwip_dns {
    nx_lwip_adapter_t* network;
    nx_time_us_t deadline;
    uint8_t address[4];
    nx_result_t result;
    bool pending;
    bool cancelled;
};
#endif
