/**
 * \file            config_default.h
 * \brief           Config Manager Default Value Internal Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Internal header for default value management functionality.
 *                  This header is not part of the public API.
 */

#ifndef CONFIG_DEFAULT_H
#define CONFIG_DEFAULT_H

#include "config/config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Get default value for a key
 * \param[in]       key: Configuration key
 * \param[out]      type: Value type (optional, can be NULL)
 * \param[out]      value: Buffer to store value
 * \param[in,out]   size: Input: buffer size, Output: actual size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_default(const char* key, config_type_t* type,
                                    void* value, size_t* size);

/**
 * \brief           Check if a default value exists for a key
 * \param[in]       key: Configuration key
 * \param[out]      exists: Pointer to store result
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_has_default(const char* key, bool* exists);

/**
 * \brief           Clear all registered defaults
 * \note            Called from config_deinit
 */
void config_default_clear_all(void);

/**
 * \brief           Set default value for a 32-bit unsigned integer
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_u32(const char* key, uint32_t value);

/**
 * \brief           Set default value for a 64-bit signed integer
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_i64(const char* key, int64_t value);

/**
 * \brief           Set default value for a float
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_float(const char* key, float value);

/**
 * \brief           Set default value for a boolean
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_bool(const char* key, bool value);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_DEFAULT_H */
