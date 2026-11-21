/**
 * \file            rtt.h
 * \brief           Whole-record nonblocking SEGGER RTT channels
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_RTT_H
#define NEXUS_COMPONENTS_RTT_H
#include "nexus/components/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Explicit channel storage and saturating record statistics.
 *
 * \note            One immutable channel assignment per object; caller owns its
 *                  name, up/down buffers and lifetime. Channel uniqueness is a
 *                  composition contract. Target calls are task-only, privileged
 *                  on one CPU/security domain. An atomic try-guard serializes
 *                  the pinned upstream without OS locks or mask changes. Host
 *                  debugger access follows the unchanged upstream protocol.
 */
typedef struct {
    unsigned channel;
    uint8_t* up;
    size_t up_bytes;
    uint8_t* down;
    size_t down_bytes;
    uint32_t accepted;
    uint32_t dropped;
    uint32_t received;
    bool initialized;
    bool stopped;
} nx_rtt_t;

/**
 * \brief           Configure caller buffers with upstream nonblocking SKIP
 * mode.
 *
 * \param[in,out]   rtt: Zeroed unpublished object, not already
 * initialized.
 *
 * \param[in]       channel: Explicit channel index, one through
 * three. Upstream channel zero remains its compile-time housekeeping channel.
 *
 * \param[in]       name: Immutable NUL-terminated static channel name.
 *
 * \param[out]      up: Exclusive caller-owned target-to-host ring storage.
 *
 * \param[in]       up_bytes: At least two bytes, at most UINT_MAX.
 *
 * \param[out]      down: Exclusive caller-owned host-to-target ring storage.
 *
 * \param[in]       down_bytes: At least two bytes, at most UINT_MAX.
 *
 * \return          SUCCESS, INVALID, STATE, CONTEXT or nonblocking BUSY.
 *
 * \note            This explicit initialization is never run for unselected
 *                  inventory. The upstream control block is initialized once.
 *                  Direct calls to vendor RTT APIs are outside this adapter's
 *                  serialization domain and must not coexist with it.
 */
nx_result_t nx_rtt_init(nx_rtt_t* rtt, unsigned channel, const char* name,
                        void* up, size_t up_bytes, void* down,
                        size_t down_bytes);

/**
 * \brief           Return the existing Log port for complete SKIP records.
 *
 * \param[in,out]   rtt: Initialized channel, retained through publisher join.
 *
 * \return          Existing synchronous sink interface, or empty for NULL.
 *
 * \note            No partial write reports success. Full/no-host buffers
 * return OVERFLOW without waiting or overwriting older data. flush reports BUSY
 * until the host consumed the ring; it never waits and does not prove durable
 * capture. Fault use is prohibited.
 */
nx_log_sink_port_t nx_rtt_log_sink(nx_rtt_t* rtt);

/**
 * \brief           Copy available host bytes once, with no polling loop.
 *
 * \param[in,out]   rtt: Live task-owned channel.
 *
 * \param[out]      bytes: Separate destination released on return.
 *
 * \param[in]       capacity: Destination bytes, at most UINT_MAX.
 *
 * \param[out]      count: Copied bytes; zero for EMPTY.
 *
 * \return          SUCCESS, EMPTY, BUSY, INVALID, CONTEXT or STATE.
 *
 * \note            No kernel API, clock, wait, allocation or hidden executor.
 */
nx_result_t nx_rtt_read(nx_rtt_t* rtt, void* bytes, size_t capacity,
                        size_t* count);

/**
 * \brief           Close target admission without overwriting pending host
 * data.
 *
 * \param[in,out]   rtt: Unique task owner after other publishers have
 * joined.
 *
 * \return          SUCCESS, BUSY while host data remains, or
 * context/state error.
 *
 * \note            SUCCESS proves local closure and consumed uplink, not host
 * join. The upstream control block still refers to ring/name storage. Retain it
 * for the image/debugger lifetime; debugger detachment remains external.
 */
nx_result_t nx_rtt_stop(nx_rtt_t* rtt);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_RTT_H */
