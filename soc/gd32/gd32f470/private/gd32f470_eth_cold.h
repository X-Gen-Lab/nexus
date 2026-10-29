/**
 * \file            gd32f470_eth_cold.h
 * \brief           Concrete RMII static assembly and retained cold lifecycle
 * \author          Nexus Team
 */
#ifndef NX_GD32F470_ETH_COLD_H
#define NX_GD32F470_ETH_COLD_H
#include "gd32f470_eth.h"
#include "gd32f470_packet_cold.h"
#include <string.h>

/** \brief Exact descriptor/storage selection and immutable RMII policy. */
typedef struct {
    const nx_gd32_platform_pin_config_t* pins;
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
    uint8_t address[6];
} nx_gd32_eth_cold_config_t;

/** \brief Connect reviewed RMII resources and initialize the actual MAC.
 * \note Privileged unmasked Thread only. Static config, descriptor/slot and
 *       RX storage survive every borrow and failed cleanup. Failed prepare
 *       retains pin/selector effects for close without resetting live epochs.
 */
static inline nx_result_t
nx_gd32_eth_cold_prepare(nx_eth_mac_t* port,
                         const nx_gd32_eth_cold_config_t* config,
                         nx_gd32_eth_pin_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL ||
        config->regions == NULL || config->tx_descriptors == NULL ||
        config->rx_descriptors == NULL || config->tx_slots == NULL ||
        config->rx_slots == NULL || config->storage == NULL ||
        config->tx_count == 0U || config->rx_count == 0U ||
        config->rx_storage_bytes == 0U || config->init_timeout_us == 0U ||
        config->drain_timeout_us == 0U || config->priority > 15U ||
        config->rx_count > SIZE_MAX / config->rx_storage_bytes ||
        !nx_gd32_packet_pins_valid(config->pins, 9U))
        return NX_ERROR_INVALID;
    if (port->epoch == UINT64_MAX)
        return NX_ERROR_INVALID;
    if (receipt->leases != 0U || receipt->mux_leased || port->retained ||
        port->wake_publishers != 0U || port->snapshot.started)
        return NX_ERROR_BUSY;
    uint64_t epoch = port->epoch;
    memset(port, 0, sizeof(*port));
    port->epoch = epoch;
    port->registers = UINT32_C(0x40028000);
    port->hclk_hz = 200000000U;
    port->regions = config->regions;
    port->region_count = 1U;
    port->tx_descriptors = config->tx_descriptors;
    port->rx_descriptors = config->rx_descriptors;
    port->tx_slots = config->tx_slots;
    port->rx_slots = config->rx_slots;
    port->tx_count = config->tx_count;
    port->rx_count = config->rx_count;
    port->drain_timeout_us = config->drain_timeout_us;
    memcpy(port->address, config->address, sizeof(port->address));
    port->irq_policy = config->irq_policy;
    port->source.irq_number = ENET_IRQn;
    port->source.priority = config->priority;
    port->source.priority_group = 3U;
    for (size_t i = 0U; i < config->rx_count; ++i) {
        config->rx_slots[i].data =
            config->storage + i * config->rx_storage_bytes;
        config->rx_slots[i].capacity = config->rx_storage_bytes;
    }
    nx_result_t status = nx_gd32_eth_pins_prepare(config->pins, receipt, false);
    if (status != NX_SUCCESS)
        return status;
    return nx_gd32_eth_initialize(
        port, nx_deadline_after(nx_time_now_us(), config->init_timeout_us));
}

/** \brief Detach every MAC reference before restoring selected pads/mux.
 * \note Failed MAC drain, pin or mux readback retains ownership for retry;
 *       the shared Platform gate owner closes only after this succeeds.
 */
static inline nx_result_t
nx_gd32_eth_cold_close(nx_eth_mac_t* port,
                       const nx_gd32_eth_cold_config_t* config,
                       nx_gd32_eth_pin_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL ||
        !nx_gd32_packet_pins_valid(config->pins, 9U))
        return NX_ERROR_INVALID;
    if (receipt->leases == 0U && !receipt->mux_leased &&
        (port->retained || port->snapshot.started ||
         port->wake_publishers != 0U))
        return NX_ERROR_BUSY;
    nx_result_t status = nx_eth_mac_ops.stop(port);
    if (status != NX_SUCCESS)
        return status;
    return nx_gd32_eth_pins_close(config->pins, receipt);
}
#endif
