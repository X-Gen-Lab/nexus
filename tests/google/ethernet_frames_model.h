/**
 * \file            ethernet_frames_model.h
 * \brief           External enhanced-DMA and MAC-clock test boundaries
 * \author          Nexus Team
 * \note            Fault-controlled oracle, separate from physical validation.
 */
#ifndef NEXUS_ETHERNET_FRAMES_MODEL_H
#define NEXUS_ETHERNET_FRAMES_MODEL_H
#include "nexus/io/wake.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_eth_frames_model_read(uintptr_t base, unsigned offset);
void nx_eth_frames_model_write(uintptr_t base, unsigned offset, uint32_t value);
void nx_eth_frames_model_poll(void* context);
void nx_eth_frames_model_descriptor(void* descriptor, uint32_t value);
struct nx_eth_frames_mac;
nx_result_t nx_eth_frames_model_irq_control(struct nx_eth_frames_mac*, bool);
void nx_eth_frames_model_pend_irq(struct nx_eth_frames_mac*);
nx_result_t nx_eth_frames_model_validate_wake(struct nx_eth_frames_mac*,
                                              const nx_irq_wake_t*, uint8_t);
#ifdef __cplusplus
}
#endif
#define NX_ETH_FRAMES_SOC_IRQ_CONTROL   nx_eth_frames_model_irq_control
#define NX_ETH_FRAMES_SOC_PEND_IRQ      nx_eth_frames_model_pend_irq
#define NX_ETH_FRAMES_SOC_WAKE_VALIDATE nx_eth_frames_model_validate_wake
#define NX_ETH_FRAMES_READ(base, offset)                                       \
    nx_eth_frames_model_read((uintptr_t)(base), (offset))
#define NX_ETH_FRAMES_WRITE(base, offset, value)                               \
    nx_eth_frames_model_write((uintptr_t)(base), (offset), (uint32_t)(value))
#define NX_ETH_FRAMES_POLL(mac) nx_eth_frames_model_poll(mac)
#define NX_ETH_FRAMES_DESCRIPTOR_STATUS(descriptor, value)                     \
    nx_eth_frames_model_descriptor((descriptor), (uint32_t)(value))
#endif
