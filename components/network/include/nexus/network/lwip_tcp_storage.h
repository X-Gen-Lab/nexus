/**
 * \file            lwip_tcp_storage.h
 * \brief           Exact pinned TCP client storage; composition only
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_TCP_STORAGE_H
#define NEXUS_NETWORK_LWIP_TCP_STORAGE_H
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "nexus/network/lwip_tcp.h"
struct nx_lwip_tcp {
    nx_lwip_tcp_config_t config;
    nx_byte_stream_t stream;
    struct tcp_pcb* pcb;
    struct pbuf* receive;
    nx_time_us_t connect_deadline;
    nx_time_us_t close_deadline;
    nx_lwip_tcp_diagnostic_t diagnostic;
    uint16_t receive_offset;
    bool initialized;
    bool active;
    bool callback_active;
};
#endif
