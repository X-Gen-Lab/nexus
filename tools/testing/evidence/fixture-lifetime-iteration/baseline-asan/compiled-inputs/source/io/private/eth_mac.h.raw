/**
 * \file            eth_mac.h
 * \brief           Private four-word chained DWMAC descriptor mechanism
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_ETH_MAC_H
#define NEXUS_PRIVATE_ETH_MAC_H
#include "nexus/arch/arch.h"
#include "nexus/io/dma.h"
#include "nexus/io/ethernet.h"
#include "nexus/io/mdio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Actual normal descriptor; enhanced/PTP descriptor mode is disabled.
 */
typedef struct {
    volatile uint32_t status;
    volatile uint32_t control;
    volatile uint32_t buffer;
    volatile uint32_t next;
} nx_eth_descriptor_t;
typedef enum {
    NX_ETH_RX_DMA,
    NX_ETH_RX_READY,
    NX_ETH_RX_HELD
} nx_eth_rx_slot_state_t;
typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t length;
    size_t ready_next;
    uint64_t epoch;
    nx_time_us_t timestamp_us;
    nx_eth_rx_slot_state_t state;
} nx_eth_rx_slot_t;
typedef struct {
    nx_eth_tx_request_t* request;
    nx_result_t terminal;
} nx_eth_tx_slot_t;
/**
 * \brief Exact caller storage and independent Ethernet bus-master domains.
 * \note Every descriptor and RX buffer needs RW access; TX needs read access.
 *       Regions/configuration and all storage outlive a successful final stop.
 *       No SDK global descriptor cursor, buffer pool or default instance.
 */
typedef struct nx_eth_mac {
    uintptr_t registers;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_eth_descriptor_t* tx_descriptors;
    nx_eth_descriptor_t* rx_descriptors;
    nx_eth_tx_slot_t* tx_slots;
    nx_eth_rx_slot_t* rx_slots;
    size_t tx_count;
    size_t rx_count;
    size_t tx_producer;
    size_t tx_consumer;
    size_t rx_producer;
    size_t rx_consumer;
    size_t rx_ready_tail;
    uint64_t epoch;
    nx_time_us_t drain_deadline;
    uint32_t drain_timeout_us;
    uint32_t hclk_hz;
    uint8_t address[6];
    nx_irq_policy_t irq_policy;
    nx_irq_source_t source;
    const nx_irq_wake_t* wake;
    nx_result_t (*irq_control)(struct nx_eth_mac*, bool);
    void (*pend_irq)(struct nx_eth_mac*);
    nx_result_t (*validate_wake)(struct nx_eth_mac*, const nx_irq_wake_t*,
                                 uint8_t);
    nx_eth_state_t snapshot;
    uint16_t wake_publishers;
    uint8_t mdio_divider;
    bool tx_closing;
    bool flush_started;
    bool retained;
} nx_eth_mac_t;
extern const nx_eth_ops_t nx_eth_mac_ops;
extern const nx_mdio_ops_t nx_eth_mdio_ops;
/** \brief Validate complete storage before reset; finite task deadline. */
nx_result_t nx_eth_mac_initialize(nx_eth_mac_t* mac, nx_time_us_t deadline);
/** \brief At most one TX and one RX descriptor per interrupt invocation. */
void nx_eth_mac_irq(nx_eth_mac_t* mac);
/** \brief Private frame/header validation without effects. */
bool nx_eth_frame_valid(const void* data, size_t length);
#ifndef NX_ETH_READ
#define NX_ETH_READ(base, offset)                                              \
    (*(volatile uint32_t*)((uintptr_t)(base) + (offset)))
#endif
#ifndef NX_ETH_WRITE
#define NX_ETH_WRITE(base, offset, value)                                      \
    (NX_ETH_READ((base), (offset)) = (uint32_t)(value))
#endif
#ifndef NX_ETH_POLL
#define NX_ETH_POLL(mac) ((void)(mac))
#endif
#ifndef NX_ETH_DESCRIPTOR_STATUS
#define NX_ETH_DESCRIPTOR_STATUS(descriptor, value)                            \
    ((descriptor)->status = (uint32_t)(value))
#endif
#ifndef NX_ETH_HOST_ADDRESSES
#define NX_ETH_HOST_ADDRESSES 0
#endif
#ifdef __cplusplus
}
#endif
#endif
