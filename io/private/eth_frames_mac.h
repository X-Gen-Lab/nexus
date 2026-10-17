/**
 * \file            eth_frames_mac.h
 * \brief           Selected-only enhanced descriptor and MAC-clock mechanism
 * \author          Nexus Team
 * \note            Independent state keeps the legacy Ethernet face unchanged.
 */
#ifndef NEXUS_PRIVATE_ETH_FRAMES_MAC_H
#define NEXUS_PRIVATE_ETH_FRAMES_MAC_H
#include "nexus/arch/arch.h"
#include "nexus/io/dma.h"
#include "nexus/io/ethernet_frames.h"
#include "nexus/io/mdio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exactly eight hardware words, never the old normal descriptor. */
typedef struct {
    volatile uint32_t status;
    volatile uint32_t control;
    volatile uint32_t buffer;
    volatile uint32_t next;
    volatile uint32_t extended;
    volatile uint32_t reserved;
    volatile uint32_t timestamp_low;
    volatile uint32_t timestamp_high;
} nx_eth_frames_descriptor_t;
typedef enum {
    NX_ETH_FRAMES_TX_FREE,
    NX_ETH_FRAMES_TX_PREPARING,
    NX_ETH_FRAMES_TX_DMA,
    NX_ETH_FRAMES_TX_DRAINING
} nx_eth_frames_tx_phase_t;
typedef struct {
    nx_eth_frames_tx_request_t* request;
    size_t first;
    size_t count;
    size_t length;
    size_t returned;
    nx_time_us_t admitted_us;
    nx_result_t terminal;
    nx_eth_frames_tx_phase_t phase;
} nx_eth_frames_tx_slot_t;
typedef enum {
    NX_ETH_FRAMES_RX_DMA,
    NX_ETH_FRAMES_RX_PARTIAL,
    NX_ETH_FRAMES_RX_READY,
    NX_ETH_FRAMES_RX_HELD
} nx_eth_frames_rx_state_t;
typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t length;
    size_t first;
    size_t count;
    size_t ready_next;
    uint64_t epoch;
    nx_eth_frames_rx_state_t state;
    nx_time_us_t acquired_us;
    nx_time_us_t armed_us;
    nx_io_timestamp_t stamp;
    nx_eth_vlan_t vlan;
    uint32_t flags;
    uint32_t capture_flags;
} nx_eth_frames_rx_slot_t;
/**
 * \brief Exact selected state; no legacy face/state/ops expansion.
 * \note All arrays are caller-owned with exactly tx_count/rx_count entries.
 *       Public request/fragment metadata stays outside DMA windows. Epochs
 *       survive stop/start, binding is immutable and initialization cannot
 *       erase retained ownership. Gates/pads/PHY remain Platform/Board leases.
 */
typedef struct nx_eth_frames_mac {
    uintptr_t registers;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_eth_frames_descriptor_t* tx_descriptors;
    nx_eth_frames_descriptor_t* rx_descriptors;
    nx_eth_frames_tx_slot_t* tx_slots;
    nx_eth_frames_rx_slot_t* rx_slots;
    size_t tx_count;
    size_t rx_count;
    size_t tx_producer;
    size_t tx_consumer;
    size_t tx_in_use;
    size_t rx_producer;
    size_t rx_ready_head;
    size_t rx_ready_tail;
    uint64_t rx_epoch;
    uint32_t clock_domain;
    uint32_t clock_epoch;
    uint32_t clock_last_seconds;
    uint32_t clock_last_nanoseconds;
    uint64_t clock_wrap_nanoseconds;
    nx_time_us_t clock_last_sample_us;
    bool clock_observed;
    nx_time_us_t drain_deadline;
    nx_time_us_t last_control_write_us;
    uint32_t drain_timeout_us;
    uint32_t hclk_hz;
    uint8_t address[6];
    nx_irq_policy_t irq_policy;
    nx_irq_source_t source;
    const nx_irq_wake_t* wake;
    nx_result_t (*irq_control)(struct nx_eth_frames_mac*, bool);
    void (*pend_irq)(struct nx_eth_frames_mac*);
    nx_result_t (*validate_wake)(struct nx_eth_frames_mac*,
                                 const nx_irq_wake_t*, uint8_t);
    nx_eth_frames_state_t snapshot;
    uint16_t wake_publishers;
    uint8_t mdio_divider;
    bool tx_closing;
    bool flush_started;
    bool retained;
} nx_eth_frames_mac_t;
extern const nx_eth_frames_ops_t nx_eth_frames_mac_ops;
extern const nx_mdio_ops_t nx_eth_frames_mdio_ops;
nx_result_t nx_eth_frames_mac_initialize(nx_eth_frames_mac_t* mac,
                                         nx_time_us_t deadline);
void nx_eth_frames_mac_irq(nx_eth_frames_mac_t* mac);
/* Private family leaves required by the same focused target in two variants. */
nx_result_t nx_stm32_eth_frames_initialize(nx_eth_frames_mac_t* mac,
                                           nx_time_us_t deadline);
nx_result_t nx_gd32_eth_frames_initialize(nx_eth_frames_mac_t* mac,
                                          nx_time_us_t deadline);
#ifndef NX_ETH_FRAMES_READ
#define NX_ETH_FRAMES_READ(base, offset)                                       \
    (*(volatile uint32_t*)((uintptr_t)(base) + (offset)))
#endif
#ifndef NX_ETH_FRAMES_HOST_ADDRESSES
#define NX_ETH_FRAMES_HOST_ADDRESSES 0
#endif
#ifndef NX_ETH_FRAMES_WRITE
#define NX_ETH_FRAMES_WRITE(base, offset, value)                               \
    (NX_ETH_FRAMES_READ((base), (offset)) = (uint32_t)(value))
#endif
#ifndef NX_ETH_FRAMES_POLL
#define NX_ETH_FRAMES_POLL(mac) ((void)(mac))
#endif
#ifndef NX_ETH_FRAMES_DESCRIPTOR_STATUS
#define NX_ETH_FRAMES_DESCRIPTOR_STATUS(descriptor, value)                     \
    ((descriptor)->status = (uint32_t)(value))
#endif
#ifdef __cplusplus
}
#endif
#endif
