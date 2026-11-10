/**
 * \file            usb_host_stack.h
 * \brief           Private official Host core over existing Nexus packet faces
 * \author          Nexus Team
 */
#ifndef NEXUS_ST_USB_HOST_STACK_H
#define NEXUS_ST_USB_HOST_STACK_H
#include "nexus/io/usb_host.h"
#include "usbh_core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    nx_usb_host_request_t request;
    nx_usb_host_pipe_t pipe;
    uint8_t* storage;
    size_t capacity;
    uint8_t* bytes;
    size_t length;
    size_t offset;
    uint32_t transferred;
    USBH_URBStateTypeDef urb;
    bool configured;
    bool busy;
    bool pending;
    bool toggle;
    bool setup;
} nx_st_usb_host_slot_t;

typedef struct {
    const nx_usb_host_port_t* transport;
    nx_st_usb_host_slot_t* slots;
    size_t slot_count;
    nx_result_t (*vbus)(void*, bool);
    void* power_context;
    uint32_t transfer_timeout_us;
    uint32_t reset_timeout_us;
    nx_result_t (*restart)(void*, nx_time_us_t);
    void* restart_context;
} nx_st_usb_host_stack_config_t;

/** \brief Selected mutable middleware state stays behind the private boundary.
 * \note No concurrent USBH_Process or direct LL calls are allowed. The exact
 *       config, slots, staging and class state outlive successful close.
 */
typedef struct {
    USBH_HandleTypeDef middleware;
    USBH_ClassTypeDef selected_class;
    const nx_st_usb_host_stack_config_t* config;
    nx_result_t failure;
    uint32_t milliseconds;
    bool connected;
    bool enabled;
    bool closing;
    bool stopped;
} nx_st_usb_host_stack_t;

/** \brief Bind one initialized transport and explicit physical VBUS owner.
 * \param[in,out] state: Fresh private middleware storage, unborrowed.
 * \param[in] config: Immutable actual packet, staging and finite timing plan.
 * \param[in] selected_class: Optional caller-owned ST class record, copied.
 * \param[in] user: Optional explicit middleware event callback.
 * \return SUCCESS or failure retaining exact effects for close.
 */
nx_result_t
nx_st_usb_host_stack_initialize(nx_st_usb_host_stack_t* state,
                                const nx_st_usb_host_stack_config_t* config,
                                USBH_ClassTypeDef* selected_class,
                                void (*user)(USBH_HandleTypeDef*, uint8_t));
/** \brief Poll packet settlement then advance official enumeration/class core.
 * \param[in,out] state: Same serialized privileged task owner.
 * \return SUCCESS, BUSY or retained transport/bridge error.
 */
nx_result_t nx_st_usb_host_stack_step(nx_st_usb_host_stack_t* state);
/** \brief Drain the packet owner before withdrawing VBUS or class storage.
 * \param[in,out] state: Exact private owner with all staging still alive.
 * \return SUCCESS only after complete transport and middleware withdrawal.
 */
nx_result_t nx_st_usb_host_stack_close(nx_st_usb_host_stack_t* state);
#ifdef __cplusplus
}
#endif
#endif
