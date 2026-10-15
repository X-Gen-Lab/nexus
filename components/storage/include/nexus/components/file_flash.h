/**
 * \file            file_flash.h
 * \brief           Native persistent file Flash model with fault injection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_FILE_FLASH_H
#define NEXUS_FILE_FLASH_H
#include "nexus/components/storage.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native POSIX flash model, not a hardware driver. Exclusive file ownership,
 * exact persistent geometry, 1->0 programming and caller-controlled failures.
 * fail_after counts programmed/erased bytes and sync attempts, including bytes
 * whose value remains unchanged. Negative disables it.
 * A zero budget simulates a powered-off device until explicitly cleared. */
typedef struct {
    int fd;
    nx_storage_port_t port;
    int64_t fail_after;
    uint64_t mutation_events;
} nx_file_flash_t;
/**
 * \brief           Open an exclusively locked persistent host Flash model
 * \param[out]      flash: Unused caller-owned storage
 * \param[in]       path: Regular-file path; symlink traversal rejected
 * \param[in]       size: Exact persistent byte size
 * \param[in]       erase_size: Uniform erase block bytes
 * \param[in]       program_size: Aligned program bytes, 1..256
 * \return          OK or INVALID/IO; failed open retains no file descriptor
 * \note            Task-only POSIX model. Does not qualify MCU Flash or actual
 *                  power loss; existing file size mismatch is rejected without
 *                  truncation.
 */
nx_storage_status_t nx_file_flash_open(nx_file_flash_t* flash, const char* path,
                                       size_t size, size_t erase_size,
                                       size_t program_size);
/**
 * \brief           Close the host file after all store users have quiesced
 * \param[in,out]   flash: Open serialized model
 * \note            No future port calls are permitted until explicit reopen.
 */
void nx_file_flash_close(nx_file_flash_t* flash);
/**
 * \brief           Set an explicit model interruption boundary
 * \param[in,out]   flash: Serialized open model
 * \param[in]       events: Programmed/erased bytes and sync attempts permitted;
 *                  negative disables
 * \note            Zero simulates power-off until explicitly cleared.
 */
void nx_file_flash_fail_after(nx_file_flash_t* flash, int64_t events);
#ifdef __cplusplus
}
#endif
#endif
