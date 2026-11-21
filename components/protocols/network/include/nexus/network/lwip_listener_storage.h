/**
 * \file            lwip_listener_storage.h
 * \brief           Exact raw listener storage, restricted to static composition
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_LISTENER_STORAGE_H
#define NEXUS_NETWORK_LWIP_LISTENER_STORAGE_H
#include "lwip/tcp.h"
#include "nexus/network/lwip_listener.h"
/** \brief Private selected listener state; counters saturate instead of wrap.
 */
struct nx_lwip_tcp_listener {
    struct tcp_pcb_listen* pcb;
    nx_lwip_adapter_t* network;
    nx_lwip_tcp_t* clients;
    size_t count;
    size_t receive_budget;
    uint32_t accepted;
    uint32_t rejected;
    bool active;
};
#endif
