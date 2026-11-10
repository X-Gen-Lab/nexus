/**
 * \file            usb_device_stack.h
 * \brief           Private ST Device Core over caller-owned Nexus packet loans
 * \author          Nexus Team
 */
#ifndef NEXUS_ST_USB_DEVICE_STACK_H
#define NEXUS_ST_USB_DEVICE_STACK_H
#include "nexus/io/usb_activation.h"
#include "nexus/io/usb_profile.h"
#include "usbd_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One exact selected direction, with explicit caller-owned staging. */
typedef struct {
    nx_usb_request_t request;
    const nx_usb_endpoint_profile_t* endpoint;
    uint8_t* storage;
    size_t capacity;
    uint8_t* bytes;
    size_t length;
    size_t offset;
    size_t packet;
    uint32_t received;
    nx_time_us_t deadline;
    uint64_t epoch;
    bool opened;
    bool busy;
    bool pending;
    bool deferred;
    bool cancel_sent;
    bool stalled;
} nx_st_usb_device_slot_t;
/** \brief Stable selected facts and finite packet budget, without discovery. */
typedef struct {
    const nx_usb_device_profile_t* profile;
    const nx_usb_activation_port_t* activation;
    nx_st_usb_device_slot_t* slots;
    size_t slot_count;
    uint32_t transfer_timeout_us;
    void* class_context;
} nx_st_usb_device_stack_config_t;
/** \brief Private official middleware records never enter public I/O faces.
 * \note Config, class state, descriptor bytes, slots and staging outlive
 *       successful close. ST may write descriptor type bytes. Class callback
 *       records and all protocol/product policy belong to private application
 *       wiring. One serialized privileged unmasked Thread calls step/close;
 *       IRQs publish DCD facts only. No concurrent direct Core calls.
 */
typedef struct {
    USBD_HandleTypeDef middleware;
    USBD_ClassTypeDef selected_class;
    USBD_DescriptorsTypeDef descriptors;
    const nx_st_usb_device_stack_config_t* config;
    nx_usb_device_state_t latest;
    nx_usb_event_t event;
    nx_result_t failure;
    uint64_t epoch;
    uint64_t lost_events;
    nx_time_us_t control_deadline;
    bool event_pending;
    bool joining;
    bool join_all;
    bool closing;
    bool stopped;
    bool executing;
    bool core_initialized;
} nx_st_usb_device_stack_t;
/**
 * \brief Bind fresh caller storage to a started, address-zero exclusive DCD.
 *
 * \param[in,out] state: Zeroed private state, distinct from caller metadata.
 *
 * \param[in] config: Exact selected slots and disjoint aligned staging;
 *                   profile matches any already observed bus speed.
 *
 * \param[in] descriptors: Stable official descriptor callback record, copied.
 *
 * \param[in] selected_class: Stable external class callback record, copied.
 *
 * \return SUCCESS or retained failure requiring explicit close.
 */
nx_result_t
nx_st_usb_device_stack_initialize(nx_st_usb_device_stack_t* state,
                                  const nx_st_usb_device_stack_config_t* config,
                                  const USBD_DescriptorsTypeDef* descriptors,
                                  const USBD_ClassTypeDef* selected_class);
/**
 * \brief Service packets, join old epochs, then advance actual official Core.
 *
 * \param[in,out] state: Same serialized owner; callbacks must not reenter.
 *
 * \return SUCCESS, BUSY or retained first bridge error; no implicit replay.
 *
 * \note One event per call. Reset/new SETUP never revoke accepted borrows.
 */
nx_result_t nx_st_usb_device_stack_step(nx_st_usb_device_stack_t* state);
/**
 * \brief Stop DCD and join every packet before class deinitialization.
 *
 * \param[in,out] state: All class, descriptor, slot and staging storage alive.
 *
 * \return SUCCESS only after proven hardware, IRQ and Core withdrawal.
 */
nx_result_t nx_st_usb_device_stack_close(nx_st_usb_device_stack_t* state);
#ifdef __cplusplus
}
#endif
#endif
