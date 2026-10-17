/**
 * \file            i2c_long.c
 * \brief           Checked optional long-message dispatch and storage bounds
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/i2c_long.h"

/** \brief Interval bounds are validated before overlap arithmetic. */
static bool overlap(uintptr_t first, size_t first_size, uintptr_t second,
                    size_t second_size) {
    return first < second + second_size && second < first + first_size;
}

nx_result_t nx_i2c_long_messages_validate(const nx_i2c_message_t* messages,
                                          size_t count) {
    if (messages == NULL || count == 0U ||
        (uintptr_t)messages % _Alignof(nx_i2c_message_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (count > NX_I2C_MAX_MESSAGES) {
        return NX_ERROR_UNSUPPORTED;
    }
    uintptr_t descriptors = (uintptr_t)messages;
    size_t descriptor_bytes = count * sizeof(*messages);
    if (descriptor_bytes > UINTPTR_MAX - descriptors) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < count; ++i) {
        const nx_i2c_message_t* item = &messages[i];
        uintptr_t data = (uintptr_t)item->data;
        if (item->data == NULL || item->length == 0U ||
            item->length > UINTPTR_MAX - data) {
            return NX_ERROR_INVALID;
        }
        if (item->length > NX_I2C_LONG_MAX_MESSAGE_BYTES) {
            return NX_ERROR_UNSUPPORTED;
        }
        if (overlap(data, item->length, descriptors, descriptor_bytes)) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0U; j < i; ++j) {
            const nx_i2c_message_t* previous = &messages[j];
            if (item->read != previous->read &&
                overlap(data, item->length, (uintptr_t)previous->data,
                        previous->length)) {
                return NX_ERROR_INVALID;
            }
        }
    }
    return NX_SUCCESS;
}

nx_result_t
nx_i2c_long_endpoint_transaction(const nx_i2c_long_endpoint_t* endpoint,
                                 nx_i2c_message_t* messages, size_t count,
                                 nx_time_us_t deadline, size_t* transferred) {
    if (endpoint == NULL || endpoint->ops == NULL ||
        endpoint->context == NULL) {
        return NX_ERROR_INVALID;
    }
    if (endpoint->ops->transaction == NULL) {
        return NX_ERROR_UNSUPPORTED;
    }
    return endpoint->ops->transaction(endpoint->context, messages, count,
                                      deadline, transferred);
}
