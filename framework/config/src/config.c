/**
 * \file            config.c
 * \brief           Config Manager Core Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Core implementation of the Config Manager including
 *                  initialization, deinitialization, and error handling.
 */

#include "config/config.h"
#include "config_backend_internal.h"
#include "config_callback.h"
#include "config_crypto.h"
#include "config_default.h"
#include "config_error.h"
#include "config_namespace.h"
#include "config_store.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Config manager initialized flag
 */
static bool g_config_initialized = false;

/**
 * \brief           Config manager configuration
 */
static config_manager_config_t g_config;

/**
 * \brief           Last error code
 */
static config_status_t g_last_error = CONFIG_OK;

/*---------------------------------------------------------------------------*/
/* Error String Table                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Error code to string mapping
 */
static const char* const g_error_strings[] = {
    "OK",                  /* CONFIG_OK */
    "Error",               /* CONFIG_ERROR */
    "Invalid parameter",   /* CONFIG_ERROR_INVALID_PARAM */
    "Not initialized",     /* CONFIG_ERROR_NOT_INIT */
    "Already initialized", /* CONFIG_ERROR_ALREADY_INIT */
    "No memory",           /* CONFIG_ERROR_NO_MEMORY */
    "Not found",           /* CONFIG_ERROR_NOT_FOUND */
    "Already exists",      /* CONFIG_ERROR_ALREADY_EXISTS */
    "Type mismatch",       /* CONFIG_ERROR_TYPE_MISMATCH */
    "Key too long",        /* CONFIG_ERROR_KEY_TOO_LONG */
    "Value too large",     /* CONFIG_ERROR_VALUE_TOO_LARGE */
    "Buffer too small",    /* CONFIG_ERROR_BUFFER_TOO_SMALL */
    "No space",            /* CONFIG_ERROR_NO_SPACE */
    "NVS read error",      /* CONFIG_ERROR_NVS_READ */
    "NVS write error",     /* CONFIG_ERROR_NVS_WRITE */
    "Invalid format",      /* CONFIG_ERROR_INVALID_FORMAT */
    "No encryption key",   /* CONFIG_ERROR_NO_ENCRYPTION_KEY */
    "Crypto failed",       /* CONFIG_ERROR_CRYPTO_FAILED */
    "No backend",          /* CONFIG_ERROR_NO_BACKEND */
};

/*---------------------------------------------------------------------------*/
/* Initialization Functions                                                  */
/*---------------------------------------------------------------------------*/

config_status_t config_init(const config_manager_config_t* config) {
    if (g_config_initialized) {
        g_last_error = CONFIG_ERROR_ALREADY_INIT;
        return CONFIG_ERROR_ALREADY_INIT;
    }

    /* Use default config if NULL */
    if (config == NULL) {
        g_config.max_keys = CONFIG_DEFAULT_MAX_KEYS;
        g_config.max_key_len = CONFIG_DEFAULT_MAX_KEY_LEN;
        g_config.max_value_size = CONFIG_DEFAULT_MAX_VALUE_SIZE;
        g_config.max_namespaces = CONFIG_DEFAULT_MAX_NAMESPACES;
        g_config.max_callbacks = CONFIG_DEFAULT_MAX_CALLBACKS;
        g_config.auto_commit = false;
    } else {
        /* Validate configuration */
        if (config->max_keys < CONFIG_MIN_MAX_KEYS ||
            config->max_keys > CONFIG_MAX_MAX_KEYS) {
            g_last_error = CONFIG_ERROR_INVALID_PARAM;
            return CONFIG_ERROR_INVALID_PARAM;
        }
        if (config->max_key_len < CONFIG_MIN_MAX_KEY_LEN ||
            config->max_key_len > CONFIG_MAX_MAX_KEY_LEN) {
            g_last_error = CONFIG_ERROR_INVALID_PARAM;
            return CONFIG_ERROR_INVALID_PARAM;
        }
        if (config->max_value_size < CONFIG_MIN_MAX_VALUE_SIZE ||
            config->max_value_size > CONFIG_MAX_MAX_VALUE_SIZE) {
            g_last_error = CONFIG_ERROR_INVALID_PARAM;
            return CONFIG_ERROR_INVALID_PARAM;
        }

        memcpy(&g_config, config, sizeof(config_manager_config_t));
    }

    /* Initialize the store */
    config_status_t status = config_store_init(
        g_config.max_keys, g_config.max_key_len, g_config.max_value_size);
    if (status != CONFIG_OK) {
        g_last_error = status;
        return status;
    }

    /* Initialize the namespace manager */
    status = config_namespace_init(g_config.max_namespaces);
    if (status != CONFIG_OK) {
        config_store_deinit();
        g_last_error = status;
        return status;
    }

    /* Initialize the callback manager */
    status = config_callback_init(g_config.max_callbacks);
    if (status != CONFIG_OK) {
        config_namespace_deinit();
        config_store_deinit();
        g_last_error = status;
        return status;
    }

    g_config_initialized = true;
    g_last_error = CONFIG_OK;
    return CONFIG_OK;
}

config_status_t config_deinit(void) {
    if (!g_config_initialized) {
        g_last_error = CONFIG_ERROR_NOT_INIT;
        return CONFIG_ERROR_NOT_INIT;
    }

    /* Deinitialize backend */
    config_backend_deinit();

    /* Clear crypto state */
    config_crypto_clear();

    /* Deinitialize callback manager */
    config_callback_deinit();

    /* Deinitialize namespace manager */
    config_namespace_deinit();

    config_status_t status = config_store_deinit();
    if (status != CONFIG_OK) {
        g_last_error = status;
        return status;
    }

    /* Clear all registered defaults */
    config_default_clear_all();

    g_config_initialized = false;
    memset(&g_config, 0, sizeof(g_config));
    g_last_error = CONFIG_OK;
    return CONFIG_OK;
}

bool config_is_initialized(void) {
    return g_config_initialized;
}

/*---------------------------------------------------------------------------*/
/* Error Handling Functions                                                  */
/*---------------------------------------------------------------------------*/

void config_set_last_error(config_status_t status) {
    g_last_error = status;
}

config_status_t config_get_last_error(void) {
    return g_last_error;
}

const char* config_error_to_str(config_status_t status) {
    if (status >= sizeof(g_error_strings) / sizeof(g_error_strings[0])) {
        return "Unknown error";
    }
    return g_error_strings[status];
}

/*---------------------------------------------------------------------------*/
/* Query Functions                                                           */
/*---------------------------------------------------------------------------*/

config_status_t config_exists(const char* key, bool* exists) {
    if (!g_config_initialized) {
        g_last_error = CONFIG_ERROR_NOT_INIT;
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || exists == NULL) {
        g_last_error = CONFIG_ERROR_INVALID_PARAM;
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_status_t status =
        config_store_exists(key, CONFIG_DEFAULT_NAMESPACE_ID, exists);
    g_last_error = status;
    return status;
}

config_status_t config_get_type(const char* key, config_type_t* type) {
    if (!g_config_initialized) {
        g_last_error = CONFIG_ERROR_NOT_INIT;
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || type == NULL) {
        g_last_error = CONFIG_ERROR_INVALID_PARAM;
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_status_t status =
        config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, type);
    g_last_error = status;
    return status;
}

config_status_t config_delete(const char* key) {
    if (!g_config_initialized) {
        g_last_error = CONFIG_ERROR_NOT_INIT;
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL) {
        g_last_error = CONFIG_ERROR_INVALID_PARAM;
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_status_t status =
        config_store_delete(key, CONFIG_DEFAULT_NAMESPACE_ID);
    g_last_error = status;
    return status;
}

config_status_t config_get_count(size_t* count) {
    if (!g_config_initialized) {
        g_last_error = CONFIG_ERROR_NOT_INIT;
        return CONFIG_ERROR_NOT_INIT;
    }

    if (count == NULL) {
        g_last_error = CONFIG_ERROR_INVALID_PARAM;
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_status_t status = config_store_get_count(count);
    g_last_error = status;
    return status;
}

/*---------------------------------------------------------------------------*/
/* Backend Functions                                                         */
/*---------------------------------------------------------------------------*/

config_status_t config_set_backend(const config_backend_t* backend) {
    if (!g_config_initialized) {
        g_last_error = CONFIG_ERROR_NOT_INIT;
        return CONFIG_ERROR_NOT_INIT;
    }

    config_status_t status = config_backend_set(backend);
    g_last_error = status;

    /* Set auto-commit mode based on config */
    if (status == CONFIG_OK) {
        config_backend_set_auto_commit(g_config.auto_commit);
    }

    return status;
}

config_status_t config_commit(void) {
    if (!g_config_initialized) {
        g_last_error = CONFIG_ERROR_NOT_INIT;
        return CONFIG_ERROR_NOT_INIT;
    }

    config_status_t status = config_backend_commit();
    g_last_error = status;
    return status;
}

config_status_t config_load(void) {
    if (!g_config_initialized) {
        g_last_error = CONFIG_ERROR_NOT_INIT;
        return CONFIG_ERROR_NOT_INIT;
    }

    config_status_t status = config_backend_load();
    g_last_error = status;
    return status;
}
