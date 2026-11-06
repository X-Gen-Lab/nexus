/**
 * \file            parallel16.h
 * \brief           Settled 16-bit command and data endpoint
 * \author          Nexus Team
 */
#ifndef NEXUS_PARALLEL16_H
#define NEXUS_PARALLEL16_H

#include "nexus/core/time.h"
#include "nexus/io/fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Shared immutable methods for one 16-bit parallel endpoint
 * \note            Words are native halfwords; the selected physical route
 *                  fixes command/data addresses and electrical bus timing.
 *                  Methods use one serialized task executor and one finite
 *                  absolute clock domain. No span survives a return, including
 *                  failure. SUCCESS requires the whole requested word prefix.
 */
typedef struct {
    nx_result_t (*command)(void* context, uint16_t command,
                           nx_time_us_t deadline);
    nx_result_t (*write)(void* context, const uint16_t* words, size_t count,
                         size_t* transferred, nx_time_us_t deadline);
    nx_result_t (*read)(void* context, uint16_t* words, size_t count,
                        size_t* transferred, nx_time_us_t deadline);
} nx_parallel16_ops_t;

/** \brief Stable readonly face; lookup acquires no bus or bank lease. */
struct nx_parallel16_endpoint {
    const nx_parallel16_ops_t* ops;
    void* context;
};

/**
 * \brief           Send one complete command-register word
 * \param[in]       endpoint: Live exact bank under its serialized owner
 * \param[in]       command: Native command word, not a byte-count address
 * \param[in]       deadline: Finite absolute provider-clock deadline
 * \return          SUCCESS, INVALID, UNSUPPORTED or provider failure
 * \note            Partial device effects can survive a failure. No reset,
 *                  command replay, worker, DMA or GPIO control is implied.
 */
nx_result_t
nx_parallel16_endpoint_command(const nx_parallel16_endpoint_t* endpoint,
                               uint16_t command, nx_time_us_t deadline);

/**
 * \brief           Write one settled data-register word sequence
 * \param[in]       endpoint: Same live bank and task executor
 * \param[in]       words: Aligned immutable span, disjoint from known controls
 * \param[in]       count: Positive word count within the selected mode bound
 * \param[out]      transferred: Distinct exact completed word prefix
 * \param[in]       deadline: One finite absolute deadline for the sequence
 * \return          SUCCESS only for count words; provider error may report a
 *                  shorter prefix. Parameter rejection preserves output.
 * \note            Every return ends buffer access. It does not revoke the
 *                  bank lease or prove physical panel state. Consumer and bus
 *                  lifecycle must join before shared infrastructure withdrawal.
 */
nx_result_t
nx_parallel16_endpoint_write(const nx_parallel16_endpoint_t* endpoint,
                             const uint16_t* words, size_t count,
                             size_t* transferred, nx_time_us_t deadline);

/**
 * \brief           Read one settled data-register word sequence
 * \param[in]       endpoint: Same live bank and task executor
 * \param[out]      words: Aligned writable storage, disjoint from controls
 * \param[in]       count: Positive selected word count
 * \param[out]      transferred: Distinct exact completed word prefix
 * \param[in]       deadline: One finite absolute deadline for the sequence
 * \return          SUCCESS only for count words; failed-prefix bytes outside
 *                  transferred are unspecified. Parameter rejection preserves
 *                  transferred and performs no access.
 * \note            Dummy-read/command sequencing belongs to the device
 *                  protocol. This endpoint never interprets a controller ID.
 */
nx_result_t
nx_parallel16_endpoint_read(const nx_parallel16_endpoint_t* endpoint,
                            uint16_t* words, size_t count, size_t* transferred,
                            nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif
