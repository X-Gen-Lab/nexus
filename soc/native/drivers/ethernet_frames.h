/**
 * \file            ethernet_frames.h
 * \brief           Explicit Native enhanced DMA and hardware-counter model
 * \author          Nexus Team
 * \note            Explicit software wire events do not qualify physical I/O.
 */
#ifndef NEXUS_NATIVE_ETHERNET_FRAMES_H
#define NEXUS_NATIVE_ETHERNET_FRAMES_H
#include "eth_frames_mac.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t registers[0x1060U / 4U];
    uint16_t* phy_registers;
    uint64_t hardware_nanoseconds;
    size_t tx_cursor;
    size_t rx_cursor;
    uint8_t phy_address;
    uint8_t priority;
    uint8_t priority_group;
    bool irq_enabled;
    bool irq_pending;
    bool hold_reset;
    bool hold_engine;
    bool hold_ptp;
    bool hold_interrupt_clear;
} nx_native_eth_frames_hardware_t;
typedef struct {
    nx_eth_frames_mac_t mac;
    nx_native_eth_frames_hardware_t hardware;
} nx_native_eth_frames_t;
/** \brief Initialize selected C model; caller owns all DMA metadata/buffers. */
nx_result_t nx_native_eth_frames_initialize(nx_native_eth_frames_t* instance,
                                            nx_time_us_t deadline);
/**
 * \brief Explicit one-frame egress, validating all physical-model fragments.
 * \note Insufficient output capacity preserves OWN and cursors. Retire only
 *       after the complete frame was copied. This never invokes service/IRQ or
 *       releases the request: actual provider observes written status later.
 *       A full-pointer/instance binding resolves modeled 32-bit DMA addresses;
 *       truncated equality alone must not authorize a different payload.
 */
nx_result_t nx_native_eth_frames_transmit(nx_native_eth_frames_t* instance,
                                          void* output, size_t capacity,
                                          size_t* length,
                                          nx_result_t wire_result,
                                          bool captured);
/**
 * \brief Explicit one-frame ingress including generated model FCS storage.
 * \note Whole-ring admission precedes every write. A HELD/CPU-owned descriptor
 *       returns OVERFLOW, records RBU and writes none of the frame. Timestamp
 *       comes only from this instance's explicitly advanced hardware counter.
 */
nx_result_t nx_native_eth_frames_receive(nx_native_eth_frames_t* instance,
                                         const void* frame, size_t length,
                                         bool captured);
/** \brief Advance only this model's MAC counter, independently of core time. */
nx_result_t nx_native_eth_frames_tick(nx_native_eth_frames_t* instance,
                                      uint64_t nanoseconds);
#ifdef __cplusplus
}
#endif
#endif
