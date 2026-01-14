/**
 * \file            config_flash_backend.c
 * \brief           Config Manager Flash Backend Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements a Flash-based storage backend for Config Manager.
 *                  This backend provides persistent storage across resets.
 *                  Supports wear leveling hints for flash storage optimization.
 *
 *                  Note: This is a simulated flash backend for native platform.
 *                  On actual embedded targets, this would interface with
 *                  platform-specific flash drivers.
 */

#include "config/config_backend.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Maximum number of entries in Flash backend
 */
#ifndef CONFIG_FLASH_BACKEND_MAX_ENTRIES
#define CONFIG_FLASH_BACKEND_MAX_ENTRIES 128
#endif

/**
 * \brief           Maximum key length in Flash backend
 */
#ifndef CONFIG_FLASH_BACKEND_MAX_KEY_LEN
#define CONFIG_FLASH_BACKEND_MAX_KEY_LEN 64
#endif

/**
 * \brief           Maximum value size in Flash backend
 */
#ifndef CONFIG_FLASH_BACKEND_MAX_VALUE_SIZE
#define CONFIG_FLASH_BACKEND_MAX_VALUE_SIZE 512
#endif

/**
 * \brief           Flash page size for wear leveling
 */
#ifndef CONFIG_FLASH_PAGE_SIZE
#define CONFIG_FLASH_PAGE_SIZE 4096
#endif

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Flash backend entry structure
 */
typedef struct {
    char key[CONFIG_FLASH_BACKEND_MAX_KEY_LEN];
    uint8_t data[CONFIG_FLASH_BACKEND_MAX_VALUE_SIZE];
    size_t size;
    bool in_use;
    uint32_t write_count; /**< Write count for wear leveling hints */
} flash_backend_entry_t;

/**
 * \brief           Flash backend context structure
 */
typedef struct {
    bool initialized;
    flash_backend_entry_t entries[CONFIG_FLASH_BACKEND_MAX_ENTRIES];
    size_t entry_count;
    bool dirty;            /**< Uncommitted changes flag */
    uint32_t total_writes; /**< Total write operations */
    uint32_t total_erases; /**< Total erase operations */
} flash_backend_ctx_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Global Flash backend context
 */
static flash_backend_ctx_t g_flash_ctx;

/*---------------------------------------------------------------------------*/
/* Internal Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find entry by key
 * \param[in]       key: Key to find
 * \return          Pointer to entry if found, NULL otherwise
 */
static flash_backend_entry_t* flash_backend_find_entry(const char* key) {
    if (key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < CONFIG_FLASH_BACKEND_MAX_ENTRIES; ++i) {
        if (g_flash_ctx.entries[i].in_use &&
            strncmp(g_flash_ctx.entries[i].key, key,
                    CONFIG_FLASH_BACKEND_MAX_KEY_LEN) == 0) {
            return &g_flash_ctx.entries[i];
        }
    }
    return NULL;
}

/**
 * \brief           Find free entry slot with wear leveling
 * \return          Pointer to free entry if available, NULL otherwise
 * \note            Prefers slots with lower write counts for wear leveling
 */
static flash_backend_entry_t* flash_backend_find_free_entry(void) {
    flash_backend_entry_t* best_entry = NULL;
    uint32_t min_write_count = UINT32_MAX;

    for (size_t i = 0; i < CONFIG_FLASH_BACKEND_MAX_ENTRIES; ++i) {
        if (!g_flash_ctx.entries[i].in_use) {
            /* Prefer entry with lowest write count for wear leveling */
            if (g_flash_ctx.entries[i].write_count < min_write_count) {
                min_write_count = g_flash_ctx.entries[i].write_count;
                best_entry = &g_flash_ctx.entries[i];
            }
        }
    }
    return best_entry;
}

/*---------------------------------------------------------------------------*/
/* Backend Interface Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize Flash backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t flash_backend_init(void* ctx) {
    (void)ctx;

    /* Don't clear existing data - flash is persistent */
    if (!g_flash_ctx.initialized) {
        memset(&g_flash_ctx, 0, sizeof(g_flash_ctx));
    }

    g_flash_ctx.initialized = true;
    g_flash_ctx.dirty = false;

    return CONFIG_OK;
}

/**
 * \brief           Deinitialize Flash backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t flash_backend_deinit(void* ctx) {
    (void)ctx;

    /* Don't clear data - flash is persistent */
    g_flash_ctx.initialized = false;
    return CONFIG_OK;
}

/**
 * \brief           Read data from Flash backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to read
 * \param[out]      data: Buffer to store data
 * \param[in,out]   size: Input: buffer size, Output: actual data size
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t flash_backend_read(void* ctx, const char* key,
                                          void* data, size_t* size) {
    (void)ctx;

    if (!g_flash_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    flash_backend_entry_t* entry = flash_backend_find_entry(key);
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
 * \brief           Write data to Flash backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to write
 * \param[in]       data: Data to write
 * \param[in]       size: Data size
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t flash_backend_write(void* ctx, const char* key,
                                           const void* data, size_t size) {
    (void)ctx;

    if (!g_flash_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL || data == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= CONFIG_FLASH_BACKEND_MAX_KEY_LEN) {
        return CONFIG_ERROR_KEY_TOO_LONG;
    }

    if (size > CONFIG_FLASH_BACKEND_MAX_VALUE_SIZE) {
        return CONFIG_ERROR_VALUE_TOO_LARGE;
    }

    /* Find existing entry or allocate new one */
    flash_backend_entry_t* entry = flash_backend_find_entry(key);

    if (entry == NULL) {
        entry = flash_backend_find_free_entry();
        if (entry == NULL) {
            return CONFIG_ERROR_NO_SPACE;
        }
        g_flash_ctx.entry_count++;
    }

    /* Store the entry */
    memset(entry->key, 0, sizeof(entry->key));
    size_t copy_len = key_len < (CONFIG_FLASH_BACKEND_MAX_KEY_LEN - 1)
                          ? key_len
                          : (CONFIG_FLASH_BACKEND_MAX_KEY_LEN - 1);
    memcpy(entry->key, key, copy_len);
    entry->key[copy_len] = '\0';

    memcpy(entry->data, data, size);
    entry->size = size;
    entry->in_use = true;
    entry->write_count++;

    g_flash_ctx.total_writes++;
    g_flash_ctx.dirty = true;

    return CONFIG_OK;
}

/**
 * \brief           Erase data from Flash backend
 * \param[in]       ctx: Backend context (unused)
 * \param[in]       key: Key to erase
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t flash_backend_erase(void* ctx, const char* key) {
    (void)ctx;

    if (!g_flash_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    flash_backend_entry_t* entry = flash_backend_find_entry(key);
    if (entry == NULL) {
        return CONFIG_ERROR_NOT_FOUND;
    }

    /* Clear the entry but preserve write_count for wear leveling */
    uint32_t write_count = entry->write_count;
    memset(entry, 0, sizeof(*entry));
    entry->write_count = write_count;
    entry->in_use = false;

    g_flash_ctx.entry_count--;
    g_flash_ctx.total_erases++;
    g_flash_ctx.dirty = true;

    return CONFIG_OK;
}

/**
 * \brief           Erase all data from Flash backend
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 */
static config_status_t flash_backend_erase_all(void* ctx) {
    (void)ctx;

    if (!g_flash_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    for (size_t i = 0; i < CONFIG_FLASH_BACKEND_MAX_ENTRIES; ++i) {
        /* Preserve write_count for wear leveling */
        uint32_t write_count = g_flash_ctx.entries[i].write_count;
        memset(&g_flash_ctx.entries[i], 0, sizeof(g_flash_ctx.entries[i]));
        g_flash_ctx.entries[i].write_count = write_count;
    }

    g_flash_ctx.entry_count = 0;
    g_flash_ctx.total_erases++;
    g_flash_ctx.dirty = true;

    return CONFIG_OK;
}

/**
 * \brief           Commit changes to Flash
 * \param[in]       ctx: Backend context (unused)
 * \return          CONFIG_OK on success
 * \note            In a real implementation, this would flush to actual flash
 */
static config_status_t flash_backend_commit(void* ctx) {
    (void)ctx;

    if (!g_flash_ctx.initialized) {
        return CONFIG_ERROR_NOT_INIT;
    }

    /* In a real implementation, this would:
     * 1. Calculate CRC/checksum
     * 2. Write to flash with atomic operation
     * 3. Verify write success
     *
     * For simulation, we just clear the dirty flag
     */
    g_flash_ctx.dirty = false;

    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Backend Instance                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Flash backend instance
 */
static const config_backend_t g_flash_backend = {.name = "flash",
                                                 .init = flash_backend_init,
                                                 .deinit = flash_backend_deinit,
                                                 .read = flash_backend_read,
                                                 .write = flash_backend_write,
                                                 .erase = flash_backend_erase,
                                                 .erase_all =
                                                     flash_backend_erase_all,
                                                 .commit = flash_backend_commit,
                                                 .ctx = NULL};

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

const config_backend_t* config_backend_flash_get(void) {
    return &g_flash_backend;
}
