/**
 * \file            portable_consumer.c
 * \brief           One business source consumes the public OS contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "portable_consumer.h"

/** \brief Channel closure, rather than a kernel handle, ends the consumer. */
void nx_portable_consumer_run(void* context) {
    nx_portable_consumer_t* consumer = context;
    for (;;) {
        uint32_t value = 0;
        nx_result_t result = nx_channel_receive_until(
            consumer->channel, &value, NX_DEADLINE_NEVER, consumer->waiter);
        if (result != NX_SUCCESS) {
            consumer->result = result;
            return;
        }
        ++consumer->count;
        consumer->sum += value;
    }
}

/** \brief Both stages retain one absolute budget and ordinary copy ownership.
 */
nx_result_t nx_portable_queue_exchange(nx_queue_t* queue, const void* input,
                                       void* output, nx_time_us_t deadline) {
    nx_result_t result = nx_queue_send_until(queue, input, deadline);
    return result == NX_SUCCESS
               ? nx_queue_receive_until(queue, output, deadline)
               : result;
}
