/**
 * \file            portable_consumer.h
 * \brief           Kernel-independent consumer used by OS contract fixtures
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_PORTABLE_CONSUMER_H
#define NEXUS_TEST_PORTABLE_CONSUMER_H

#include "nexus/os/channel.h"
#include "nexus/os/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Caller-owned state survives until the consumer task joins. */
typedef struct {
    nx_channel_t* channel;
    nx_channel_waiter_t* waiter;
    uint32_t count;
    uint32_t sum;
    nx_result_t result;
} nx_portable_consumer_t;

/**
 * \brief           Consume uint32_t items until the channel is closed/drained
 * \param[in,out]   context: State retained through successful task join
 * \note            Task context. The owner closes the channel and joins before
 *                  reclaiming state, waiter or channel. No native API is used.
 */
void nx_portable_consumer_run(void* context);

/**
 * \brief           Exchange one copied item through the public queue contract
 * \param[in,out]   queue: Live caller-owned queue
 * \param[in]       input: One complete item, borrowed during this call
 * \param[out]      output: One complete item, valid on success only
 * \param[in]       deadline: Absolute monotonic deadline, shared by both stages
 * \return          The first failed stage, or success after receive
 * \note            Task context. This fixture assumes exclusive queue access.
 */
nx_result_t nx_portable_queue_exchange(nx_queue_t* queue, const void* input,
                                       void* output, nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_TEST_PORTABLE_CONSUMER_H */
