/**
 * \file            usb_device_controller.h
 * \brief           Private PIO full-speed DWC device mechanism
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_USB_DEVICE_CONTROLLER_H
#define NEXUS_PRIVATE_USB_DEVICE_CONTROLLER_H
/** \brief Private code selection, derived from the sole typed assembly. */
#ifndef NEXUS_STM32F4_USB_REMOTE_WAKEUP
#define NEXUS_STM32F4_USB_REMOTE_WAKEUP 0
#endif
#include "nexus/arch/arch.h"
#include "nexus/io/dma.h"
#include "nexus/io/usb_activation.h"
#include "nexus/io/usb_device.h"
#include "nexus/io/usb_profile.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Cold-only actual 48MHz domain; no hot register access table. */
typedef struct {
    nx_result_t (*retain)(void*);
    nx_result_t (*release)(void*);
    nx_result_t (*validate)(const void*);
    /** \brief Live Core epoch under caller IRQ exclusion, zero if absent. */
    uint64_t (*epoch)(const void*);
} nx_usb_clock_ops_t;
typedef struct {
    const nx_usb_clock_ops_t* ops;
    void* context;
    size_t context_size;
} nx_usb_clock_domain_t;
typedef struct nx_usb_controller nx_usb_controller_t;
/** \brief Optional selected remote-wakeup runtime, no worker or second owner.
 * \note Static storage outlives controller stop; packet-only plans use NULL.
 */
typedef struct {
    /** \brief Suspend observation, then the post-assertion minimum bound. */
    nx_time_us_t time_us;
    /** \brief Pre-assertion bound also exposes delay before timestamping. */
    nx_time_us_t before_us;
    uint64_t time_epoch;
    nx_usb_remote_wakeup_request_t* request;
    uint8_t flags;
} nx_usb_remote_runtime_t;
/** \brief Per-direction static DMA bounce storage, separate from user loans. */
typedef struct {
    uint8_t* bytes;
    size_t capacity;
} nx_usb_dma_storage_t;
enum { NX_USB_CORE_FS, NX_USB_CORE_HS_INTERNAL_FS, NX_USB_CORE_HS_ULPI };
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
    /** \brief ISO request target in the actual-speed hardware frame domain. */
    uint16_t target_frame;
    uint8_t number;
    bool in;
    /** \brief Validated profile transfer value, stored in one byte. */
    uint8_t transfer;
    bool halted;
    bool rx_seen;
    bool disable_requested;
    /** \brief Packet completion and independent pending DMA SETUP receipts. */
    uint8_t observed;
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
    const nx_usb_dma_storage_t* dma_storage;
    const nx_dma_memory_region_t* dma_regions;
    size_t dma_region_count;
    uint16_t rx_fifo_words;
    uint8_t core;
    uint8_t turnaround;
    uint8_t pending_address;
    bool address_pending;
    bool clock_retained;
    bool reset_pending;
    bool setup_pending;
    bool vbus_sensing;
    bool dma;
    nx_usb_remote_runtime_t* remote;
};
extern const nx_usb_endpoint_ops_t nx_usb_fs_endpoint_ops;
extern const nx_usb_device_ops_t nx_usb_fs_device_ops;
extern const nx_usb_activation_ops_t nx_usb_fs_activation_ops;
extern const nx_usb_setup_decision_ops_t nx_usb_fs_setup_decision_ops;
/** \brief Cold privileged start, pins/electrical role already authorized. */
nx_result_t nx_usb_controller_initialize(nx_usb_controller_t* controller);
/** \brief Pure complete plan validation before pin/clock acquisition.
 * \param[in] controller: Fully bound static facts and unborrowed storage.
 * \return SUCCESS or INVALID without calling clock ops or accessing MMIO.
 */
nx_result_t nx_usb_controller_plan_validate(nx_usb_controller_t* controller);
/** \brief Derive PHY turnaround from selected actual AHB frequency.
 * \param[in] core: Exact FS, HS embedded FS or HS ULPI core selection.
 * \param[in] ahb_hz: Selected HCLK, not the USB PLL48 frequency.
 * \return Register TRDT value, or zero below the reviewed clock floor.
 * \note Pure bounded facts; no MMIO, allocation or clock changes.
 */
uint8_t nx_usb_turnaround(uint8_t core, uint32_t ahb_hz);
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
