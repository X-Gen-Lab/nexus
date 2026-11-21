/**
 * \file            persistent_record.c
 * \brief           Explicit record wire format using shared endian and CRC math
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/persistent_record.h"
#include "nexus/algorithms/crc.h"
#include "nexus/algorithms/endian.h"
#include <string.h>

/** \brief Reject output aliases before writing even the first header byte. */
static bool overlaps(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t left = (uintptr_t)first;
    uintptr_t right = (uintptr_t)second;
    return first_bytes > UINTPTR_MAX - left ||
           second_bytes > UINTPTR_MAX - right ||
           (first_bytes != 0U && second_bytes != 0U &&
            left < right + second_bytes && right < left + first_bytes);
}

/** \brief Known wire widths have no host alignment or endian dependency. */
static uint64_t number(const uint8_t* image, size_t offset, size_t width) {
    uint64_t result = 0U;
    (void)nx_endian_read(image + offset, width, true, &result);
    return result;
}

/** \brief Cover metadata and payload, excluding only the stored checksum word.
 */
static uint32_t checksum(const uint8_t* image, size_t payload_bytes) {
    uint32_t state = nx_crc32_iso_hdlc.initial;
    uint32_t result = 0U;
    (void)nx_crc_update(&nx_crc32_iso_hdlc, image, 28U, &state);
    (void)nx_crc_update(&nx_crc32_iso_hdlc,
                        image + NX_PERSISTENT_RECORD_HEADER_BYTES,
                        payload_bytes, &state);
    (void)nx_crc_finish(&nx_crc32_iso_hdlc, state, &result);
    return result;
}

/** \brief Validate complete admission before encoding any header or payload. */
nx_result_t nx_persistent_record_encode(uint32_t schema, uint64_t identifier,
                                        const void* payload,
                                        size_t payload_bytes, void* image,
                                        size_t* image_bytes) {
    if (image_bytes == NULL || identifier == 0U ||
        (payload == NULL && payload_bytes != 0U) ||
        payload_bytes > UINT32_MAX ||
        payload_bytes > SIZE_MAX - NX_PERSISTENT_RECORD_HEADER_BYTES) {
        return NX_ERROR_INVALID;
    }
    size_t required = NX_PERSISTENT_RECORD_HEADER_BYTES + payload_bytes;
    size_t capacity = *image_bytes;
    if (image != NULL &&
        overlaps(image, capacity < required ? capacity : required, image_bytes,
                 sizeof(*image_bytes))) {
        return NX_ERROR_INVALID;
    }
    if (image != NULL && capacity < required) {
        *image_bytes = required;
        return NX_ERROR_EXHAUSTED;
    }
    if (image != NULL && overlaps(image, required, payload, payload_bytes)) {
        return NX_ERROR_INVALID;
    }
    *image_bytes = required;
    if (image == NULL) {
        return NX_SUCCESS;
    }
    uint8_t* output = image;
    (void)nx_endian_write(output, 4U, true, UINT32_C(0x3152584e));
    (void)nx_endian_write(output + 4U, 2U, true, 1U);
    (void)nx_endian_write(output + 6U, 2U, true,
                          NX_PERSISTENT_RECORD_HEADER_BYTES);
    (void)nx_endian_write(output + 8U, 4U, true, schema);
    (void)nx_endian_write(output + 12U, 8U, true, identifier);
    (void)nx_endian_write(output + 20U, 4U, true, payload_bytes);
    (void)nx_endian_write(output + 24U, 4U, true, 0U);
    if (payload_bytes != 0U) {
        memcpy(output + NX_PERSISTENT_RECORD_HEADER_BYTES, payload,
               payload_bytes);
    }
    (void)nx_endian_write(output + 28U, 4U, true,
                          checksum(output, payload_bytes));
    return NX_SUCCESS;
}

/** \brief Publish a borrowed payload only after exact-format validation. */
nx_result_t nx_persistent_record_decode(uint32_t schema, const void* image,
                                        size_t image_bytes,
                                        nx_persistent_record_view_t* view) {
    if (image == NULL || view == NULL ||
        image_bytes < NX_PERSISTENT_RECORD_HEADER_BYTES ||
        overlaps(image, image_bytes, view, sizeof(*view))) {
        return NX_ERROR_INVALID;
    }
    const uint8_t* input = image;
    if (number(input, 0U, 4U) != UINT32_C(0x3152584e) ||
        number(input, 4U, 2U) != 1U ||
        number(input, 6U, 2U) != NX_PERSISTENT_RECORD_HEADER_BYTES ||
        number(input, 8U, 4U) != schema || number(input, 24U, 4U) != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t payload_bytes = (size_t)number(input, 20U, 4U);
    uint64_t identifier = number(input, 12U, 8U);
    if (identifier == 0U ||
        payload_bytes != image_bytes - NX_PERSISTENT_RECORD_HEADER_BYTES ||
        number(input, 28U, 4U) != checksum(input, payload_bytes)) {
        return NX_ERROR_IO;
    }
    nx_persistent_record_view_t valid = {
        identifier, input + NX_PERSISTENT_RECORD_HEADER_BYTES, payload_bytes};
    *view = valid;
    return NX_SUCCESS;
}
