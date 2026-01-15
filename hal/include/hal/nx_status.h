/**
 * \file            nx_status.h
 * \brief           Nexus platform unified status/error codes
 * \author          Nexus Team
 *
 * This file defines the unified error handling system for the Nexus HAL.
 * All HAL functions return nx_status_t to indicate success or failure.
 *
 * Naming convention: NX_ERR_<CATEGORY>_<SPECIFIC_ERROR>
 * - NX_OK: Success
 * - NX_ERR_*: Error codes
 */

#ifndef NX_STATUS_H
#define NX_STATUS_H

#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Unified status/error code enumeration
 *
 * Error codes are grouped by category:
 * - 0: Success
 * - 1-19: General errors
 * - 20-39: State errors
 * - 40-59: Resource errors
 * - 60-79: Timeout errors
 * - 80-99: IO errors
 * - 100-119: DMA errors
 * - 120-139: Data errors
 * - 140-159: Permission errors
 */
typedef enum nx_status_e {
    /* Success */
    NX_OK = 0, /**< Operation successful */

    /* General errors (1-19) */
    NX_ERR_GENERIC = 1,       /**< Generic error */
    NX_ERR_INVALID_PARAM = 2, /**< Invalid parameter */
    NX_ERR_NULL_PTR = 3,      /**< Null pointer */
    NX_ERR_NOT_SUPPORTED = 4, /**< Operation not supported */
    NX_ERR_NOT_FOUND = 5,     /**< Item not found */
    NX_ERR_INVALID_SIZE = 6,  /**< Invalid size */

    /* State errors (20-39) */
    NX_ERR_NOT_INIT = 20,      /**< Not initialized */
    NX_ERR_ALREADY_INIT = 21,  /**< Already initialized */
    NX_ERR_INVALID_STATE = 22, /**< Invalid state */
    NX_ERR_BUSY = 23,          /**< Device busy */
    NX_ERR_SUSPENDED = 24,     /**< Device suspended */

    /* Resource errors (40-59) */
    NX_ERR_NO_MEMORY = 40,     /**< Out of memory */
    NX_ERR_NO_RESOURCE = 41,   /**< Resource unavailable */
    NX_ERR_RESOURCE_BUSY = 42, /**< Resource busy */
    NX_ERR_LOCKED = 43,        /**< Resource locked */
    NX_ERR_FULL = 44,          /**< Buffer/queue full */
    NX_ERR_EMPTY = 45,         /**< Buffer/queue empty */

    /* Timeout errors (60-79) */
    NX_ERR_TIMEOUT = 60,     /**< Operation timeout */
    NX_ERR_WOULD_BLOCK = 61, /**< Operation would block */

    /* IO errors (80-99) */
    NX_ERR_IO = 80,          /**< IO error */
    NX_ERR_OVERRUN = 81,     /**< Buffer overrun */
    NX_ERR_UNDERRUN = 82,    /**< Buffer underrun */
    NX_ERR_PARITY = 83,      /**< Parity error */
    NX_ERR_FRAMING = 84,     /**< Framing error */
    NX_ERR_NOISE = 85,       /**< Noise error */
    NX_ERR_NACK = 86,        /**< NACK received (I2C) */
    NX_ERR_BUS = 87,         /**< Bus error */
    NX_ERR_ARBITRATION = 88, /**< Arbitration lost */

    /* DMA errors (100-119) */
    NX_ERR_DMA = 100,          /**< DMA error */
    NX_ERR_DMA_TRANSFER = 101, /**< DMA transfer error */
    NX_ERR_DMA_CONFIG = 102,   /**< DMA configuration error */

    /* Data errors (120-139) */
    NX_ERR_NO_DATA = 120,   /**< No data available */
    NX_ERR_DATA_SIZE = 121, /**< Data size error */
    NX_ERR_CRC = 122,       /**< CRC check error */
    NX_ERR_CHECKSUM = 123,  /**< Checksum error */

    /* Permission errors (140-159) */
    NX_ERR_PERMISSION = 140, /**< Permission denied */
    NX_ERR_READ_ONLY = 141,  /**< Read-only resource */

    NX_ERR_MAX /**< Maximum error code value (for bounds checking) */
} nx_status_t;

/**
 * \brief           Check if status indicates success
 * \param[in]       status: Status code to check
 * \return          true if status is NX_OK, false otherwise
 */
#define NX_IS_OK(status) ((status) == NX_OK)

/**
 * \brief           Check if status indicates error
 * \param[in]       status: Status code to check
 * \return          true if status is not NX_OK, false otherwise
 */
#define NX_IS_ERROR(status) ((status) != NX_OK)

/**
 * \brief           Return from function if error occurred
 * \param[in]       status: Status code to check
 *
 * This macro evaluates the status and returns immediately if it indicates
 * an error. Useful for propagating errors up the call stack.
 */
#define NX_RETURN_IF_ERROR(status)                                             \
    do {                                                                       \
        nx_status_t __nx_status = (status);                                    \
        if (NX_IS_ERROR(__nx_status)) {                                        \
            return __nx_status;                                                \
        }                                                                      \
    } while (0)

/**
 * \brief           Return from function if pointer is NULL
 * \param[in]       ptr: Pointer to check
 */
#define NX_RETURN_IF_NULL(ptr)                                                 \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            return NX_ERR_NULL_PTR;                                            \
        }                                                                      \
    } while (0)

/**
 * \brief           Goto label if error occurred
 * \param[in]       status: Status code to check
 * \param[in]       label: Label to jump to on error
 * \param[out]      result: Variable to store the status
 */
#define NX_GOTO_IF_ERROR(status, label, result)                                \
    do {                                                                       \
        (result) = (status);                                                   \
        if (NX_IS_ERROR(result)) {                                             \
            goto label;                                                        \
        }                                                                      \
    } while (0)

/**
 * \brief           Convert status code to human-readable string
 * \param[in]       status: Status code
 * \return          String description of the status code
 */
const char* nx_status_to_string(nx_status_t status);

/**
 * \brief           Error callback function type
 * \param[in]       user_data: User data pointer passed during registration
 * \param[in]       status: Status code that triggered the callback
 * \param[in]       module: Module name where error occurred (may be NULL)
 * \param[in]       msg: Error message (may be NULL)
 */
typedef void (*nx_error_callback_t)(void* user_data, nx_status_t status,
                                    const char* module, const char* msg);

/**
 * \brief           Set global error callback
 * \param[in]       callback: Callback function (NULL to disable)
 * \param[in]       user_data: User data pointer passed to callback
 *
 * The error callback is invoked when errors occur in HAL functions.
 * This provides a centralized error notification mechanism.
 */
void nx_set_error_callback(nx_error_callback_t callback, void* user_data);

/**
 * \brief           Report an error through the global callback
 * \param[in]       status: Status code
 * \param[in]       module: Module name (may be NULL)
 * \param[in]       msg: Error message (may be NULL)
 *
 * This function is called internally by HAL modules to report errors.
 * If a callback is registered, it will be invoked.
 */
void nx_report_error(nx_status_t status, const char* module, const char* msg);

#ifdef __cplusplus
}
#endif

#endif /* NX_STATUS_H */
