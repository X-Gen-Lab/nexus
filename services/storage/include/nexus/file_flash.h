#ifndef NEXUS_FILE_FLASH_H
#define NEXUS_FILE_FLASH_H
#include "nexus/storage.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native POSIX flash model, not a hardware driver. Exclusive file ownership,
 * exact persistent geometry, 1->0 programming and caller-controlled failures.
 * fail_after counts changed bytes and sync operations. Negative disables it.
 * A zero budget simulates a powered-off device until explicitly cleared. */
typedef struct {
    int fd;
    nx_flash_port_t port;
    int64_t fail_after;
    uint64_t mutation_events;
} nx_file_flash_t;
nx_storage_status_t nx_file_flash_open(nx_file_flash_t*, const char* path,
                                       size_t size, size_t erase_size,
                                       size_t program_size);
void nx_file_flash_close(nx_file_flash_t*);
void nx_file_flash_fail_after(nx_file_flash_t*, int64_t events);
#ifdef __cplusplus
}
#endif
#endif
