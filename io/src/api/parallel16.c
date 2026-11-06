/**
 * \file            parallel16.c
 * \brief           Checked settled parallel command and word dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/parallel16.h"

/** \brief Check known immutable controls before reading their methods. */
static bool face_valid(const nx_parallel16_endpoint_t* endpoint) {
    return endpoint != NULL && endpoint->ops != NULL &&
           endpoint->context != NULL;
}

/** \brief All ranges are bounded and disjoint from known control storage. */
static bool span_valid(const nx_parallel16_endpoint_t* endpoint,
                       const uint16_t* words, size_t count,
                       const size_t* transferred) {
    uintptr_t begin = (uintptr_t)words;
    uintptr_t output = (uintptr_t)transferred;
    uintptr_t face = (uintptr_t)endpoint;
    uintptr_t ops = (uintptr_t)endpoint->ops;
    if (words == NULL || transferred == NULL || count == 0U ||
        count > SIZE_MAX / sizeof(*words) || begin % _Alignof(uint16_t) != 0U ||
        output % _Alignof(size_t) != 0U ||
        begin > UINTPTR_MAX - count * sizeof(*words) ||
        output > UINTPTR_MAX - sizeof(*transferred) ||
        face > UINTPTR_MAX - sizeof(*endpoint) ||
        ops > UINTPTR_MAX - sizeof(*endpoint->ops))
        return false;
    uintptr_t end = begin + count * sizeof(*words);
    uintptr_t output_end = output + sizeof(*transferred);
    return !(begin < output_end && output < end) &&
           !(begin < face + sizeof(*endpoint) && face < end) &&
           !(begin < ops + sizeof(*endpoint->ops) && ops < end) &&
           !(output < face + sizeof(*endpoint) && face < output_end) &&
           !(output < ops + sizeof(*endpoint->ops) && ops < output_end);
}

/** \brief Never publish an impossible prefix or accept incomplete SUCCESS. */
static nx_result_t complete(nx_result_t result, size_t count,
                            size_t* transferred) {
    if (result != NX_SUCCESS)
        return result;
    if (*transferred > count) {
        *transferred = 0U;
        return NX_ERROR_IO;
    }
    return result == NX_SUCCESS && *transferred != count ? NX_ERROR_IO : result;
}

nx_result_t
nx_parallel16_endpoint_command(const nx_parallel16_endpoint_t* endpoint,
                               uint16_t command, nx_time_us_t deadline) {
    if (!face_valid(endpoint) || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (endpoint->ops->command == NULL)
        return NX_ERROR_UNSUPPORTED;
    return endpoint->ops->command(endpoint->context, command, deadline);
}

nx_result_t
nx_parallel16_endpoint_write(const nx_parallel16_endpoint_t* endpoint,
                             const uint16_t* words, size_t count,
                             size_t* transferred, nx_time_us_t deadline) {
    if (!face_valid(endpoint) || deadline == NX_DEADLINE_NEVER ||
        !span_valid(endpoint, words, count, transferred))
        return NX_ERROR_INVALID;
    if (endpoint->ops->write == NULL)
        return NX_ERROR_UNSUPPORTED;
    nx_result_t result = endpoint->ops->write(endpoint->context, words, count,
                                              transferred, deadline);
    return complete(result, count, transferred);
}

nx_result_t
nx_parallel16_endpoint_read(const nx_parallel16_endpoint_t* endpoint,
                            uint16_t* words, size_t count, size_t* transferred,
                            nx_time_us_t deadline) {
    if (!face_valid(endpoint) || deadline == NX_DEADLINE_NEVER ||
        !span_valid(endpoint, words, count, transferred))
        return NX_ERROR_INVALID;
    if (endpoint->ops->read == NULL)
        return NX_ERROR_UNSUPPORTED;
    nx_result_t result = endpoint->ops->read(endpoint->context, words, count,
                                             transferred, deadline);
    return complete(result, count, transferred);
}
