/**
 * \file            nx_error.h
 * \brief           Error information interface (simplified)
 * \author          Nexus Team
 */

#ifndef NX_ERROR_H
#define NX_ERROR_H

#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Error information structure
 */
typedef struct nx_error_info_s {
    nx_status_t code;   /**< Error code */
    void* source;       /**< Error source (device pointer) */
    uint32_t timestamp; /**< Error timestamp */
} nx_error_info_t;

/**
 * \brief           Error handler callback
 */
typedef void (*nx_error_handler_t)(const nx_error_info_t* error,
                                   void* user_data);

/*---------------------------------------------------------------------------*/
/* Function Declarations                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set global error handler
 * \param[in]       handler: Error handler callback
 * \param[in]       user_data: User data pointer
 * \return          NX_OK on success
 */
nx_status_t nx_set_error_handler(nx_error_handler_t handler, void* user_data);

/**
 * \brief           Get last error information
 * \param[out]      error: Error information structure
 * \return          NX_OK on success, NX_ERR_NOT_FOUND if no error
 */
nx_status_t nx_get_last_error(nx_error_info_t* error);

/**
 * \brief           Clear error state
 */
void nx_clear_error(void);

/**
 * \brief           Check if device handle is valid (for USB/SDIO presence
 * detection)
 * \param[in]       device: Device pointer
 * \return          true if device is present, false otherwise
 */
bool nx_device_is_present(void* device);

#ifdef __cplusplus
}
#endif

#endif /* NX_ERROR_H */
