/**
 * \file            osal_event.h
 * \brief           OSAL Event Flags Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef OSAL_EVENT_H
#define OSAL_EVENT_H

#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL_EVENT Event Flags
 * \brief           Event flags interface for multi-condition synchronization
 * \{
 */

/**
 * \brief           Event flags handle type
 */
typedef void* osal_event_handle_t;

/**
 * \brief           Event bits type (at least 24 bits)
 */
typedef uint32_t osal_event_bits_t;

/**
 * \brief           Event wait mode enumeration
 */
typedef enum {
    OSAL_EVENT_WAIT_ANY = 0, /**< Wait for any of the specified bits */
    OSAL_EVENT_WAIT_ALL = 1  /**< Wait for all of the specified bits */
} osal_event_wait_mode_t;

/**
 * \brief           Event wait options structure
 */
typedef struct {
    osal_event_wait_mode_t mode; /**< Wait mode (ANY or ALL) */
    bool auto_clear;             /**< Auto-clear matched bits after wait */
    uint32_t timeout_ms;         /**< Timeout in milliseconds */
} osal_event_wait_options_t;

/**
 * \brief           Create event flags
 * \param[out]      handle: Pointer to store event flags handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_event_create(osal_event_handle_t* handle);

/**
 * \brief           Delete event flags
 * \param[in]       handle: Event flags handle
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_event_delete(osal_event_handle_t handle);

/**
 * \brief           Set event bits
 * \param[in]       handle: Event flags handle
 * \param[in]       bits: Bits to set
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_event_set(osal_event_handle_t handle,
                             osal_event_bits_t bits);

/**
 * \brief           Clear event bits
 * \param[in]       handle: Event flags handle
 * \param[in]       bits: Bits to clear
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_event_clear(osal_event_handle_t handle,
                               osal_event_bits_t bits);

/**
 * \brief           Wait for event bits
 * \param[in]       handle: Event flags handle
 * \param[in]       bits: Bits to wait for
 * \param[in]       options: Wait options (mode, auto-clear, timeout)
 * \param[out]      bits_out: Pointer to store actual bits that satisfied condition (optional)
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_event_wait(osal_event_handle_t handle,
                              osal_event_bits_t bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out);

/**
 * \brief           Get current event bits (non-blocking)
 * \param[in]       handle: Event flags handle
 * \return          Current event bits value (0 if handle is NULL)
 */
osal_event_bits_t osal_event_get(osal_event_handle_t handle);

/**
 * \brief           Set event bits from ISR context
 * \param[in]       handle: Event flags handle
 * \param[in]       bits: Bits to set
 * \return          OSAL_OK on success, error code otherwise
 */
osal_status_t osal_event_set_from_isr(osal_event_handle_t handle,
                                      osal_event_bits_t bits);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_EVENT_H */
