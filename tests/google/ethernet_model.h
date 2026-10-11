/**
 * \file            ethernet_model.h
 * \brief           External DMA, W1C, reset and MDIO hardware boundaries
 * \author          Nexus Team
 */
#ifndef NEXUS_ETHERNET_MODEL_H
#define NEXUS_ETHERNET_MODEL_H
#include "nexus/core/status.h"
#include "nexus/io/wake.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_eth_model_read(uintptr_t base, unsigned offset);
void nx_eth_model_write(uintptr_t base, unsigned offset, uint32_t value);
void nx_eth_model_poll(void* mac);
struct nx_eth_mac;
nx_result_t nx_eth_model_irq_control(struct nx_eth_mac* mac, bool enabled);
nx_result_t nx_eth_model_validate_wake(struct nx_eth_mac* mac,
                                       const nx_irq_wake_t* wake,
                                       uint8_t ceiling);
void nx_eth_model_pend_irq(struct nx_eth_mac* mac);
void nx_eth_model_descriptor(void* descriptor, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_ETH_READ(base, offset) nx_eth_model_read((uintptr_t)(base), (offset))
#define NX_ETH_WRITE(base, offset, value)                                      \
    nx_eth_model_write((uintptr_t)(base), (offset), (uint32_t)(value))
#define NX_ETH_POLL(mac) nx_eth_model_poll(mac)
#define NX_ETH_DESCRIPTOR_STATUS(descriptor, value)                            \
    nx_eth_model_descriptor((descriptor), (uint32_t)(value))
#define NX_ETH_HOST_ADDRESSES    1
#define NX_ETH_SOC_IRQ_CONTROL   nx_eth_model_irq_control
#define NX_ETH_SOC_PEND_IRQ      nx_eth_model_pend_irq
#define NX_ETH_SOC_WAKE_VALIDATE nx_eth_model_validate_wake
#endif
