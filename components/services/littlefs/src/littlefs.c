/**
 * \file            littlefs.c
 * \brief           Real LittleFS using explicit Flash transactions and caches
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/littlefs.h"
#include <lfs.h>
#include <limits.h>
#include <string.h>

/** \brief Every upstream cache and state byte belongs to the caller workspace.
 */
typedef struct {
    lfs_t filesystem;
    struct lfs_config config;
    uint8_t read_cache[256];
    uint8_t program_cache[256];
    uint8_t file_cache[256];
    uint8_t lookahead[32];
} filesystem_state_t;

/** \brief Translate a logical block only after proving its complete bounds. */
static bool address(const struct lfs_config* config, lfs_block_t block,
                    lfs_off_t offset, lfs_size_t bytes, uint32_t* result) {
    uint64_t start = (uint64_t)block * config->block_size + offset;
    uint64_t end = (uint64_t)config->block_count * config->block_size;
    if (block >= config->block_count || offset > config->block_size ||
        bytes > config->block_size - offset || start > UINT32_MAX ||
        bytes > end - start) {
        return false;
    }
    *result = (uint32_t)start;
    return true;
}

/** \brief Read callbacks consume the existing transaction budget. */
static int read_block(const struct lfs_config* config, lfs_block_t block,
                      lfs_off_t offset, void* data, lfs_size_t bytes) {
    nx_littlefs_t* instance = config->context;
    uint32_t start;
    return address(config, block, offset, bytes, &start) &&
                   nx_fal_read(instance->flash, start, data, bytes) ==
                       (int)bytes
               ? 0
               : LFS_ERR_IO;
}

/** \brief Write callbacks never retain a file buffer beyond their return. */
static int program_block(const struct lfs_config* config, lfs_block_t block,
                         lfs_off_t offset, const void* data, lfs_size_t bytes) {
    nx_littlefs_t* instance = config->context;
    uint32_t start;
    return address(config, block, offset, bytes, &start) &&
                   nx_fal_program(instance->flash, start, data, bytes) ==
                       (int)bytes
               ? 0
               : LFS_ERR_IO;
}

/** \brief Each logical erase names exactly one admitted physical sector. */
static int erase_block(const struct lfs_config* config, lfs_block_t block) {
    nx_littlefs_t* instance = config->context;
    uint32_t start;
    return address(config, block, 0U, config->block_size, &start) &&
                   nx_fal_erase(instance->flash, start, config->block_size) ==
                       (int)config->block_size
               ? 0
               : LFS_ERR_IO;
}

/** \brief Physical Flash callbacks already settle their final hardware pulse.
 */
static int sync_blocks(const struct lfs_config* config) {
    nx_littlefs_t* instance = config->context;
    nx_fal_t* flash = instance->flash;
    nx_time_us_t now = flash->clock.read(flash->clock.context);
    if (flash->error == NX_SUCCESS) {
        if (now == NX_DEADLINE_NEVER || now < flash->last_time) {
            flash->error = NX_ERROR_STATE;
        } else if (nx_deadline_expired(flash->deadline, now)) {
            flash->error = NX_ERROR_TIMEOUT;
        }
    }
    return flash->error == NX_SUCCESS ? 0 : LFS_ERR_IO;
}

/** \brief Report the actual fixed-profile vendor state and caches. */
size_t nx_littlefs_workspace_bytes(void) {
    return sizeof(filesystem_state_t);
}

/** \brief Public callers do not need any vendor header or structure layout. */
size_t nx_littlefs_workspace_alignment(void) {
    return _Alignof(filesystem_state_t);
}

/** \brief Initialize static state without reading, formatting or mounting. */
nx_result_t nx_littlefs_init(nx_littlefs_t* filesystem, nx_fal_t* flash,
                             void* workspace, size_t bytes) {
    if (filesystem == NULL || flash == NULL || workspace == NULL ||
        bytes < sizeof(filesystem_state_t) ||
        (uintptr_t)workspace % _Alignof(filesystem_state_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (flash->region.port == NULL || flash->program_bytes == 0U ||
        flash->erase_bytes < 256U || flash->erase_bytes % 256U != 0U ||
        256U % flash->program_bytes != 0U ||
        flash->region.size / flash->erase_bytes < 2U) {
        return NX_ERROR_UNSUPPORTED;
    }
    filesystem_state_t* state = workspace;
    memset(state, 0, sizeof(*state));
    nx_littlefs_t initialized = {flash, workspace, false, false};
    *filesystem = initialized;
    state->config.context = filesystem;
    state->config.read = read_block;
    state->config.prog = program_block;
    state->config.erase = erase_block;
    state->config.sync = sync_blocks;
    state->config.read_size = 1U;
    state->config.prog_size = flash->program_bytes;
    state->config.block_size = flash->erase_bytes;
    state->config.block_count = flash->region.size / flash->erase_bytes;
    state->config.block_cycles = 500;
    state->config.cache_size = sizeof(state->read_cache);
    state->config.lookahead_size = sizeof(state->lookahead);
    state->config.read_buffer = state->read_cache;
    state->config.prog_buffer = state->program_cache;
    state->config.lookahead_buffer = state->lookahead;
    state->config.name_max = 127U;
    return NX_SUCCESS;
}

/** \brief Enter the existing physical owner, without acquiring a hidden lock.
 */
static nx_result_t begin(nx_littlefs_t* instance, nx_time_us_t deadline,
                         bool require_mounted) {
    if (instance == NULL || instance->state == NULL ||
        instance->flash == NULL) {
        return NX_ERROR_INVALID;
    }
    if (instance->entered) {
        return NX_ERROR_BUSY;
    }
    if (require_mounted && !instance->mounted) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_fal_begin(instance->flash, deadline);
    if (result == NX_SUCCESS) {
        instance->entered = true;
    }
    return result;
}

/** \brief Preserve the first physical error even after an upstream close. */
static nx_result_t finish(nx_littlefs_t* instance, int result) {
    nx_result_t io = nx_fal_end(instance->flash);
    instance->entered = false;
    if (io != NX_SUCCESS) {
        if (instance->mounted) {
            filesystem_state_t* state = instance->state;
            (void)lfs_unmount(&state->filesystem);
        }
        instance->mounted = false;
        return io;
    }
    if (result >= 0) {
        return NX_SUCCESS;
    }
    if (result == LFS_ERR_NOENT) {
        return NX_ERROR_EMPTY;
    }
    if (result == LFS_ERR_NOSPC || result == LFS_ERR_NOMEM) {
        return NX_ERROR_EXHAUSTED;
    }
    return result == LFS_ERR_INVAL ? NX_ERROR_INVALID : NX_ERROR_IO;
}

/** \brief Product-authorized formatting has no automatic fallback caller. */
nx_result_t nx_littlefs_format(nx_littlefs_t* filesystem,
                               nx_time_us_t deadline) {
    nx_result_t result = begin(filesystem, deadline, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (filesystem->mounted) {
        return finish(filesystem, LFS_ERR_INVAL);
    }
    filesystem_state_t* state = filesystem->state;
    return finish(filesystem, lfs_format(&state->filesystem, &state->config));
}

/** \brief Mount only the existing bytes; corruption never authorizes format. */
nx_result_t nx_littlefs_mount(nx_littlefs_t* filesystem,
                              nx_time_us_t deadline) {
    nx_result_t result = begin(filesystem, deadline, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (filesystem->mounted) {
        return finish(filesystem, LFS_ERR_INVAL);
    }
    filesystem_state_t* state = filesystem->state;
    int mounted = lfs_mount(&state->filesystem, &state->config);
    result = finish(filesystem, mounted);
    filesystem->mounted = result == NX_SUCCESS;
    return result;
}

/** \brief Logical unmount neither releases nor stops the physical provider. */
nx_result_t nx_littlefs_unmount(nx_littlefs_t* filesystem,
                                nx_time_us_t deadline) {
    if (filesystem == NULL || filesystem->state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!filesystem->mounted && !filesystem->entered) {
        return NX_SUCCESS;
    }
    nx_result_t result = begin(filesystem, deadline, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    filesystem_state_t* state = filesystem->state;
    result = finish(filesystem, lfs_unmount(&state->filesystem));
    filesystem->mounted = false;
    return result;
}

/** \brief Do not pass an unbounded product string into the filesystem parser.
 */
static bool valid_path(const char* path) {
    if (path == NULL || path[0] == '\0' || path[0] == '/') {
        return false;
    }
    for (size_t index = 1U; index < 128U; ++index) {
        if (path[index] == '\0') {
            return true;
        }
    }
    return false;
}

/** \brief Close the file while its cache and complete operation scope are live.
 */
nx_result_t nx_littlefs_put(nx_littlefs_t* filesystem, const char* path,
                            const void* data, size_t bytes,
                            nx_time_us_t deadline) {
    if (!valid_path(path) || bytes > INT32_MAX ||
        (data == NULL && bytes != 0U)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(filesystem, deadline, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    filesystem_state_t* state = filesystem->state;
    lfs_file_t file;
    struct lfs_file_config config = {0};
    config.buffer = state->file_cache;
    int upstream =
        lfs_file_opencfg(&state->filesystem, &file, path,
                         LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC, &config);
    if (upstream == 0) {
        lfs_ssize_t written =
            lfs_file_write(&state->filesystem, &file, data, (lfs_size_t)bytes);
        upstream = written == (lfs_ssize_t)bytes
                       ? lfs_file_sync(&state->filesystem, &file)
                       : (written < 0 ? (int)written : LFS_ERR_NOSPC);
        int closed = lfs_file_close(&state->filesystem, &file);
        if (upstream == 0) {
            upstream = closed;
        }
    }
    return finish(filesystem, upstream);
}

/** \brief Required-size validation precedes copying any file contents. */
nx_result_t nx_littlefs_get(nx_littlefs_t* filesystem, const char* path,
                            void* data, size_t* bytes, nx_time_us_t deadline) {
    if (!valid_path(path) || bytes == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(filesystem, deadline, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    filesystem_state_t* state = filesystem->state;
    lfs_file_t file;
    struct lfs_file_config config = {0};
    config.buffer = state->file_cache;
    int upstream = lfs_file_opencfg(&state->filesystem, &file, path,
                                    LFS_O_RDONLY, &config);
    if (upstream == 0) {
        lfs_soff_t length = lfs_file_size(&state->filesystem, &file);
        if (length < 0) {
            upstream = (int)length;
        } else {
            size_t capacity = *bytes;
            *bytes = (size_t)length;
            if (data != NULL && capacity < (size_t)length) {
                upstream = LFS_ERR_NOMEM;
            } else if (data != NULL && length != 0) {
                lfs_ssize_t read = lfs_file_read(&state->filesystem, &file,
                                                 data, (lfs_size_t)length);
                upstream = read == length ? 0 : LFS_ERR_IO;
            }
        }
        int closed = lfs_file_close(&state->filesystem, &file);
        if (upstream == 0) {
            upstream = closed;
        }
    }
    return finish(filesystem, upstream);
}

/** \brief A local file is always withdrawn before its stack storage expires. */
nx_result_t nx_littlefs_read_at(nx_littlefs_t* filesystem, const char* path,
                                uint32_t offset, void* data, size_t* bytes,
                                nx_time_us_t deadline) {
    if (!valid_path(path) || data == NULL || bytes == NULL || *bytes == 0U ||
        *bytes > INT32_MAX || offset > INT32_MAX) {
        return NX_ERROR_INVALID;
    }
    size_t capacity = *bytes;
    nx_result_t result = begin(filesystem, deadline, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    *bytes = 0U;
    filesystem_state_t* state = filesystem->state;
    lfs_file_t file;
    struct lfs_file_config config = {0};
    config.buffer = state->file_cache;
    int upstream = lfs_file_opencfg(&state->filesystem, &file, path,
                                    LFS_O_RDONLY, &config);
    if (upstream == 0) {
        lfs_soff_t length = lfs_file_size(&state->filesystem, &file);
        if (length < 0 || offset > (uint32_t)length) {
            upstream = length < 0 ? (int)length : LFS_ERR_INVAL;
        } else {
            lfs_soff_t position = lfs_file_seek(
                &state->filesystem, &file, (lfs_soff_t)offset, LFS_SEEK_SET);
            if (position < 0) {
                upstream = (int)position;
            } else {
                lfs_ssize_t read = lfs_file_read(&state->filesystem, &file,
                                                 data, (lfs_size_t)capacity);
                if (read < 0) {
                    upstream = (int)read;
                } else {
                    *bytes = (size_t)read;
                }
            }
        }
        int closed = lfs_file_close(&state->filesystem, &file);
        if (upstream == 0) {
            upstream = closed;
        }
    }
    return finish(filesystem, upstream);
}

/** \brief Prefix progress and durable completion are separate observations. */
nx_result_t nx_littlefs_append(nx_littlefs_t* filesystem, const char* path,
                               const void* data, size_t* bytes,
                               nx_time_us_t deadline) {
    if (!valid_path(path) || bytes == NULL || *bytes > INT32_MAX ||
        (data == NULL && *bytes != 0U)) {
        return NX_ERROR_INVALID;
    }
    size_t requested = *bytes;
    nx_result_t result = begin(filesystem, deadline, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    *bytes = 0U;
    filesystem_state_t* state = filesystem->state;
    lfs_file_t file;
    struct lfs_file_config config = {0};
    config.buffer = state->file_cache;
    int upstream =
        lfs_file_opencfg(&state->filesystem, &file, path,
                         LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND, &config);
    if (upstream == 0) {
        lfs_soff_t length = lfs_file_size(&state->filesystem, &file);
        if (length < 0 || requested > (size_t)INT32_MAX - (size_t)length) {
            upstream = length < 0 ? (int)length : LFS_ERR_FBIG;
        } else if (requested != 0U) {
            lfs_ssize_t written = lfs_file_write(&state->filesystem, &file,
                                                 data, (lfs_size_t)requested);
            if (written < 0) {
                upstream = (int)written;
            } else {
                *bytes = (size_t)written;
                if (*bytes != requested) {
                    upstream = LFS_ERR_NOSPC;
                }
            }
        }
        if (upstream == 0) {
            upstream = lfs_file_sync(&state->filesystem, &file);
        }
        int closed = lfs_file_close(&state->filesystem, &file);
        if (upstream == 0) {
            upstream = closed;
        }
    }
    return finish(filesystem, upstream);
}
