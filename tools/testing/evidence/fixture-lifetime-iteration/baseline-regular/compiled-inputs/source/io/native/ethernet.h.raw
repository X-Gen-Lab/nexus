/**
 * \file            ethernet.h
 * \brief           Explicit Native DMA ownership and MDIO register hardware
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_ETHERNET_H
#define NEXUS_NATIVE_ETHERNET_H
#include "../private/eth_mac.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Caller-owned model MMIO, independent cursors and optional PHY file.
 * \note PHY register storage is exactly 32 caller values; absent or another
 *       address reads 0xffff, never an invented supported identity or link.
 */
typedef struct {
    uint32_t registers[0x1060U / 4U];
    uint16_t* phy_registers;
    size_t tx_cursor;
    size_t rx_cursor;
    uint8_t phy_address;
    uint8_t priority;
    uint8_t priority_group;
    bool enabled_irq;
    bool pending_irq;
    bool hold_reset;
    bool hold_engine;
    bool hold_flush;
    bool hold_mdio;
    bool hold_phy_reset;
    bool hold_irq_enable;
    bool hold_irq_disable;
    bool hold_interrupt_clear;
} nx_native_eth_hardware_t;
/** \brief Selected model instance; all descriptor/buffer storage remains
 * external. */
typedef struct {
    nx_eth_mac_t mac;
    nx_native_eth_hardware_t hardware;
} nx_native_eth_t;
typedef nx_native_eth_t nx_native_eth_state_t;
#define nx_native_eth_ops  nx_eth_mac_ops
#define nx_native_mdio_ops nx_eth_mdio_ops
/** \brief Bind caller storage, not a fake PHY identity or auto-link policy. */
nx_result_t nx_native_eth_initialize(nx_native_eth_t* instance,
                                     nx_time_us_t deadline);
/**
 * \brief Explicit modeled on-wire transmit event, excluding FCS output.
 * \note EMPTY/BUSY leaves DMA ownership; capacity failure does not lose a
 *       frame. SUCCESS means one hardware descriptor returned, not SETTLED:
 *       invoke the actual provider IRQ/service separately.
 */
nx_result_t nx_native_eth_transmit(nx_native_eth_t* instance, void* frame,
                                   size_t capacity, size_t* length,
                                   nx_result_t status);
/**
 * \brief Inject one complete frame plus modeled FCS without ring overwrite.
 * \note Payload excludes FCS; status supplies actual RX flags (FS/LS required).
 *       Full/held descriptors return OVERFLOW and latch actual RBU. No IRQ,
 *       worker, network protocol or automatic consumer is invoked here.
 */
nx_result_t nx_native_eth_receive(nx_native_eth_t* instance, const void* frame,
                                  size_t length, uint32_t status);
/** \brief Model register reads, with no implicit transmit completion. */
uint32_t nx_native_eth_read(uintptr_t base, unsigned offset);
/** \brief Model W1C and command side effects at the register store boundary. */
void nx_native_eth_write(uintptr_t base, unsigned offset, uint32_t value);
/** \brief Explicit hardware reset/MDIO/flush acknowledgement progress only. */
void nx_native_eth_poll(nx_eth_mac_t* mac);
#ifdef __cplusplus
}
#endif
#endif
