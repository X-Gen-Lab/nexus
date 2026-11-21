/**
 * \file            persistent_record.h
 * \brief           Versioned byte records with explicit schema and checksum
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_PERSISTENT_RECORD_H
#define NEXUS_COMPONENTS_PERSISTENT_RECORD_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_PERSISTENT_RECORD_HEADER_BYTES 32U
/** \brief Validated borrowed view; backing image remains caller-owned. */
typedef struct {
    uint64_t identifier;
    const uint8_t* payload;
    size_t payload_bytes;
} nx_persistent_record_view_t;
/**
 * \brief Encode one explicitly identified little-endian record, without I/O.
 * \param[in] schema: Product-defined data schema identity.
 * \param[in] identifier: Nonzero product-generated deduplication identity.
 * \param[in] payload: Immutable bytes, NULL permitted only when empty.
 * \param[in] payload_bytes: Exact payload length, representable in uint32_t.
 * \param[out] image: Disjoint output or NULL for a required-size query.
 * \param[in,out] image_bytes: Capacity on entry, required bytes on
 * success/full. \return SUCCESS, INVALID or EXHAUSTED; rejected capacity
 * changes no image. \note CRC32 detects damage, not authentication. A record ID
 * does not create reliable delivery; ACK, retention and retries remain product
 * policy.
 */
nx_result_t nx_persistent_record_encode(uint32_t schema, uint64_t identifier,
                                        const void* payload,
                                        size_t payload_bytes, void* image,
                                        size_t* image_bytes);
/**
 * \brief Validate exact format/schema/checksum before publishing a payload
 * view. \param[in] schema: Exact expected product schema, never implicitly
 * migrated. \param[in] image: Stable immutable complete record bytes.
 * \param[in] image_bytes: Exact encoded length; trailing bytes are rejected.
 * \param[out] view: Disjoint output, unchanged on any failure.
 * \return SUCCESS, INVALID, UNSUPPORTED format/schema or IO for damaged data.
 * \note No allocation or retained library pointer. The caller owns backing
 *       image lifetime while using its returned view. No Flash access occurs.
 */
nx_result_t nx_persistent_record_decode(uint32_t schema, const void* image,
                                        size_t image_bytes,
                                        nx_persistent_record_view_t* view);
#ifdef __cplusplus
}
#endif
#endif
