/**
 * \file            usb_activation.h
 * \brief           Optional explicit nonzero USB endpoint activation
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_USB_ACTIVATION_H
#define NEXUS_IO_USB_ACTIVATION_H
#include "nexus/io/usb_device.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Optional controller operation; absence means UNSUPPORTED. */
typedef struct {
    nx_result_t (*set)(void* context, uint64_t epoch, uint8_t endpoint_address,
                       bool active);
} nx_usb_activation_ops_t;
/** \brief Immutable borrowed face, using the existing controller context. */
typedef struct {
    const nx_usb_activation_ops_t* ops;
    void* context;
} nx_usb_activation_port_t;
/**
 * \brief Explicitly activate or deactivate one selected nonzero direction.
 * \param[in] port: Started serialized privileged Task owner, IRQ masks clear.
 * \param[in] epoch: Current software USB epoch; cold epoch zero is valid.
 * \param[in] endpoint_address: USB address byte; EP0 is not admitted.
 * \param[in] active: Requested active state, never packet submission.
 * \return Success after proof; UNSUPPORTED when the operation is absent.
 *         INVALID/CONTEXT/STATE/BUSY reject before writes. IO may have partial
 *         effects and closes controller admission until explicit recovery.
 * \note No request or buffer is borrowed. Existing loans must be explicitly
 *       cancelled and drained before target changes; failure never revokes
 *       other loans. Configuration/class policy remains external.
 */
nx_result_t nx_usb_activation_set(const nx_usb_activation_port_t* port,
                                  uint64_t epoch, uint8_t endpoint_address,
                                  bool active);
#ifdef __cplusplus
}
#endif
#endif
