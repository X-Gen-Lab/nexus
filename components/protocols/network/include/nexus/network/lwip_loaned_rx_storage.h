/**
 * \file            lwip_loaned_rx_storage.h
 * \brief           Exact selected RX-loan/custom-pbuf composition storage
 * \author          Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_LOANED_RX_STORAGE_H
#define NEXUS_NETWORK_LWIP_LOANED_RX_STORAGE_H

#include "lwip/pbuf.h"
#include "nexus/network/lwip_loaned_rx.h"
#include "nexus/network/lwip_storage.h"

#if !defined(NEXUS_LWIP_LOANED_RX) || !LWIP_SUPPORT_CUSTOM_PBUF
#error "Select only the explicit lwIP loaned RX storage profile"
#endif

/**
 * \brief One caller-owned custom-pbuf header with its unique owning slot.
 * \note Private backend fields; raw users own only normal pbuf references.
 */
struct nx_lwip_rx_piece {
    struct pbuf_custom custom;
    nx_lwip_rx_slot_t* slot;
};

/**
 * \brief One exact RX token shared by all retained pieces of that frame.
 * \note The union provides eight-byte alignment in the maintained profile.
 *       A free callback only decrements references; it never touches hardware.
 *       A slot cannot be reused until the provider accepts the exact release.
 *       Raw users cannot access a pbuf after returning its final reference.
 */
struct nx_lwip_rx_slot {
    nx_eth_frames_rx_loan_t loan;
    nx_lwip_rx_piece_t header;
    union {
        uint64_t alignment;
        uint8_t bytes[136];
    } prefix;
    size_t live_pieces;
    bool held;
};

#endif
