/**
 * \file            usb_device_controller.h
 * \brief           Private PIO full-speed DWC device mechanism
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_USB_DEVICE_CONTROLLER_H
#define NEXUS_PRIVATE_USB_DEVICE_CONTROLLER_H
#include "nexus/arch/arch.h"
#include "nexus/io/usb_device.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Cold-only actual 48MHz domain; no hot register access table. */
typedef struct {
    nx_result_t (*retain)(void*);
    nx_result_t (*release)(void*);
    nx_result_t (*validate)(const void*);
} nx_usb_clock_ops_t;
typedef struct {
    const nx_usb_clock_ops_t* ops;
    void* context;
    size_t context_size;
} nx_usb_clock_domain_t;
typedef struct nx_usb_controller nx_usb_controller_t;
/** \brief Exact selected direction; caller state never allocates or discovers.
 */
typedef struct {
    nx_usb_controller_t* controller;
    nx_usb_request_t* request;
    nx_time_us_t drain_deadline;
    size_t received;
    nx_result_t terminal;
    uint16_t fifo_start;
    uint16_t fifo_words;
    uint16_t max_packet;
    uint8_t number;
    bool in;
    bool bulk;
    bool halted;
    bool rx_seen;
    bool disable_requested;
} nx_usb_endpoint_t;
/** \brief One selected static controller, endpoint array and exact event ring.
 */
struct nx_usb_controller {
    uintptr_t registers;
    nx_usb_clock_domain_t clock;
    nx_usb_endpoint_t* endpoints;
    size_t endpoint_count;
    nx_usb_event_t* events;
    size_t capacity;
    size_t head;
    size_t count;
    uint64_t lost;
    nx_time_us_t loss_time;
    nx_irq_policy_t irq_policy;
    nx_irq_source_t irq;
    const nx_irq_wake_t* wake;
    nx_usb_device_state_t snapshot;
    size_t control_transferred;
    nx_usb_stage_t control_stage;
    uint32_t publishers;
    uint32_t poll_limit;
    uint32_t drain_timeout_us;
    uint16_t rx_fifo_words;
    uint8_t pending_address;
    bool address_pending;
    bool clock_retained;
    bool reset_pending;
    bool setup_pending;
    bool vbus_sensing;
};
extern const nx_usb_endpoint_ops_t nx_usb_fs_endpoint_ops;
extern const nx_usb_device_ops_t nx_usb_fs_device_ops;
/** \brief Cold privileged start, pins/electrical role already authorized. */
nx_result_t nx_usb_controller_initialize(nx_usb_controller_t* controller);
/** \brief Bounded PIO IRQ executor; never calls a class/product callback. */
void nx_usb_controller_irq(nx_usb_controller_t* controller);
#ifndef NX_USB_READ
#define NX_USB_READ(base, offset)                                              \
    (*(volatile uint32_t*)((uintptr_t)(base) + (uintptr_t)(offset)))
#endif
#ifndef NX_USB_WRITE
#define NX_USB_WRITE(base, offset, value)                                      \
    (NX_USB_READ((base), (offset)) = (uint32_t)(value))
#endif
#ifndef NX_USB_POLL
#define NX_USB_POLL(base) ((void)(base))
#endif
#ifdef __cplusplus
}
#endif
#endif
