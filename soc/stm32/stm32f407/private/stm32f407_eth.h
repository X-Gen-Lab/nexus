/**
 * \file            stm32f407_eth.h
 * \brief           SDK-reviewed RMII MAC with caller-owned descriptor storage
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_ETH_H
#define NEXUS_STM32F407_ETH_H
#include "../../../../io/private/eth_mac.h"
#include "stm32f407xx.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef nx_eth_mac_t nx_stm32_eth_state_t;
#define nx_stm32_eth_ops  nx_eth_mac_ops
#define nx_stm32_mdio_ops nx_eth_mdio_ops
/**
 * \brief Cold exact-controller start after Board clock/RMII/AF preparation.
 * \note Privileged task, finite deadline. Board owns PHY clock, pins and their
 *       electrical release. Only reviewed main SRAM domains are accepted;
 *       CCM/TCM, enhanced descriptors, MII and PTP remain unsupported.
 */
nx_result_t nx_stm32_eth_initialize(nx_eth_mac_t* mac, nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
