/**
 * \file            byte_stream.h
 * \brief           Immediate partial byte transport without retained buffers
 * \author          Nexus Team
 */
#ifndef NEXUS_NETWORK_BYTE_STREAM_H
#define NEXUS_NETWORK_BYTE_STREAM_H

#include "nexus/core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Immutable methods for a serialized nonblocking byte stream.
 * \note Every call has a finite absolute monotonic deadline in the transport's
 *       explicit clock domain. Calls do not wait, create workers or retain
 *       input/output buffers after return. An accepted write copies bytes into
 *       the transport's own bounded storage; it does not promise peer delivery.
 *       SUCCESS writes 1..requested bytes for a nonempty write, or reads
 *       1..requested bytes. SUCCESS with read count zero means orderly EOF.
 *       BUSY has count zero and means retry after explicit owner progress.
 *       Other results are errors, never alternate encodings of would-block.
 *       Zero-byte calls return SUCCESS/count zero without underlying I/O.
 *       Output count is disjoint from all buffers and live transport storage.
 *       Transport, table and context outlive every consumer and its shutdown.
 */
typedef struct {
    nx_result_t (*write)(void* context, const uint8_t* data, size_t bytes,
                         size_t* accepted, nx_time_us_t deadline);
    nx_result_t (*read)(void* context, uint8_t* data, size_t bytes,
                        size_t* received, nx_time_us_t deadline);
} nx_byte_stream_ops_t;

/** \brief Stable readonly face; no factory lookup, allocation or start. */
typedef struct {
    const nx_byte_stream_ops_t* ops;
    void* context;
} nx_byte_stream_t;

#ifdef __cplusplus
}
#endif
#endif
