/**
 * \file            settings.c
 * \brief           Sorted bounded settings with explicit durable commits
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/settings.h"
#include <string.h>

#define ENTRY_HEADER_SIZE 8u

static const uint8_t s_settings_magic[4] = {'N', 'X', 'K', 'V'};

/** \brief Encode a little-endian field without native alignment assumptions. */
static void put32(uint8_t* output, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        output[i] = (uint8_t)(value >> (i * 8u));
}

/** \brief Decode a bounded little-endian field. */
static uint32_t get32(const uint8_t* input) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= (uint32_t)input[i] << (i * 8u);
    return value;
}

/** \brief Check overlap without constructing overflowing end pointers. */
static bool overlaps(const void* left, size_t left_size, const void* right,
                     size_t right_size) {
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    return left_size != 0 && right_size != 0 &&
           (a <= b ? b - a < left_size : a - b < right_size);
}

/** \brief Require an established image and the still-open underlying store. */
static bool usable(const nx_settings_t* settings) {
    return settings && settings->opened && settings->storage &&
           settings->storage->opened;
}

/** \brief Prevent API outputs from overwriting borrowed state or scratch. */
static bool protected_range(const nx_settings_t* settings, const void* data,
                            size_t size) {
    const nx_storage_t* storage = settings->storage;
    return overlaps(data, size, settings, sizeof(*settings)) ||
           overlaps(data, size, settings->image, settings->capacity) ||
           overlaps(data, size, storage, sizeof(*storage)) ||
           overlaps(data, size, storage->workspace, storage->workspace_bytes);
}

/** \brief Validate the whole stored image before publishing any key. */
static nx_storage_status_t validate(nx_settings_t* settings) {
    const uint8_t* image = settings->image;
    if (settings->size < NX_SETTINGS_HEADER_SIZE ||
        memcmp(image, s_settings_magic, sizeof(s_settings_magic)) != 0)
        return NX_STORAGE_CORRUPT;
    if (get32(image + 4) != 1 || get32(image + 8) != settings->schema)
        return NX_STORAGE_UNSUPPORTED;
    uint32_t count = get32(image + 12);
    if (count > (settings->size - NX_SETTINGS_HEADER_SIZE) / ENTRY_HEADER_SIZE)
        return NX_STORAGE_CORRUPT;
    size_t offset = NX_SETTINGS_HEADER_SIZE;
    uint32_t previous = 0;
    for (uint32_t entry = 0; entry < count; ++entry) {
        if (settings->size - offset < ENTRY_HEADER_SIZE)
            return NX_STORAGE_CORRUPT;
        uint32_t key = get32(image + offset);
        size_t size = get32(image + offset + 4);
        if ((entry != 0 && key <= previous) ||
            size > settings->size - offset - ENTRY_HEADER_SIZE)
            return NX_STORAGE_CORRUPT;
        previous = key;
        offset += ENTRY_HEADER_SIZE + size;
    }
    if (offset != settings->size)
        return NX_STORAGE_CORRUPT;
    if (count > settings->max_entries)
        return NX_STORAGE_NO_SPACE;
    settings->count = count;
    return NX_STORAGE_OK;
}

/** \brief Locate a key or its sorted insertion point in the valid image. */
static size_t locate(const nx_settings_t* settings, uint32_t key, bool* found) {
    size_t offset = NX_SETTINGS_HEADER_SIZE;
    while (offset < settings->size) {
        uint32_t current = get32(settings->image + offset);
        if (current >= key) {
            *found = current == key;
            return offset;
        }
        offset += ENTRY_HEADER_SIZE + get32(settings->image + offset + 4);
    }
    *found = false;
    return offset;
}

/** \brief Load only a valid exact-schema image, or stage an empty image. */
nx_storage_status_t nx_settings_open(nx_settings_t* settings,
                                     nx_storage_t* storage, uint32_t schema,
                                     void* image, size_t image_bytes,
                                     uint32_t max_entries) {
    if (!settings || !storage || !storage->opened || !image ||
        image_bytes < NX_SETTINGS_HEADER_SIZE || image_bytes > UINT32_MAX ||
        max_entries == 0 ||
        overlaps(settings, sizeof(*settings), storage, sizeof(*storage)) ||
        overlaps(image, image_bytes, settings, sizeof(*settings)) ||
        overlaps(image, image_bytes, storage, sizeof(*storage)) ||
        overlaps(image, image_bytes, storage->workspace,
                 storage->workspace_bytes) ||
        overlaps(settings, sizeof(*settings), storage->workspace,
                 storage->workspace_bytes))
        return NX_STORAGE_INVALID;
    memset(settings, 0, sizeof(*settings));
    nx_settings_t staged = {0};
    staged.storage = storage;
    staged.image = image;
    staged.capacity = nx_storage_capacity(storage);
    if (staged.capacity > image_bytes)
        staged.capacity = image_bytes;
    if (staged.capacity < NX_SETTINGS_HEADER_SIZE)
        return NX_STORAGE_NO_SPACE;
    staged.schema = schema;
    staged.max_entries = max_entries;
    staged.size = staged.capacity;
    nx_storage_status_t status =
        nx_storage_load(storage, staged.image, &staged.size);
    if (status == NX_STORAGE_NOT_FOUND) {
        staged.size = NX_SETTINGS_HEADER_SIZE;
        memcpy(staged.image, s_settings_magic, sizeof(s_settings_magic));
        put32(staged.image + 4, 1);
        put32(staged.image + 8, schema);
        put32(staged.image + 12, 0);
        staged.dirty = true;
    } else if (status == NX_STORAGE_OK) {
        status = validate(&staged);
        if (status != NX_STORAGE_OK)
            return status;
    } else {
        return status;
    }
    staged.opened = true;
    *settings = staged;
    return NX_STORAGE_OK;
}

/** \brief Copy a value without retaining output or exposing the staging image.
 */
nx_storage_status_t nx_settings_get(const nx_settings_t* settings, uint32_t key,
                                    void* output, size_t* size) {
    if (!usable(settings) || !size ||
        protected_range(settings, size, sizeof(*size)))
        return NX_STORAGE_INVALID;
    if (output && (protected_range(settings, output, *size) ||
                   overlaps(output, *size, size, sizeof(*size))))
        return NX_STORAGE_INVALID;
    bool found;
    size_t offset = locate(settings, key, &found);
    if (!found) {
        *size = 0;
        return NX_STORAGE_NOT_FOUND;
    }
    size_t bytes = get32(settings->image + offset + 4);
    if (output && *size < bytes) {
        *size = bytes;
        return NX_STORAGE_NO_SPACE;
    }
    if (output && bytes != 0)
        memcpy(output, settings->image + offset + ENTRY_HEADER_SIZE, bytes);
    *size = bytes;
    return NX_STORAGE_OK;
}

/** \brief Admit all bounds before compacting or growing a staged entry. */
nx_storage_status_t nx_settings_set(nx_settings_t* settings, uint32_t key,
                                    const void* value, size_t size) {
    if (!usable(settings) || (!value && size != 0))
        return NX_STORAGE_INVALID;
    if (size > UINT32_MAX)
        return NX_STORAGE_NO_SPACE;
    if (overlaps(value, size, settings, sizeof(*settings)) ||
        overlaps(value, size, settings->image, settings->capacity))
        return NX_STORAGE_INVALID;
    bool found;
    size_t offset = locate(settings, key, &found);
    size_t previous =
        found ? ENTRY_HEADER_SIZE + get32(settings->image + offset + 4) : 0;
    if (found && previous - ENTRY_HEADER_SIZE == size &&
        (size == 0 || memcmp(settings->image + offset + ENTRY_HEADER_SIZE,
                             value, size) == 0))
        return NX_STORAGE_OK;
    if (!found && settings->count == settings->max_entries)
        return NX_STORAGE_NO_SPACE;
    size_t available = settings->capacity - (settings->size - previous);
    if (available < ENTRY_HEADER_SIZE || size > available - ENTRY_HEADER_SIZE)
        return NX_STORAGE_NO_SPACE;
    size_t next = ENTRY_HEADER_SIZE + size;
    memmove(settings->image + offset + next,
            settings->image + offset + previous,
            settings->size - offset - previous);
    put32(settings->image + offset, key);
    put32(settings->image + offset + 4, (uint32_t)size);
    if (size != 0)
        memcpy(settings->image + offset + ENTRY_HEADER_SIZE, value, size);
    settings->size = settings->size - previous + next;
    if (!found)
        settings->count++;
    put32(settings->image + 12, settings->count);
    settings->dirty = true;
    return NX_STORAGE_OK;
}

/** \brief Compact a found entry and preserve the sorted staging invariant. */
nx_storage_status_t nx_settings_remove(nx_settings_t* settings, uint32_t key) {
    if (!usable(settings))
        return NX_STORAGE_INVALID;
    bool found;
    size_t offset = locate(settings, key, &found);
    if (!found)
        return NX_STORAGE_NOT_FOUND;
    size_t bytes = ENTRY_HEADER_SIZE + get32(settings->image + offset + 4);
    memmove(settings->image + offset, settings->image + offset + bytes,
            settings->size - offset - bytes);
    settings->size -= bytes;
    settings->count--;
    put32(settings->image + 12, settings->count);
    settings->dirty = true;
    return NX_STORAGE_OK;
}

/** \brief Let Storage establish the durable commit and surviving generation. */
nx_storage_status_t nx_settings_commit(nx_settings_t* settings) {
    if (!usable(settings))
        return NX_STORAGE_INVALID;
    if (!settings->dirty)
        return NX_STORAGE_OK;
    nx_storage_status_t status =
        nx_storage_save(settings->storage, settings->image, settings->size);
    if (status == NX_STORAGE_OK) {
        settings->dirty = false;
    } else if (!settings->storage->opened) {
        settings->opened = false;
        settings->storage = NULL;
        settings->image = NULL;
    }
    return status;
}

/** \brief Drop only metadata; uncommitted staging is never saved implicitly. */
void nx_settings_close(nx_settings_t* settings) {
    if (settings)
        memset(settings, 0, sizeof(*settings));
}
