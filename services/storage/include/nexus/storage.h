#ifndef NEXUS_STORAGE_H
#define NEXUS_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_STORAGE_OK = 0,
    NX_STORAGE_INVALID,
    NX_STORAGE_IO,
    NX_STORAGE_CORRUPT,
    NX_STORAGE_NO_SPACE,
    NX_STORAGE_NOT_FOUND,
    NX_STORAGE_UNSUPPORTED
} nx_storage_status_t;

/* Synchronous task-context port. Every successful program/erase is durable
 * after sync. read/program use byte offsets. program accepts complete aligned
 * program units and changes only 1 to 0. erase accepts complete erase blocks.
 * A failed call may have changed any subset of the addressed units; no caller
 * buffer is retained after return. Product owns serialization and locking. */
typedef struct {
    void* ctx;
    size_t size;
    size_t erase_size;
    size_t program_size;
    nx_storage_status_t (*read)(void*, size_t, void*, size_t);
    nx_storage_status_t (*program)(void*, size_t, const void*, size_t);
    nx_storage_status_t (*erase)(void*, size_t, size_t);
    nx_storage_status_t (*sync)(void*);
} nx_flash_port_t;

#define NX_STORAGE_MAX_PROGRAM_SIZE 256u
#define NX_STORAGE_HEADER_SIZE 32u

/* Two independently erasable banks, supplied by the external application layout. Scratch is
 * caller-owned and may be freed after open; no allocation or retained buffers.
 * Instance and port remain alive through all calls. The whole blob is atomic;
 * saving many keys is one transaction. Every mutating I/O error invalidates
 * the instance: reopen before deciding which generation committed, particularly
 * after marker writes where success is ambiguous. Keep referenced keys. */
typedef struct {
    nx_flash_port_t flash;
    size_t offset;
    size_t bank_size;
    size_t payload_offset;
    uint64_t generation;
    size_t payload_size;
    int active_bank;
    bool opened;
} nx_storage_t;

nx_storage_status_t nx_storage_open(nx_storage_t*, const nx_flash_port_t*,
                                    size_t offset, size_t bank_size);
nx_storage_status_t nx_storage_load(nx_storage_t*, void*, size_t*);
nx_storage_status_t nx_storage_save(nx_storage_t*, const void*, size_t);
size_t nx_storage_capacity(const nx_storage_t*);

#ifdef __cplusplus
}
#endif
#endif
