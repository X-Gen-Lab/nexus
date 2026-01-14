/**
 * \file            config_callback.h
 * \brief           Config Manager Callback Internal Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Internal header for callback notification functionality.
 *                  This header is not part of the public API.
 */

#ifndef CONFIG_CALLBACK_H
#define CONFIG_CALLBACK_H

#include "config/config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Initialize the callback manager
 * \param[in]       max_callbacks: Maximum number of callbacks
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_callback_init(uint8_t max_callbacks);

/**
 * \brief           Deinitialize the callback manager
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_callback_deinit(void);

/**
 * \brief           Check if callback manager is initialized
 * \return          true if initialized, false otherwise
 */
bool config_callback_is_initialized(void);

/**
 * \brief           Notify callbacks of a value change
 * \param[in]       key: Configuration key that changed
 * \param[in]       type: Value type
 * \param[in]       old_value: Previous value (may be NULL for new keys)
 * \param[in]       old_size: Size of old value
 * \param[in]       new_value: New value
 * \param[in]       new_size: Size of new value
 * \return          CONFIG_OK on success, error code otherwise
 * \note            This function is called internally when a value changes
 */
config_status_t config_callback_notify(const char* key, config_type_t type,
                                       const void* old_value, size_t old_size,
                                       const void* new_value, size_t new_size);

/**
 * \brief           Get the number of registered callbacks
 * \param[out]      count: Pointer to store count
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_callback_get_count(size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_CALLBACK_H */
