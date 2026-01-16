/**
 * \file            osal_timer.h
 * \brief           OSAL Timer Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef OSAL_TIMER_H
#define OSAL_TIMER_H

#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL_TIMER Timer
 * \brief           Software timer interface for periodic and one-shot callbacks
 * \{
 */

/**
 * \brief           Timer handle type
 */
typedef void* osal_timer_handle_t;

/**
 * \brief           Timer callback function type
 * \param[in]       arg: User-provided argument
 */
typedef void (*osal_timer_callback_t)(void* arg);

/**
 * \brief           Timer mode enumeration
 */
typedef enum {
    OSAL_TIMER_ONE_SHOT = 0, /**< Timer fires once */
    OSAL_TIMER_PERIODIC = 1  /**< Timer fires repeatedly */
} osal_timer_mode_t;

/**
 * \brief           Timer configuration structure
 */
typedef struct {
    const char* name;               /**< Timer name (optional, can be NULL) */
    uint32_t period_ms;             /**< Timer period in milliseconds */
    osal_timer_mode_t mode;         /**< Timer mode (one-shot or periodic) */
    osal_timer_callback_t callback; /**< Callback function */
    void* arg;                      /**< Callback argument */
} osal_timer_config_t;

/**
 * \brief           Create a timer
 * \param[in]       config: Timer configuration
 * \param[out]      handle: Pointer to store timer handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Timer created successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM: callback is NULL or period is zero
 * \retval          OSAL_ERROR_NO_MEMORY: Memory allocation failed
 */
osal_status_t osal_timer_create(const osal_timer_config_t* config,
                                osal_timer_handle_t* handle);

/**
 * \brief           Delete a timer
 * \param[in]       handle: Timer handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Timer deleted successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 */
osal_status_t osal_timer_delete(osal_timer_handle_t handle);

/**
 * \brief           Start a timer
 * \param[in]       handle: Timer handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Timer started successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 */
osal_status_t osal_timer_start(osal_timer_handle_t handle);

/**
 * \brief           Stop a timer
 * \param[in]       handle: Timer handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Timer stopped successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 */
osal_status_t osal_timer_stop(osal_timer_handle_t handle);

/**
 * \brief           Reset a timer (restart countdown)
 * \param[in]       handle: Timer handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Timer reset successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 */
osal_status_t osal_timer_reset(osal_timer_handle_t handle);

/**
 * \brief           Change timer period
 * \param[in]       handle: Timer handle
 * \param[in]       period_ms: New period in milliseconds
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Period changed successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 * \retval          OSAL_ERROR_INVALID_PARAM: period_ms is zero
 */
osal_status_t osal_timer_set_period(osal_timer_handle_t handle,
                                    uint32_t period_ms);

/**
 * \brief           Check if timer is active
 * \param[in]       handle: Timer handle
 * \return          true if active, false otherwise (also false if handle is
 * NULL)
 */
bool osal_timer_is_active(osal_timer_handle_t handle);

/**
 * \brief           Start timer from ISR context
 * \param[in]       handle: Timer handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Timer started successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 * \note            This function is safe to call from interrupt context
 */
osal_status_t osal_timer_start_from_isr(osal_timer_handle_t handle);

/**
 * \brief           Stop timer from ISR context
 * \param[in]       handle: Timer handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Timer stopped successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 * \note            This function is safe to call from interrupt context
 */
osal_status_t osal_timer_stop_from_isr(osal_timer_handle_t handle);

/**
 * \brief           Reset timer from ISR context
 * \param[in]       handle: Timer handle
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK: Timer reset successfully
 * \retval          OSAL_ERROR_NULL_POINTER: handle is NULL
 * \note            This function is safe to call from interrupt context
 */
osal_status_t osal_timer_reset_from_isr(osal_timer_handle_t handle);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_TIMER_H */
