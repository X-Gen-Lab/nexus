/**
 * \file            usb_otg.h
 * \brief           Explicit managed USB role and generation-bound face access
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_USB_OTG_H
#define NEXUS_IO_USB_OTG_H
#include "nexus/io/usb_device.h"
#include "nexus/io/usb_host.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum nx_usb_otg_role {
    NX_USB_OTG_OFF,
    NX_USB_OTG_DEVICE,
    NX_USB_OTG_HOST
} nx_usb_otg_role_t;
typedef struct {
    uint64_t generation;
    nx_usb_otg_role_t active;
    nx_usb_otg_role_t target;
    nx_result_t result;
    nx_result_t fault;
    bool busy;
    bool ready;
    bool source_owned;
} nx_usb_otg_state_t;
typedef struct {
    nx_result_t (*request)(void*, uint64_t, nx_usb_otg_role_t, nx_time_us_t);
    nx_result_t (*service)(void*);
    nx_result_t (*state)(const void*, nx_usb_otg_state_t*);
    const nx_usb_device_port_t* (*device)(const void*, uint64_t);
    const nx_usb_host_port_t* (*host)(const void*, uint64_t);
} nx_usb_otg_ops_t;
struct nx_usb_otg_port {
    const nx_usb_otg_ops_t* ops;
    void* context;
};
/** \brief Request an explicit role using the acquire-observed generation.
 * \param[in] port: Stable initialized composite owner, serialized task only.
 * \param[in] generation: Current generation; stale controls reject purely.
 * \param[in] role: OFF, DEVICE or HOST; no ID/SRP/HNP policy is inferred.
 * \param[in] deadline: Finite absolute Core deadline for the handover.
 * \return SUCCESS accepts; failed drain never revokes old packet loans.
 */
nx_result_t nx_usb_otg_request(const nx_usb_otg_port_t* port,
                               uint64_t generation, nx_usb_otg_role_t role,
                               nx_time_us_t deadline);
/** \brief Advance one bounded role, drain and power observation step.
 * \param[in] port: Same privileged unmasked task and stable owner.
 * \return BUSY while pending, or final transition/latched power error.
 */
nx_result_t nx_usb_otg_service(const nx_usb_otg_port_t* port);
/** \brief Copy authoritative role facts without hardware side effects.
 * \param[in] port: Stable owner; factory lookup never initializes it.
 * \param[out] state: Disjoint caller storage, unchanged on rejection.
 * \return SUCCESS or rejection before modifying caller storage.
 */
nx_result_t nx_usb_otg_state(const nx_usb_otg_port_t* port,
                             nx_usb_otg_state_t* state);
/** \brief Return the stable Device face only in its ready current generation.
 * \param[in] port: Stable initialized owner.
 * \param[in] generation: Current generation obtained from state.
 * \return Existing immutable face or NULL; never starts a transport.
 */
const nx_usb_device_port_t* nx_usb_otg_device(const nx_usb_otg_port_t* port,
                                              uint64_t generation);
/** \brief Return the stable Host face only in its ready current generation.
 * \param[in] port: Stable initialized owner.
 * \param[in] generation: Current generation obtained from state.
 * \return Existing immutable face or NULL; never starts a transport.
 */
const nx_usb_host_port_t* nx_usb_otg_host(const nx_usb_otg_port_t* port,
                                          uint64_t generation);
#ifdef __cplusplus
}
#endif
#endif
