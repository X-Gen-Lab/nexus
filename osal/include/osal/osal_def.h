/**
 * \file            osal_def.h
 * \brief           OSAL Common Definitions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef OSAL_DEF_H
#define OSAL_DEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL_DEF OSAL Definitions
 * \brief           Common definitions for OSAL layer
 * \{
 */

/**
 * \brief           OSAL status codes
 */
typedef enum {
    OSAL_OK = 0,                  /**< Operation successful */
    OSAL_ERROR = 1,               /**< Generic error */
    OSAL_ERROR_INVALID_PARAM = 2, /**< Invalid parameter */
    OSAL_ERROR_NULL_POINTER = 3,  /**< Null pointer */
    OSAL_ERROR_NO_MEMORY = 4,     /**< Out of memory */
    OSAL_ERROR_TIMEOUT = 5,       /**< Operation timeout */
    OSAL_ERROR_NOT_INIT = 6,      /**< Not initialized */
    OSAL_ERROR_BUSY = 7,          /**< Resource busy */
    OSAL_ERROR_NOT_FOUND = 8,     /**< Resource not found */
    OSAL_ERROR_FULL = 9,          /**< Queue/buffer full */
    OSAL_ERROR_EMPTY = 10,        /**< Queue/buffer empty */
    OSAL_ERROR_ISR = 11,          /**< Called from ISR context */
} osal_status_t;

/**
 * \brief           Wait forever timeout value
 */
#define OSAL_WAIT_FOREVER UINT32_MAX

/**
 * \brief           No wait timeout value
 */
#define OSAL_NO_WAIT 0

/**
 * \brief           Check if status is OK
 */
#define OSAL_IS_OK(status) ((status) == OSAL_OK)

/**
 * \brief           Check if status is error
 */
#define OSAL_IS_ERROR(status) ((status) != OSAL_OK)

/*---------------------------------------------------------------------------*/
/* Unified Error Handling Macros                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Validate pointer is not NULL
 * \param[in]       ptr: Pointer to validate
 * \return          Returns OSAL_ERROR_NULL_POINTER if ptr is NULL
 *
 * \note            Requirements: 1.2
 */
#define OSAL_VALIDATE_PTR(ptr)                                                 \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            return OSAL_ERROR_NULL_POINTER;                                    \
        }                                                                      \
    } while (0)

/**
 * \brief           Validate condition or return error
 * \param[in]       cond: Condition to validate
 * \param[in]       err: Error code to return if condition is false
 * \return          Returns err if condition is false
 *
 * \note            Requirements: 1.3
 */
#define OSAL_VALIDATE_PARAM(cond, err)                                         \
    do {                                                                       \
        if (!(cond)) {                                                         \
            return (err);                                                      \
        }                                                                      \
    } while (0)

/**
 * \brief           Check not in ISR context
 * \return          Returns OSAL_ERROR_ISR if called from ISR
 *
 * \note            Requirements: 1.4
 * \note            Requires osal_is_isr() to be declared before use
 */
#define OSAL_CHECK_NOT_ISR()                                                   \
    do {                                                                       \
        if (osal_is_isr()) {                                                   \
            return OSAL_ERROR_ISR;                                             \
        }                                                                      \
    } while (0)

/**
 * \brief           Validate handle (basic NULL check version)
 * \param[in]       handle: Handle to validate
 * \return          Returns OSAL_ERROR_NULL_POINTER if handle is NULL
 *
 * \note            Requirements: 1.1
 * \note            For full handle validation with magic number checking,
 *                  use OSAL_VALIDATE_HANDLE from osal_internal.h
 */
#define OSAL_VALIDATE_HANDLE_PTR(handle)                                       \
    do {                                                                       \
        if ((handle) == NULL) {                                                \
            return OSAL_ERROR_NULL_POINTER;                                    \
        }                                                                      \
    } while (0)

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_DEF_H */
