/**
 * \file            config_backend.h
 * \brief           Config Manager Backend Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef CONFIG_BACKEND_H
#define CONFIG_BACKEND_H

#include "config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        CONFIG_BACKEND Config Backend
 * \brief           Config backend interface and built-in backends
 * \{
 */

/**
 * \brief           Forward declaration of backend structure
 */
typedef struct config_backend config_backend_t;

/**
 * \brief           Backend initialization function type
 * \param[in]       ctx: Backend context
 * \return          CONFIG_OK on success, error code otherwise
 */
typedef config_status_t (*config_backend_init_fn)(void* ctx);

/**
 * \brief           Backend deinitialization function type
 * \param[in]       ctx: Backend context
 * \return          CONFIG_OK on success, error code otherwise
 */
typedef config_status_t (*config_backend_deinit_fn)(void* ctx);

/**
 * \brief           Backend read function type
 * \param[in]       ctx: Backend context
 * \param[in]       key: Key to read
 * \param[out]      data: Buffer to store data
 * \param[in,out]   size: Input: buffer size, Output: actual data size
 * \return          CONFIG_OK on success, error code otherwise
 */
typedef config_status_t (*config_backend_read_fn)(void* ctx, const char* key,
                                                  void* data, size_t* size);

/**
 * \brief           Backend write function type
 * \param[in]       ctx: Backend context
 * \param[in]       key: Key to write
 * \param[in]       data: Data to write
 * \param[in]       size: Data size
 * \return          CONFIG_OK on success, error code otherwise
 */
typedef config_status_t (*config_backend_write_fn)(void* ctx, const char* key,
                                                   const void* data,
                                                   size_t size);

/**
 * \brief           Backend erase function type
 * \param[in]       ctx: Backend context
 * \param[in]       key: Key to erase
 * \return          CONFIG_OK on success, error code otherwise
 */
typedef config_status_t (*config_backend_erase_fn)(void* ctx, const char* key);

/**
 * \brief           Backend erase all function type
 * \param[in]       ctx: Backend context
 * \return          CONFIG_OK on success, error code otherwise
 */
typedef config_status_t (*config_backend_erase_all_fn)(void* ctx);

/**
 * \brief           Backend commit function type
 * \param[in]       ctx: Backend context
 * \return          CONFIG_OK on success, error code otherwise
 */
typedef config_status_t (*config_backend_commit_fn)(void* ctx);

/**
 * \brief           Config backend structure
 *
 * Defines the interface for config storage backends.
 * Each backend must implement at least read, write, and erase functions.
 */
struct config_backend {
    const char* name;            /**< Backend name (must be unique) */
    config_backend_init_fn init; /**< Initialization function (optional) */
    config_backend_deinit_fn
        deinit;                    /**< Deinitialization function (optional) */
    config_backend_read_fn read;   /**< Read function (required) */
    config_backend_write_fn write; /**< Write function (required) */
    config_backend_erase_fn erase; /**< Erase function (required) */
    config_backend_erase_all_fn erase_all; /**< Erase all function (optional) */
    config_backend_commit_fn commit;       /**< Commit function (optional) */
    void* ctx;                             /**< Backend-specific context */
};

/**
 * \}
 */

/**
 * \defgroup        CONFIG_BACKEND_RAM RAM Backend
 * \brief           RAM backend for volatile storage
 * \{
 */

/**
 * \brief           Get the RAM backend instance
 * \return          Pointer to RAM backend structure
 * \note            RAM backend is volatile - data is lost on reset
 */
const config_backend_t* config_backend_ram_get(void);

/**
 * \}
 */

/**
 * \defgroup        CONFIG_BACKEND_FLASH Flash Backend
 * \brief           Flash backend for persistent storage
 * \{
 */

/**
 * \brief           Get the Flash backend instance
 * \return          Pointer to Flash backend structure
 * \note            Flash backend provides persistent storage
 */
const config_backend_t* config_backend_flash_get(void);

/**
 * \}
 */

/**
 * \defgroup        CONFIG_BACKEND_MOCK Mock Backend
 * \brief           Mock backend for testing
 * \{
 */

/**
 * \brief           Get the Mock backend instance
 * \return          Pointer to Mock backend structure
 * \note            Mock backend is for testing purposes
 */
const config_backend_t* config_backend_mock_get(void);

/**
 * \brief           Reset the Mock backend state
 * \note            Clears all stored data in mock backend
 */
void config_backend_mock_reset(void);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_BACKEND_H */
