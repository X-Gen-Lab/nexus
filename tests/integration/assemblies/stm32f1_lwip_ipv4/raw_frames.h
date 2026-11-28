/**
 * \file            raw_frames.h
 * \brief           Private single-fragment projection for the F107 fixture.
 * \author          Nexus Team
 */
#ifndef NEXUS_F107_LWIP_RAW_FRAMES_H
#define NEXUS_F107_LWIP_RAW_FRAMES_H
#include "nexus/io/ethernet_frames.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_eth_frames_port_t face;
    const nx_eth_port_t* raw;
    nx_eth_tx_request_t transmit;
    nx_eth_frames_tx_request_t* pending;
    nx_eth_rx_frame_t receive;
} f107_raw_frames_t;

void f107_raw_frames_init(f107_raw_frames_t* frames, const nx_eth_port_t* raw);

#ifdef __cplusplus
}
#endif
#endif
