/**
 * \file            framing.c
 * \brief           Capacity-checked COBS encoding and bounded stream decoding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/framing.h"

/** \brief Validate finite pointer spans without overflowing address arithmetic.
 */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (an != 0 && bn != 0 && x < y + bn && y < x + an);
}
/** \brief Compute the exact encoded extent before writing any caller bytes. */
nx_result_t nx_cobs_encode(const uint8_t* data, size_t size, uint8_t* output,
                           size_t capacity, size_t* written) {
    if (output == NULL || written == NULL || (data == NULL && size != 0) ||
        overlap(data, size, output, capacity) ||
        overlap(output, capacity, written, sizeof(*written)) ||
        overlap(data, size, written, sizeof(*written))) {
        return NX_ERROR_INVALID;
    }
    if (size > SIZE_MAX - size / 254 - 2) {
        return NX_ERROR_OVERFLOW;
    }
    size_t required = size + 2;
    unsigned run = 0;
    for (size_t i = 0; i < size; ++i) {
        if (data[i] == 0) {
            run = 0;
        } else if (++run == 254) {
            ++required;
            run = 0;
        }
    }
    if (required > capacity) {
        return NX_ERROR_EXHAUSTED;
    }
    size_t code_position = 0;
    size_t position = 1;
    uint8_t code = 1;
    for (size_t i = 0; i < size; ++i) {
        if (data[i] == 0) {
            output[code_position] = code;
            code_position = position++;
            code = 1;
        } else {
            output[position++] = data[i];
            if (++code == 255) {
                output[code_position] = code;
                code_position = position++;
                code = 1;
            }
        }
    }
    output[code_position] = code;
    output[position++] = 0;
    *written = position;
    return NX_SUCCESS;
}
/** \brief Validate every code and output extent before a second copying pass.
 */
nx_result_t nx_cobs_decode(const uint8_t* data, size_t size, uint8_t* output,
                           size_t capacity, size_t* written) {
    if (data == NULL || size == 0 || output == NULL || written == NULL ||
        (data != output && overlap(data, size, output, capacity)) ||
        overlap(output, capacity, written, sizeof(*written)) ||
        overlap(data, size, written, sizeof(*written))) {
        return NX_ERROR_INVALID;
    }
    size_t required = 0;
    for (size_t i = 0; i < size;) {
        uint8_t code = data[i++];
        if (code == 0 || (size_t)(code - 1U) > size - i) {
            return NX_ERROR_INVALID;
        }
        for (unsigned j = 1; j < code; ++j) {
            if (data[i++] == 0) {
                return NX_ERROR_INVALID;
            }
            ++required;
        }
        if (code != 255 && i < size) {
            ++required;
        }
    }
    if (required > capacity) {
        return NX_ERROR_EXHAUSTED;
    }
    size_t position = 0;
    for (size_t i = 0; i < size;) {
        uint8_t code = data[i++];
        for (unsigned j = 1; j < code; ++j) {
            output[position++] = data[i++];
        }
        if (code != 255 && i < size) {
            output[position++] = 0;
        }
    }
    *written = position;
    return NX_SUCCESS;
}
/** \brief Retain only the explicit caller buffer. */
nx_result_t nx_frame_decoder_init(nx_frame_decoder_t* d, uint8_t* storage,
                                  size_t capacity) {
    if (d == NULL || storage == NULL || capacity == 0 ||
        overlap(d, sizeof(*d), storage, capacity)) {
        return NX_ERROR_INVALID;
    }
    *d = (nx_frame_decoder_t){storage, capacity, 0, false, false};
    return NX_SUCCESS;
}
/** \brief Preserve packet borrowing and discard overflow through a delimiter.
 */
nx_result_t nx_frame_decoder_feed(nx_frame_decoder_t* d, uint8_t byte) {
    if (d == NULL || d->storage == NULL) {
        return NX_ERROR_INVALID;
    }
    if (d->ready) {
        return NX_ERROR_BUSY;
    }
    if (d->discarding) {
        if (byte == 0) {
            d->discarding = false;
            d->size = 0;
        }
        return NX_ERROR_BUSY;
    }
    if (byte == 0) {
        if (d->size == 0) {
            return NX_ERROR_BUSY;
        }
        size_t size = 0;
        nx_result_t result =
            nx_cobs_decode(d->storage, d->size, d->storage, d->capacity, &size);
        d->size = result == NX_SUCCESS ? size : 0;
        d->ready = result == NX_SUCCESS;
        return result;
    }
    if (d->size == d->capacity) {
        d->discarding = true;
        d->size = 0;
        return NX_ERROR_OVERFLOW;
    }
    d->storage[d->size++] = byte;
    return NX_ERROR_BUSY;
}
/** \brief Expose only a completed packet as an explicit borrowed view. */
nx_result_t nx_frame_decoder_peek(const nx_frame_decoder_t* d,
                                  const uint8_t** data, size_t* size) {
    if (d == NULL || d->storage == NULL || data == NULL || size == NULL ||
        overlap(d, sizeof(*d), data, sizeof(*data)) ||
        overlap(d, sizeof(*d), size, sizeof(*size)) ||
        overlap(d->storage, d->capacity, data, sizeof(*data)) ||
        overlap(d->storage, d->capacity, size, sizeof(*size)) ||
        overlap(data, sizeof(*data), size, sizeof(*size))) {
        return NX_ERROR_INVALID;
    }
    if (!d->ready) {
        return NX_ERROR_EMPTY;
    }
    *data = d->storage;
    *size = d->size;
    return NX_SUCCESS;
}
/** \brief Release the stable packet before accepting more wire bytes. */
nx_result_t nx_frame_decoder_release(nx_frame_decoder_t* d) {
    if (d == NULL || d->storage == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!d->ready) {
        return NX_ERROR_EMPTY;
    }
    d->ready = false;
    d->size = 0;
    return NX_SUCCESS;
}
