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

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_DEF_H */
