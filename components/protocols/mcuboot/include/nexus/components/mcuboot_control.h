/**
 * \file            mcuboot_control.h
 * \brief           Stateless control of a fixed MCUboot overwrite-only image.
 * \author          Nexus Team
 *
 * The selected single-image profile supports permanent requests only. This
 * API neither downloads nor verifies an image, resets hardware, confirms a
 * trial image, or provides rollback. Applications own authorization, valid
 * candidate bytes, maintenance scheduling, platform lifetime and reset policy.
 */
#ifndef NEXUS_MCUBOOT_CONTROL_H
#define NEXUS_MCUBOOT_CONTROL_H

#include "nexus/core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Requested upgrade behavior; TEST is explicitly unsupported. */
typedef enum {
    NX_MCUBOOT_REQUEST_TEST,
    NX_MCUBOOT_REQUEST_PERMANENT
} nx_mcuboot_request_t;

/**
 * \brief Upstream observed action, not a physical or image-health verdict.
 * \note TEST and REVERT can be observed in foreign trailer metadata; their
 * presence does not add support for those modes to overwrite-only firmware.
 */
typedef enum {
    NX_MCUBOOT_ACTION_NONE,
    NX_MCUBOOT_ACTION_TEST,
    NX_MCUBOOT_ACTION_PERMANENT,
    NX_MCUBOOT_ACTION_REVERT,
    NX_MCUBOOT_ACTION_FAILED
} nx_mcuboot_action_t;

/**
 * \brief Query upstream's planned action for image zero.
 * \param action Receives a value on success only; never retained.
 * \return NX_SUCCESS for a known action, NX_ERROR_INVALID for NULL, or
 * NX_ERROR_IO for upstream PANIC, an unknown value or a reported read error.
 * \note Task/boot context only. Caller serializes access and supplies the
 * immutable Flash operation hook and its finite deadline for the whole call.
 * All referenced areas, regions, clock and provider remain alive through
 * return. The pinned upstream API treats an unreadable secondary trailer as
 * empty; NONE therefore does not establish secondary availability or image
 * validity.
 */
nx_result_t nx_mcuboot_query(nx_mcuboot_action_t* action);

/**
 * \brief Request a permanent upgrade using the upstream public trailer API.
 * \param request PERMANENT requests an update; TEST rejects without access.
 * \return NX_SUCCESS after upstream completes and reports PERMANENT on
 * readback, NX_ERROR_UNSUPPORTED for TEST, NX_ERROR_INVALID for an unknown
 * request, NX_ERROR_STATE for another known readback action, or NX_ERROR_IO
 * for an upstream error. An error can follow partial persistent trailer
 * modification, including a pending TEST that cannot be silently promoted.
 * \note Same serialized finite operation/lifetime contract as query. Return
 * settles current Flash access; a timeout is not a physical pulse abort. This
 * does not prove candidate authenticity, promise old-image recovery, renew
 * the deadline, stop infrastructure or reset the MCU.
 */
nx_result_t nx_mcuboot_request(nx_mcuboot_request_t request);

#ifdef __cplusplus
}
#endif
#endif
