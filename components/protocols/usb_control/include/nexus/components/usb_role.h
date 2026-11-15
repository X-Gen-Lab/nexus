/**
 * \file            usb_role.h
 * \brief           Explicit USB role withdrawal and VBUS handover
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_USB_ROLE_H
#define NEXUS_COMPONENTS_USB_ROLE_H

#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_USB_ROLE_OFF,
    NX_USB_ROLE_DEVICE,
    NX_USB_ROLE_HOST
} nx_usb_role_kind_t;

/** \brief One controller transport owner and its reviewed power switch.
 * \note All operations use the same serialized unmasked privileged task.
 *       stop succeeds only after every transfer, IRQ and callback has joined.
 *       start failure may retain effects: stop must remain callable. source
 *       reports success only after the switch command has been read back.
 *       present reads actual VBUS, independently of the requested switch state.
 */
typedef struct {
    nx_result_t (*start)(void*, nx_usb_role_kind_t, nx_time_us_t);
    nx_result_t (*stop)(void*, nx_time_us_t);
    nx_result_t (*source)(void*, bool);
    nx_result_t (*present)(void*, bool*);
} nx_usb_role_backend_ops_t;

typedef struct {
    const nx_usb_role_backend_ops_t* ops;
    void* context;
    bool device_allowed;
    bool host_allowed;
} nx_usb_role_config_t;

/** \brief Caller-owned transaction state; initialize once before publication.
 * \note Config and backend context outlive successful OFF completion. Fields
 *       are modified only by these methods. The same serialized owner may read
 *       active/target/result and phase (zero means transaction complete).
 *       No allocation, ID-pin policy, SRP/HNP,
 *       background executor or automatic role selection is supplied.
 */
typedef struct {
    const nx_usb_role_config_t* config;
    nx_time_us_t deadline;
    nx_result_t result;
    nx_usb_role_kind_t active;
    nx_usb_role_kind_t target;
    uint8_t phase;
    bool transport_owned;
    bool source_owned;
    bool discharge_required;
} nx_usb_role_t;

/**
 * \brief Bind an idle role owner without touching hardware or power.
 * \param[out] role: Fresh unpublished storage, disjoint from config/context.
 * \param[in] config: Immutable complete backend and allowed-role facts.
 * \return SUCCESS or INVALID; failure leaves output unchanged.
 * \note Task-only. Initialization must never replace a live owner.
 */
nx_result_t nx_usb_role_initialize(nx_usb_role_t* role,
                                   const nx_usb_role_config_t* config);

/**
 * \brief Request a role transaction, retaining the old transport until drained.
 * \param[in,out] role: Initialized single task owner.
 * \param[in] target: Explicit OFF, DEVICE or HOST; no policy is inferred.
 * \param[in] deadline: Finite absolute Core deadline for the complete handover.
 * \return SUCCESS accepts the transition; BUSY retains the current transition.
 * \note REJECTED creates no effects. Call service until phase becomes zero.
 *       Source power is never enabled while VBUS is already externally present.
 */
nx_result_t nx_usb_role_request(nx_usb_role_t* role, nx_usb_role_kind_t target,
                                nx_time_us_t deadline);

/** \brief Cancel activation and finish OFF through the same drain barriers.
 * \param[in,out] role: Same serialized owner, including a pending transition.
 * \param[in] deadline: Fresh finite absolute deadline for retained cleanup.
 * \return SUCCESS accepts cleanup; it never revokes a transport or packet.
 * \note Service remains required. Its final result is CANCELLED, or an earlier
 *       failure. A failed stop still prevents source-off and replacement start.
 */
nx_result_t nx_usb_role_cancel(nx_usb_role_t* role, nx_time_us_t deadline);

/**
 * \brief Advance one bounded handover step, including failed-start cleanup.
 * \param[in,out] role: Same owner with backend/config storage still alive.
 * \return BUSY while work remains; final SUCCESS or original transition error.
 * \note stop failure never permits power withdrawal or a replacement start.
 *       Timeout cancels activation but does not revoke transport ownership.
 *       HOST withdrawal observes actual VBUS discharge before DEVICE
 * activation. DEVICE activation waits for externally supplied VBUS. No sleeps
 * occur.
 */
nx_result_t nx_usb_role_service(nx_usb_role_t* role);

#ifdef __cplusplus
}
#endif
#endif
