/**
 * \file            config_ram_backend.c
 * \brief           Config Manager RAM Backend Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements a RAM-based storage backend for Config Manager.
 *                  This backend provides volatile storage - data is lost on
 * reset. Useful for testing and temporary configuration storage.
 */

#include "config/config_backend.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Maximum number of entries in RAM backend
 */
#ifndef CONFIG_RAM_BACKEND_MAX_ENTRIES
#define CONFIG_RAM_BACKEND_MAX_ENTRIES 128
#endif

/**
 * \brief           Maximum key length in RAM backend
 */
#ifndef CONFIG_RAM_BACKEND_MAX_KEY_LEN
#define CONFIG_RAM_BACKEND_MAX_KEY_LEN 64
#endif

/**
 * \brief           Maximum value size in RAM backend
 */
#ifndef CONFIG_RAM_BACKEND_MAX_VALUE_SIZE
#define CONFIG_RAM_BACKEND_MAX_VALUE_SIZE 512
#endif

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RAM backend entry structure
 */
typedef struct {
    char key[CONFIG_RAM_BACKEND_MAX_KEY_LEN];
    uint8_t data[CONFIG_RAM_BACKEND_MAX_VALUE_SIZE];
    size_t size;
    bool in_use;
} ram_backend_entry_t;

/**
 * \brief           RAM backend context structure
 */
typedef struct {
    bool initialized;
    ram_backend_entry_t entries[CONFIG_RAM_BACKEND_MAX_ENTRIES];
    size_t entry_count;
} ram_backend_ctx_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Global RAM backend context
 */
static ram_backend_ctx_t g_ram_ctx;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find entry by key
 * \param[in]       key: Key to find
 * \return          Pointer to entry if found, NULL otherwise
 */
static ram_backend_entry_t* ram_backend_find_entry(const char* key) {
    if (key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < CONFIG_RAM_BACKEND_MAX_ENTRIES; ++i) {
        if (g_ram_ctx.entries[i].in_use &&
            strncmp(g_ram_ctx.entries[i].key, key,
                    CONFIG_RAM_BACKEND_MAX_KEY_LEN) == 0) {
            return &g_ram_ctx.entries[i];
        }
    }
    return NULL;
}

/**
 * \brief           Find free entry slot
 * \return          Pointer to free entry if available, NULL otherwise
 */
static ram_backend_entry_t* ram_backend_find_free_entry(void) {
    for (size_t i = 0; i < CONFIG_RAM_BACKEND_MAX_ENTRIES; ++i) {
        if (!g_ram_ctx.entries[i].in_use) {
            return &g_ram_ctx.entries[i];
        }
    }
    return NULL;
}

/*---------------------------------------------------------------------------*/
/* Backend Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize RAM backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t ram_backend_init(void* ctx) {
    (void)ctx;

    memset(&g_ram_ctx, 0, sizeof(g_ram_ctx));
    g_ram_ctx.initialized = true;
    g_ram_ctx.entry_count = 0;

    return CONFIG_OK;
}

/**
 * \brief           Deinitialize RAM backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t ram_backend_deinit(void* ctx) {
    (void)ctx;

    memset(&g_ram_ctx, 0, sizeof(g_ram_ctx));
    return CONFIG_OK;
}

/**
 * \brief           Read data from RAM backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to read
 * \param[out]      data: Buffer to store data
 * \param[in,out]   size: Input: buffer size, Output: actual data size
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t ram_backend_read(void* ctx, const char* key, void* data,
                                        size_t* size) {
    (void)ctx;

    if (!g_ram_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    ram_backend_entry_t* entry = ram_backend_find_entry(key);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Check buffer size */
    if (data != NULL && *size < entry->size) {
        *size = entry->size;
        return CONFIG_ERROR_BUFFER_TOO_SMALL;
    }

    /* Copy data */
    if (data != NULL) {
        memcpy(data, entry->data, entry->size);
    }
    *size = entry->size;

    return CONFIG_OK;
}

/**
 * \brief           Write data to RAM backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to write
 * \param[in]       data: Data to write
 * \param[in]       size: Data size
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t ram_backend_write(void* ctx, const char* key,
                                         const void* data, size_t size) {
    (void)ctx;

    if (!g_ram_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || data == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= CONFIG_RAM_BACKEND_MAX_KEY_LEN) {
        return CONFIG_ERROR_KEY_TOO_LONG;
    }

    if (size > CONFIG_RAM_BACKEND_MAX_VALUE_SIZE) {
        return CONFIG_ERROR_VALUE_TOO_LARGE;
    }

    /* Find existing entry or allocate new one */
    ram_backend_entry_t* entry = ram_backend_find_entry(key);

    if (entry == NULL) {
        entry = ram_backend_find_free_entry();
        if (entry == NULL) {
            return CONFIG_ERROR_NO_SPACE;
        }
        g_ram_ctx.entry_count++;
    }

    /* Store the entry */
    memset(entry->key, 0, sizeof(entry->key));
    size_t copy_len = key_len < (CONFIG_RAM_BACKEND_MAX_KEY_LEN - 1)
                          ? key_len
                          : (CONFIG_RAM_BACKEND_MAX_KEY_LEN - 1);
    memcpy(entry->key, key, copy_len);
    entry->key[copy_len] = '\0';

    memcpy(entry->data, data, size);
    entry->size = size;
    entry->in_use = true;

    return CONFIG_OK;
}

/**
 * \brief           Erase data from RAM backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to erase
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t ram_backend_erase(void* ctx, const char* key) {
    (void)ctx;

    if (!g_ram_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    ram_backend_entry_t* entry = ram_backend_find_entry(key);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Clear the entry */
    memset(entry, 0, sizeof(*entry));
    entry->in_use = false;
    g_ram_ctx.entry_count--;

    return CONFIG_OK;
}

/**
 * \brief           Erase all data from RAM backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t ram_backend_erase_all(void* ctx) {
    (void)ctx;

    if (!g_ram_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    for (size_t i = 0; i < CONFIG_RAM_BACKEND_MAX_ENTRIES; ++i) {
        memset(&g_ram_ctx.entries[i], 0, sizeof(g_ram_ctx.entries[i]));
    }
    g_ram_ctx.entry_count = 0;

    return CONFIG_OK;
}

/**
 * \brief           Commit changes (no-op for RAM backend)
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK always
 */
static config_status_t ram_backend_commit(void* ctx) {
    (void)ctx;

    /* RAM backend doesn't need commit - changes are immediate */
    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Backend Instance                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           RAM backend instance
 */
static const config_backend_t g_ram_backend = {.name = "ram",
                                               .init = ram_backend_init,
                                               .deinit = ram_backend_deinit,
                                               .read = ram_backend_read,
                                               .write = ram_backend_write,
                                               .erase = ram_backend_erase,
                                               .erase_all =
                                                   ram_backend_erase_all,
                                               .commit = ram_backend_commit,
                                               .ctx = NULL};

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

const config_backend_t* config_backend_ram_get(void) {
    return &g_ram_backend;
}
