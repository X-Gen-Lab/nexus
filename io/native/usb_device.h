/**
 * \file            usb_device.h
 * \brief           Optional Native full-speed hardware facts and fault controls
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_USB_DEVICE_H
#define NEXUS_NATIVE_USB_DEVICE_H
#include "usb_device_controller.h"
typedef struct {
    uint32_t status;
    uint32_t words[16];
} nx_native_usb_packet_t;
typedef struct {
    uint32_t registers[1024];
    uint32_t tx[4][16];
    size_t tx_count[4];
    nx_native_usb_packet_t rx[16];
    size_t rx_head;
    size_t rx_count;
    nx_native_usb_packet_t current;
    size_t rx_word;
    uint32_t consumers;
    uint32_t reset_reads;
    void (*on_interrupt_enable)(void*);
    void* interrupt_context;
    void (*on_source_disable)(void*);
    void* disable_context;
    uint8_t priority;
    bool irq_enabled;
    bool clock_ready;
    bool hold_reset;
    bool hold_flush;
    bool hold_disable;
    bool hold_gonak;
    bool hold_irq_disable;
    bool hold_clear;
    bool hold_address_clear;
    uint8_t hold_stall_clear_mask;
    bool hold_in_nak;
    bool hold_disconnect;
    bool early_in_disable;
    bool out_zero_disable_requested;
    bool reserved_ep0_pid_command;
    bool hold_bulk_pid;
} nx_native_usb_hardware_t;
typedef struct {
    nx_usb_controller_t controller;
    nx_native_usb_hardware_t hardware;
} nx_native_usb_device_t;
nx_result_t nx_native_usb_initialize(nx_native_usb_device_t* model);
nx_result_t nx_native_usb_receive(nx_native_usb_device_t* model,
                                  uint8_t endpoint, const uint8_t* bytes,
                                  size_t length, bool setup);
nx_result_t nx_native_usb_complete(nx_native_usb_device_t* model,
                                   uint8_t endpoint, bool in);
void nx_native_usb_reset(nx_native_usb_device_t* model);
uint32_t nx_native_usb_read(uintptr_t base, uint32_t offset);
void nx_native_usb_write(uintptr_t base, uint32_t offset, uint32_t value);
void nx_native_usb_irq_enable(uintptr_t base, int irq, uint8_t priority,
                              uint8_t bits);
void nx_native_usb_irq_disable(uintptr_t base, int irq);
bool nx_native_usb_irq_enabled(uintptr_t base, int irq);
uint8_t nx_native_usb_irq_priority(uintptr_t base, int irq, uint8_t bits);
#endif
