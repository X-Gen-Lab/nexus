/**
 * \file            settings.h
 * \brief           Bounded key/value staging over durable whole-blob storage
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SETTINGS_H
#define NEXUS_COMPONENTS_SETTINGS_H

#include "nexus/components/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_SETTINGS_HEADER_SIZE 16u

/**
 * \brief           Caller-owned settings metadata and bounded staging image
 * \note            Task-only; serialize all operations and the underlying
 *                  storage. No heap, workers or global key registry. Keys are
 *                  product-assigned uint32 identifiers; values are byte
 * strings, including empty strings. The format is little-endian, not
 *                  authenticated. Products own defaults, schema migration,
 *                  authorization, durability frequency and Flash reservations.
 *                  Only commit writes storage; close discards staged changes.
 */
typedef struct {
    nx_storage_t* storage;
    uint8_t* image;
    size_t capacity;
    size_t size;
    uint32_t schema;
    uint32_t count;
    uint32_t max_entries;
    bool opened;
    bool dirty;
} nx_settings_t;

/**
 * \brief           Load one exact schema or stage an empty, uncommitted image
 * \param[out]      settings: Unused or closed instance, exclusively owned
 * \param[in,out]   storage: Open durable store; no other mutating client while
 *                  this settings instance is open
 * \param[in]       schema: Product-defined schema identity, including zero
 * \param[in,out]   image: Caller buffer, disjoint from both instances and the
 *                  storage workspace; borrowed until close or a failed commit
 * \param[in]       image_bytes: At least HEADER_SIZE and at most UINT32_MAX
 * \param[in]       max_entries: Nonzero upper bound on staged entries
 * \return          OK, INVALID, NO_SPACE, CORRUPT, UNSUPPORTED or port error
 * \note            Synchronous task-only scan. Missing blob stages an empty
 *                  image, without reading defaults or writing Flash. Different
 *                  schema/format is UNSUPPORTED and never rewritten. Truncated,
 *                  duplicate or unordered keys are CORRUPT. A failed load never
 *                  becomes an empty writable store; the instance stays closed.
 */
nx_storage_status_t nx_settings_open(nx_settings_t* settings,
                                     nx_storage_t* storage, uint32_t schema,
                                     void* image, size_t image_bytes,
                                     uint32_t max_entries);

/**
 * \brief           Copy a staged value without exposing internal storage
 * \param[in]       settings: Open instance, serialized with all mutation
 * \param[in]       key: Exact product-assigned identifier
 * \param[out]      output: Disjoint destination, or NULL to query size only
 * \param[in,out]   size: Capacity on input, required/value bytes on output;
 *                  disjoint from metadata, staging image and output
 * \return          OK, NOT_FOUND, NO_SPACE or INVALID
 * \note            Task-only synchronous copy; no pointer is retained. Missing
 *                  key reports size zero. NO_SPACE leaves output untouched and
 *                  reports required bytes. No underlying Flash operation
 * occurs.
 */
nx_storage_status_t nx_settings_get(const nx_settings_t* settings, uint32_t key,
                                    void* output, size_t* size);

/**
 * \brief           Insert or replace one copied value in bounded RAM staging
 * \param[in,out]   settings: Open exclusively owned instance
 * \param[in]       key: Exact product-assigned identifier
 * \param[in]       value: Input disjoint from instance and staging image;
 *                  NULL only for an empty value, borrowed during this call
 * \param[in]       size: Value byte count, at most UINT32_MAX
 * \return          OK, NO_SPACE or INVALID
 * \note            Task-only, bounded by image capacity and entry limit. Failed
 *                  admission changes neither entries nor dirty state. A changed
 *                  value marks staging dirty without touching Flash; an
 * identical value preserves both image and the previous dirty state.
 */
nx_storage_status_t nx_settings_set(nx_settings_t* settings, uint32_t key,
                                    const void* value, size_t size);

/**
 * \brief           Remove a staged key without writing Flash
 * \param[in,out]   settings: Open exclusively owned instance
 * \param[in]       key: Exact product-assigned identifier
 * \return          OK, NOT_FOUND or INVALID
 * \note            Task-only synchronous operation. Missing keys have no
 *                  effects. Success compacts staging and marks it dirty.
 */
nx_storage_status_t nx_settings_remove(nx_settings_t* settings, uint32_t key);

/**
 * \brief           Commit the complete staged snapshot through durable Storage
 * \param[in,out]   settings: Open exclusively owned instance
 * \return          OK or underlying storage error
 * \note            Task-only settled call; underlying Flash may stall
 * execution. Clean staging does not write. Successful save clears dirty. A
 * mutating I/O failure closes the instance and preserves the dirty flag for
 * diagnosis, but retains no staging borrow. Reopen Storage, then Settings to
 * determine the surviving commit; failure may occur after a new marker became
 * durable. Retrying a failed commit without recovery is rejected.
 */
nx_storage_status_t nx_settings_commit(nx_settings_t* settings);

/**
 * \brief           Release settings metadata without committing staged values
 * \param[in,out]   settings: Quiesced instance, or NULL
 * \note            Task-only, serialized after all calls. No I/O or retained
 *                  references remain. Underlying Storage stays open; its
 * scratch lifetime remains governed by the Storage contract.
 */
void nx_settings_close(nx_settings_t* settings);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_SETTINGS_H */
