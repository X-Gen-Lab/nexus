/**
 * \file            storage.h
 * \brief           Two-bank durable blob storage over an explicit narrow port
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STORAGE_H
#define NEXUS_COMPONENTS_STORAGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Storage errors expose retry/reopen/recovery decisions. */
typedef enum {
    NX_STORAGE_OK = 0,
    NX_STORAGE_INVALID,
    NX_STORAGE_IO,
    NX_STORAGE_CORRUPT,
    NX_STORAGE_NO_SPACE,
    NX_STORAGE_NOT_FOUND,
    NX_STORAGE_UNSUPPORTED
} nx_storage_status_t;

/**
 * \brief           Synchronous settled persistent Flash port
 * \note            Task-only; caller serializes every operation. Offsets are
 *                  byte-relative to the supplied region. program changes only 1
 *                  to 0 in complete aligned program units; erase uses complete
 *                  uniform erase units. sync makes earlier successful writes
 *                  durable. Errors can change a subset of addressed units, but
 *                  never retain caller buffers after returning. RAM is a model,
 *                  not a persistent implementation. Product owns the
 *                  region/layout and power policy.
 */
typedef struct {
    void* ctx;
    size_t size;
    size_t erase_size;
    size_t program_size;
    nx_storage_status_t (*read)(void*, size_t, void*, size_t);
    nx_storage_status_t (*program)(void*, size_t, const void*, size_t);
    nx_storage_status_t (*erase)(void*, size_t, size_t);
    nx_storage_status_t (*sync)(void*);
} nx_storage_port_t;

#define NX_STORAGE_HEADER_SIZE 32u

/**
 * \brief           Instance metadata with caller-sized workspace
 * \note            Two independently erasable banks commit whole blobs. No
 *                  heap, global registry, maximum pool or default product
 *                  reservation. Wire format v1 is explicit little-endian and
 *                  CRC-protected, not authenticated. Keep instance, port
 *                  context and workspace alive through close/quiescence. A
 *                  mutating I/O error invalidates opened; reopen to establish
 *                  which commit actually survived.
 */
typedef struct {
    nx_storage_port_t flash;
    size_t offset;
    size_t bank_size;
    size_t payload_offset;
    uint64_t generation;
    size_t payload_size;
    int active_bank;
    bool opened;
    uint8_t* workspace;
    size_t workspace_bytes;
} nx_storage_t;

/**
 * \brief           Open/recover two externally supplied uniform erase banks
 * \param[out]      storage: Unused or quiesced instance, no concurrent calls
 * \param[in]       port: Persistent settled port, may be storage's existing
 *                  port
 * \param[in]       offset: Erase-aligned start within port region
 * \param[in]       bank_size: Each bank's erase-aligned byte size
 * \param[in]       workspace: Caller buffer, disjoint from instance/payload
 * \param[in]       workspace_bytes: At least max(32, port.program_size)
 * \return          OK for recovered/empty store, CORRUPT for ambiguous or only
 *                  invalid committed generations, IO/INVALID for port/geometry
 *                  failures
 * \note            Task-only bounded scan over both bank payloads. Workspace
 *                  remains borrowed by the instance through every subsequent
 *                  load/save call.
 */
nx_storage_status_t nx_storage_open(nx_storage_t* storage,
                                    const nx_storage_port_t* port,
                                    size_t offset, size_t bank_size,
                                    void* workspace, size_t workspace_bytes);
/**
 * \brief           Load and validate the latest committed whole blob
 * \param[in,out]   storage: Open instance, exclusively owned for this call
 * \param[out]      output: Destination; NULL requests only required size
 * \param[in,out]   size: Input capacity, output payload/required bytes
 * \return          OK, NOT_FOUND, NO_SPACE, CORRUPT or IO/INVALID
 * \note            Output bytes valid only on OK; failure may partially write
 *                  output. No destination pointer is retained after returning.
 */
nx_storage_status_t nx_storage_load(nx_storage_t* storage, void* output,
                                    size_t* size);
/**
 * \brief           Commit a complete blob to the inactive bank
 * \param[in,out]   storage: Open exclusively owned instance
 * \param[in]       data: Payload disjoint from workspace, borrowed during call
 * \param[in]       size: Payload bytes; zero permits NULL
 * \return          OK, NO_SPACE, INVALID or underlying port error
 * \note            Marker is programmed last after sync. Failed mutation keeps
 *                  the previous bank and requires reopen; ambiguous marker
 *                  failure can mean the new blob committed. Product owns
 *                  migration/authentication/recovery policy.
 */
nx_storage_status_t nx_storage_save(nx_storage_t* storage, const void* data,
                                    size_t size);
/**
 * \brief           Query usable blob capacity without touching hardware
 * \param[in]       storage: Geometry-established instance, or NULL
 * \return          Payload bytes per bank, or zero for invalid geometry
 */
size_t nx_storage_capacity(const nx_storage_t* storage);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_STORAGE_H */
