/**
 * \file            lwip_storage.h
 * \brief           Exact pinned lwIP backend storage for static allocation
 * \author          Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_STORAGE_H
#define NEXUS_NETWORK_LWIP_STORAGE_H

#include "lwip/netif.h"
#include "nexus/network/lwip_adapter.h"
#if defined(NEXUS_LWIP_LOANED_RX)
#include "nexus/network/lwip_loaned_rx.h"
#endif

/**
 * \brief Caller-owned backend storage; include only at the composition site.
 * \note This explicit backend header is tied to the pinned lwIP profile and
 *       exposes its exact netif layout to avoid a maximum-size opaque pool.
 *       Fields are implementation state, not an application control API.
 *       Application raw PCBs use this netif only during live attachment.
 */
struct nx_lwip_adapter {
    struct netif interface;
    nx_lwip_config_t config;
    nx_eth_frames_tx_request_t transmit;
    nx_eth_fragment_t transmit_fragment;
#if !defined(NEXUS_LWIP_LOANED_RX)
    nx_eth_frames_rx_loan_t receive;
#endif
    bool attached;
    bool closing;
#if !defined(NEXUS_LWIP_LOANED_RX)
    bool receive_held;
#else
    nx_lwip_rx_slot_t* slots;
    size_t slot_count;
    nx_lwip_rx_piece_t* pieces;
    size_t piece_count;
    bool in_call;
#endif
};

#endif
