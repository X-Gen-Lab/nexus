/**
 * \file            raw.h
 * \brief           Exact F107 normal-descriptor Ethernet cold ownership.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_ETH_RAW_H
#define NEXUS_STM32F1_ETH_RAW_H

#include "drivers/gpio/instance.h"
#include "eth_mac.h"

/** \brief Const storage and policy for one selected raw MAC session. */
typedef struct {
    const nx_dma_memory_region_t* regions;
    nx_eth_descriptor_t* tx_descriptors;
    nx_eth_descriptor_t* rx_descriptors;
    nx_eth_tx_slot_t* tx_slots;
    nx_eth_rx_slot_t* rx_slots;
    uint8_t* storage;
    size_t rx_storage_bytes;
    size_t tx_count;
    size_t rx_count;
    uint32_t init_timeout_us;
    uint32_t drain_timeout_us;
    nx_irq_policy_t irq_policy;
    uint8_t priority;
    bool mii;
    uint8_t address[6];
} nx_stm32f1_eth_config_t;

/** \brief Retained peripheral, remap and pad effects during retryable close. */
typedef struct {
    nx_stm32f1_pads_t pads[4];
    uint32_t prior_mapr;
    uint8_t prior_irq_priority;
    uint8_t effects;
} nx_stm32f1_eth_receipt_t;

nx_result_t nx_stm32f1_eth_prepare(nx_eth_mac_t* mac,
                                   const nx_stm32f1_eth_config_t* config,
                                   nx_stm32f1_eth_receipt_t* receipt);
nx_result_t nx_stm32f1_eth_close(nx_eth_mac_t* mac,
                                 const nx_stm32f1_eth_config_t* config,
                                 nx_stm32f1_eth_receipt_t* receipt);
#endif
