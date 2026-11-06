/**
 * \file            eth_validation.h
 * \brief           Private Ethernet frame validation without hardware
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_ETH_VALIDATION_H
#define NEXUS_PRIVATE_ETH_VALIDATION_H
#include "nexus/io/ethernet.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Validate the entire frame without admission or retaining storage. */
bool nx_eth_frame_valid(const void* data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
