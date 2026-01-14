/**
 * \file            config_query.c
 * \brief           Config Manager Query and Enumeration Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements query and enumeration functionality for the
 *                  Config Manager. Provides functions to check key existence,
 *                  get types, delete keys, count entries, and iterate over
 *                  configuration entries.
 *
 *                  Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6
 */

#include "config/config.h"
#include "config_namespace.h"
#include "config_store.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Iteration adapter context
 * \details         Used to adapt between internal store iteration callback
 *                  and public API iteration callback
 */
typedef struct {
    config_iterate_cb_t callback; /**< User callback */
    void* user_data;              /**< User data */
} config_iterate_adapter_ctx_t;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Adapter callback for iteration
 * \param[in]       info: Store entry information
 * \param[in]       user_data: Adapter context
 * \return          true to continue, false to stop
 */
static bool config_iterate_adapter(const config_store_entry_info_t* info,
                                   void* user_data) {
    config_iterate_adapter_ctx_t* ctx =
        (config_iterate_adapter_ctx_t*)user_data;

    if (ctx == NULL || ctx->callback == NULL || info == NULL) {
        return false;
    }

    /* Convert store entry info to public entry info */
    config_entry_info_t public_info;
    memset(&public_info, 0, sizeof(public_info));

    /* Copy key */
    size_t key_len = strlen(info->key);
    size_t copy_len = key_len < (CONFIG_DEFAULT_MAX_KEY_LEN - 1)
                          ? key_len
                          : (CONFIG_DEFAULT_MAX_KEY_LEN - 1);
    memcpy(public_info.key, info->key, copy_len);
    public_info.key[copy_len] = '\0';

    public_info.type = info->type;
    public_info.value_size = info->value_size;
    public_info.flags = info->flags;

    return ctx->callback(&public_info, ctx->user_data);
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Iterate over all configuration entries
 * \param[in]       callback: Iteration callback
 * \param[in]       user_data: User-provided context
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_iterate(config_iterate_cb_t callback, void* user_data) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (callback == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Set up adapter context */
    config_iterate_adapter_ctx_t ctx = {.callback = callback,
                                        .user_data = user_data};

    return config_store_iterate(config_iterate_adapter, &ctx);
}

/**
 * \brief           Iterate over entries in a namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       callback: Iteration callback
 * \param[in]       user_data: User-provided context
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_iterate(config_ns_handle_t ns,
                                  config_iterate_cb_t callback,
                                  void* user_data) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns == NULL || callback == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Validate namespace handle */
    if (!config_namespace_is_valid_handle(ns)) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get namespace ID from handle */
    uint8_t ns_id;
    config_status_t status = config_namespace_get_handle_id(ns, &ns_id);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Set up adapter context */
    config_iterate_adapter_ctx_t ctx = {.callback = callback,
                                        .user_data = user_data};

    return config_store_iterate_namespace(ns_id, config_iterate_adapter, &ctx);
}
