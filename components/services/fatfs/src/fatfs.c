/**
 * \file            fatfs.c
 * \brief           Real FatFs with exact Block progress and explicit scopes
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fatfs.h"
#include <diskio.h>
#include <ff.h>
#include <limits.h>
#include <string.h>

/** \brief Alignment-safe sector staging and all vendor state are caller-owned.
 */
typedef struct {
    FATFS filesystem;
    nx_time_us_t last_time;
    _Alignas(max_align_t) uint8_t sector[512];
} filesystem_state_t;

/** \brief Resolve only the active single-volume operation supplied by product.
 */
static nx_fatfs_t* selected(void) {
    const nx_fatfs_operation_t* operation = nx_fatfs_operation_get();
    return operation != NULL && operation->filesystem != NULL &&
                   operation->filesystem->entered
               ? operation->filesystem
               : NULL;
}

/** \brief Preserve a concrete failure behind FatFs's coarse disk result. */
static DRESULT failed(nx_fatfs_t* filesystem, nx_result_t result,
                      bool uncertain) {
    if (filesystem != NULL) {
        if (filesystem->error == NX_SUCCESS) {
            filesystem->error = result;
        }
        filesystem->faulted = filesystem->faulted || uncertain;
    }
    return RES_ERROR;
}

/** \brief Every sector callback consumes one original finite deadline. */
static nx_result_t budget(nx_fatfs_t* filesystem) {
    const nx_fatfs_operation_t* operation = nx_fatfs_operation_get();
    filesystem_state_t* state = filesystem->state;
    if (operation == NULL || operation->filesystem != filesystem ||
        operation->deadline == NX_DEADLINE_NEVER ||
        filesystem->error != NX_SUCCESS) {
        return filesystem->error != NX_SUCCESS ? filesystem->error
                                               : NX_ERROR_STATE;
    }
    nx_time_us_t now = filesystem->clock.read(filesystem->clock.context);
    if (now == NX_DEADLINE_NEVER || now < state->last_time) {
        return NX_ERROR_STATE;
    }
    state->last_time = now;
    return nx_deadline_expired(operation->deadline, now) ? NX_ERROR_TIMEOUT
                                                         : NX_SUCCESS;
}

/** \brief Disk initialization never starts or reprobes the existing Block
 * owner. */
DSTATUS disk_initialize(BYTE drive) {
    return disk_status(drive);
}

/** \brief Vendor availability is derived from the same existing media session.
 */
DSTATUS disk_status(BYTE drive) {
    nx_fatfs_t* filesystem = selected();
    return (
        DSTATUS)(drive == 0U && filesystem != NULL && !filesystem->faulted &&
                         nx_block_port_state(filesystem->port) == NX_BLOCK_READY
                     ? 0U
                     : STA_NOINIT);
}

/** \brief Reject short successful sector prefixes instead of masking data loss.
 */
DRESULT disk_read(BYTE drive, BYTE* output, LBA_t first, UINT count) {
    nx_fatfs_t* filesystem = selected();
    if (drive != 0U || filesystem == NULL || output == NULL || count == 0U ||
        first > filesystem->block_count ||
        count > filesystem->block_count - first) {
        return RES_PARERR;
    }
    filesystem_state_t* state = filesystem->state;
    for (UINT index = 0U; index < count; ++index) {
        nx_result_t ready = budget(filesystem);
        if (ready != NX_SUCCESS) {
            return failed(filesystem, ready, false);
        }
        const nx_fatfs_operation_t* operation = nx_fatfs_operation_get();
        nx_block_io_result_t result = nx_block_port_read(
            filesystem->port, filesystem->first_block + first + index,
            state->sector, sizeof(state->sector), 1U, operation->deadline);
        if (result.result != NX_SUCCESS || result.completed_blocks != 1U) {
            return failed(filesystem,
                          result.result != NX_SUCCESS ? result.result
                                                      : NX_ERROR_IO,
                          true);
        }
        memcpy(output + (size_t)index * 512U, state->sector, 512U);
    }
    return RES_OK;
}

/** \brief Preserve product sector bounds and wait for actual media readiness.
 */
DRESULT disk_write(BYTE drive, const BYTE* input, LBA_t first, UINT count) {
    nx_fatfs_t* filesystem = selected();
    if (drive != 0U || filesystem == NULL || input == NULL || count == 0U ||
        first > filesystem->block_count ||
        count > filesystem->block_count - first) {
        return RES_PARERR;
    }
    filesystem_state_t* state = filesystem->state;
    for (UINT index = 0U; index < count; ++index) {
        nx_result_t ready = budget(filesystem);
        if (ready != NX_SUCCESS) {
            return failed(filesystem, ready, false);
        }
        memcpy(state->sector, input + (size_t)index * 512U, 512U);
        const nx_fatfs_operation_t* operation = nx_fatfs_operation_get();
        nx_block_io_result_t result = nx_block_port_write(
            filesystem->port, filesystem->first_block + first + index,
            state->sector, sizeof(state->sector), 1U, operation->deadline);
        if (result.result != NX_SUCCESS || result.completed_blocks != 1U) {
            return failed(filesystem,
                          result.result != NX_SUCCESS ? result.result
                                                      : NX_ERROR_IO,
                          true);
        }
    }
    return RES_OK;
}

/** \brief Only the selected Block session can establish sync readiness. */
DRESULT disk_ioctl(BYTE drive, BYTE command, void* output) {
    nx_fatfs_t* filesystem = selected();
    if (drive != 0U || filesystem == NULL) {
        return RES_PARERR;
    }
    nx_result_t ready = budget(filesystem);
    if (ready != NX_SUCCESS) {
        return failed(filesystem, ready, false);
    }
    if (command == CTRL_SYNC) {
        const nx_fatfs_operation_t* operation = nx_fatfs_operation_get();
        nx_result_t result =
            nx_block_port_sync(filesystem->port, operation->deadline);
        return result == NX_SUCCESS ? RES_OK : failed(filesystem, result, true);
    }
    if (output == NULL) {
        return RES_PARERR;
    }
    if (command == GET_SECTOR_COUNT) {
        *(LBA_t*)output = (LBA_t)filesystem->block_count;
    } else if (command == GET_SECTOR_SIZE) {
        *(WORD*)output = 512U;
    } else if (command == GET_BLOCK_SIZE) {
        *(DWORD*)output = 1U;
    } else {
        return RES_PARERR;
    }
    return RES_OK;
}

/** \brief Report exact state, bounce and formatting-buffer requirements. */
size_t nx_fatfs_workspace_bytes(void) {
    return sizeof(filesystem_state_t);
}

/** \brief The same alignment satisfies metadata and the admitted sector buffer.
 */
size_t nx_fatfs_workspace_alignment(void) {
    return _Alignof(filesystem_state_t);
}

/** \brief No Block activate, allocation, mount or formatting happens here. */
nx_result_t nx_fatfs_init(nx_fatfs_t* filesystem, const nx_block_port_t* port,
                          nx_clock_t clock, uint64_t first_block,
                          uint64_t block_count, void* workspace, size_t bytes) {
    if (filesystem == NULL || port == NULL || clock.read == NULL ||
        workspace == NULL || bytes < sizeof(filesystem_state_t) ||
        (uintptr_t)workspace % _Alignof(filesystem_state_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    const nx_block_geometry_t* geometry = nx_block_port_geometry(port);
    if (geometry == NULL || nx_block_port_state(port) != NX_BLOCK_READY) {
        return NX_ERROR_STATE;
    }
    if (!geometry->writable || geometry->block_bytes != 512U ||
        geometry->buffer_alignment == 0U ||
        _Alignof(filesystem_state_t) % geometry->buffer_alignment != 0U ||
        block_count == 0U || block_count > UINT32_MAX ||
        first_block > geometry->block_count ||
        block_count > geometry->block_count - first_block) {
        return NX_ERROR_UNSUPPORTED;
    }
    memset(workspace, 0, sizeof(filesystem_state_t));
    nx_fatfs_t initialized = {port,        clock,     first_block,
                              block_count, workspace, NX_SUCCESS,
                              false,       false,     false};
    *filesystem = initialized;
    return NX_SUCCESS;
}

/** \brief A product operation scope is mandatory; no mutable device selector.
 */
static nx_result_t begin(const nx_fatfs_operation_t* operation,
                         bool require_mounted) {
    if (operation == NULL || nx_fatfs_operation_get() != operation ||
        operation->filesystem == NULL || operation->filesystem->state == NULL) {
        return NX_ERROR_STATE;
    }
    nx_fatfs_t* filesystem = operation->filesystem;
    if (filesystem->entered) {
        return NX_ERROR_BUSY;
    }
    if (filesystem->faulted || (require_mounted && !filesystem->mounted)) {
        return NX_ERROR_STATE;
    }
    if (operation->deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    nx_time_us_t now = filesystem->clock.read(filesystem->clock.context);
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(operation->deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    filesystem_state_t* state = filesystem->state;
    state->last_time = now;
    filesystem->error = NX_SUCCESS;
    filesystem->entered = true;
    return NX_SUCCESS;
}

/** \brief Keep physical uncertainty independent of logical volume withdrawal.
 */
static nx_result_t finish(nx_fatfs_t* filesystem, FRESULT upstream) {
    if (filesystem->error == NX_SUCCESS && upstream == FR_OK) {
        filesystem->error = budget(filesystem);
    }
    filesystem->entered = false;
    if (filesystem->error != NX_SUCCESS) {
        return filesystem->error;
    }
    if (upstream == FR_OK) {
        return NX_SUCCESS;
    }
    if (upstream == FR_NO_FILE || upstream == FR_NO_PATH) {
        return NX_ERROR_EMPTY;
    }
    if (upstream == FR_NOT_ENOUGH_CORE || upstream == FR_DENIED) {
        return NX_ERROR_EXHAUSTED;
    }
    return upstream == FR_INVALID_NAME ? NX_ERROR_INVALID : NX_ERROR_IO;
}

/** \brief Format is a separate destructive decision for an unmounted volume. */
nx_result_t nx_fatfs_format(const nx_fatfs_operation_t* operation) {
    nx_result_t result = begin(operation, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_fatfs_t* filesystem = operation->filesystem;
    if (filesystem->mounted) {
        return finish(filesystem, FR_INVALID_PARAMETER);
    }
    filesystem_state_t* state = filesystem->state;
    MKFS_PARM parameters = {FM_FAT | FM_SFD, 1U, 0U, 128U, 0U};
    return finish(filesystem, f_mkfs("0:", &parameters, state->sector,
                                     sizeof(state->sector)));
}

/** \brief Unknown or damaged media never trigger formatting as a fallback. */
nx_result_t nx_fatfs_mount(const nx_fatfs_operation_t* operation) {
    nx_result_t result = begin(operation, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_fatfs_t* filesystem = operation->filesystem;
    if (filesystem->mounted) {
        return finish(filesystem, FR_INVALID_PARAMETER);
    }
    filesystem_state_t* state = filesystem->state;
    result = finish(filesystem, f_mount(&state->filesystem, "0:", 1U));
    filesystem->mounted = result == NX_SUCCESS;
    return result;
}

/** \brief Detach vendor volume references without media I/O or owner release.
 */
nx_result_t nx_fatfs_unmount(const nx_fatfs_operation_t* operation) {
    if (operation == NULL || nx_fatfs_operation_get() != operation ||
        operation->filesystem == NULL || operation->filesystem->state == NULL) {
        return NX_ERROR_STATE;
    }
    nx_fatfs_t* filesystem = operation->filesystem;
    if (filesystem->entered) {
        return NX_ERROR_BUSY;
    }
    FRESULT upstream = f_mount(NULL, "0:", 0U);
    filesystem->mounted = false;
    return upstream == FR_OK ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Keep paths within the selected volume and bound vendor parsing work.
 */
static bool valid_path(const char* path) {
    if (path == NULL || path[0] == '\0' || path[0] == '/' || path[0] == ':') {
        return false;
    }
    for (size_t index = 1U; index < 64U; ++index) {
        if (path[index] == ':') {
            return false;
        }
        if (path[index] == '\0') {
            return true;
        }
    }
    return false;
}

/** \brief One finite scope covers open, write, sync and handle withdrawal. */
nx_result_t nx_fatfs_put(const nx_fatfs_operation_t* operation,
                         const char* path, const void* data, size_t bytes) {
    if (!valid_path(path) || bytes > UINT_MAX ||
        (data == NULL && bytes != 0U)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_fatfs_t* filesystem = operation->filesystem;
    FIL file;
    FRESULT upstream = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (upstream == FR_OK) {
        UINT written = 0U;
        upstream = f_write(&file, data, (UINT)bytes, &written);
        if (upstream == FR_OK && written != bytes) {
            upstream = FR_DENIED;
        }
        if (upstream == FR_OK) {
            upstream = f_sync(&file);
        }
        FRESULT closed = f_close(&file);
        if (upstream == FR_OK) {
            upstream = closed;
        }
    }
    return finish(filesystem, upstream);
}

/** \brief Never accept a truncated successful file read. */
nx_result_t nx_fatfs_get(const nx_fatfs_operation_t* operation,
                         const char* path, void* data, size_t* bytes) {
    if (!valid_path(path) || bytes == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_fatfs_t* filesystem = operation->filesystem;
    FIL file;
    FRESULT upstream = f_open(&file, path, FA_READ);
    bool full = false;
    if (upstream == FR_OK) {
        size_t length = f_size(&file);
        size_t capacity = *bytes;
        *bytes = length;
        if (data != NULL && capacity < length) {
            full = true;
        } else if (data != NULL && length != 0U) {
            UINT read = 0U;
            upstream = f_read(&file, data, (UINT)length, &read);
            if (upstream == FR_OK && read != length) {
                upstream = FR_DISK_ERR;
            }
        }
        FRESULT closed = f_close(&file);
        if (upstream == FR_OK) {
            upstream = closed;
        }
    }
    result = finish(filesystem, upstream);
    return result == NX_SUCCESS && full ? NX_ERROR_EXHAUSTED : result;
}

/** \brief A failed close keeps volume admission closed until explicit exit. */
static FRESULT close_chunk(nx_fatfs_t* filesystem, FIL* file,
                           FRESULT upstream) {
    FRESULT closed = f_close(file);
    if (closed != FR_OK) {
        filesystem->faulted = true;
    }
    return upstream == FR_OK ? closed : upstream;
}

/** \brief Read only the requested file window without a persistent handle. */
nx_result_t nx_fatfs_read_at(const nx_fatfs_operation_t* operation,
                             const char* path, uint32_t offset, void* data,
                             size_t* bytes) {
    if (!valid_path(path) || data == NULL || bytes == NULL || *bytes == 0U ||
        *bytes > UINT_MAX) {
        return NX_ERROR_INVALID;
    }
    UINT capacity = (UINT)*bytes;
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    *bytes = 0U;
    nx_fatfs_t* filesystem = operation->filesystem;
    FIL file;
    FRESULT upstream = f_open(&file, path, FA_READ);
    bool beyond = false;
    if (upstream == FR_OK) {
        beyond = offset > f_size(&file);
        if (!beyond) {
            upstream = f_lseek(&file, (FSIZE_t)offset);
            if (upstream == FR_OK) {
                UINT read = 0U;
                upstream = f_read(&file, data, capacity, &read);
                *bytes = read;
            }
        }
        upstream = close_chunk(filesystem, &file, upstream);
    }
    result = finish(filesystem, upstream);
    return result == NX_SUCCESS && beyond ? NX_ERROR_INVALID : result;
}

/** \brief Report write progress even when subsequent sync cannot complete. */
nx_result_t nx_fatfs_append(const nx_fatfs_operation_t* operation,
                            const char* path, const void* data, size_t* bytes) {
    if (!valid_path(path) || bytes == NULL || *bytes > UINT_MAX ||
        (data == NULL && *bytes != 0U)) {
        return NX_ERROR_INVALID;
    }
    UINT requested = (UINT)*bytes;
    nx_result_t result = begin(operation, true);
    if (result != NX_SUCCESS) {
        return result;
    }
    *bytes = 0U;
    nx_fatfs_t* filesystem = operation->filesystem;
    FIL file;
    FRESULT upstream = f_open(&file, path, FA_WRITE | FA_OPEN_APPEND);
    if (upstream == FR_OK) {
        if (requested > UINT32_MAX - f_size(&file)) {
            upstream = FR_DENIED;
        } else if (requested != 0U) {
            UINT written = 0U;
            upstream = f_write(&file, data, requested, &written);
            *bytes = written;
            if (upstream == FR_OK && written != requested) {
                upstream = FR_DENIED;
            }
        }
        if (upstream == FR_OK) {
            upstream = f_sync(&file);
        }
        upstream = close_chunk(filesystem, &file, upstream);
    }
    return finish(filesystem, upstream);
}
