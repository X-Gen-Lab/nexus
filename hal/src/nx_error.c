/**
 * \file            nx_error.c
 * \brief           Error information implementation (simplified)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-17
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/system/nx_error.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Private Variables                                                         */
/*---------------------------------------------------------------------------*/

static nx_error_info_t g_last_error = {0};
static bool g_error_valid = false;
static nx_error_handler_t g_error_handler = NULL;
static void* g_error_handler_user_data = NULL;

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Set global error handler
 */
nx_status_t nx_set_error_handler(nx_error_handler_t handler, void* user_data) {
    g_error_handler = handler;
    g_error_handler_user_data = user_data;
    return NX_OK;
}

/**
 * \brief           Get last error information
 */
nx_status_t nx_get_last_error(nx_error_info_t* error) {
    if (error == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!g_error_valid) {
        return NX_ERR_NOT_FOUND;
    }

    memcpy(error, &g_last_error, sizeof(nx_error_info_t));
    return NX_OK;
}

/**
 * \brief           Clear error state
 */
void nx_clear_error(void) {
    g_error_valid = false;
    memset(&g_last_error, 0, sizeof(nx_error_info_t));
}

/**
 * \brief           Check if device handle is valid (for USB/SDIO presence
 * detection)
 * \details         This function checks if a device pointer is non-NULL.
 *                  For removable devices like USB and SDIO, this indicates
 * presence.
 */
bool nx_device_is_present(void* device) {
    return device != NULL;
}

/**
 * \brief           Internal function to record error
 * \note            This function is called internally by HAL when errors occur
 */
void nx_record_error(nx_status_t code, void* source, uint32_t timestamp) {
    g_last_error.code = code;
    g_last_error.source = source;
    g_last_error.timestamp = timestamp;
    g_error_valid = true;

    /* Call error handler if registered */
    if (g_error_handler != NULL) {
        g_error_handler(&g_last_error, g_error_handler_user_data);
    }
}
