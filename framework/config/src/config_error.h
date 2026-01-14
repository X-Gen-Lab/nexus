/**
 * \file            config_error.h
 * \brief           Config Manager Internal Error Handling Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Internal header for error handling functionality.
 *                  This header is not part of the public API.
 */

#ifndef CONFIG_ERROR_H
#define CONFIG_ERROR_H

#include "config/config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Set the last error code
 * \param[in]       status: Error code to set
 * \note            This function is for internal use only
 */
void config_set_last_error(config_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_ERROR_H */
