/**
 * \file            graphics_checks.h
 * \brief           Checked graphics byte ranges and geometry without effects
 * \author          Nexus Team
 */
#ifndef NX_GRAPHICS_CHECKS_H
#define NX_GRAPHICS_CHECKS_H
#include "nexus/io/image.h"

/** \brief Require a representable full extent before pointer arithmetic. */
static inline bool nx_graphics_span(const void* p, size_t bytes, size_t align) {
    return p != NULL && bytes != 0U && (uintptr_t)p % align == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)p;
}
/** \brief Compare only spans whose exclusive ends were already checked. */
static inline bool nx_graphics_overlap(const void* a, size_t an, const void* b,
                                       size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
/** \brief Convert exactly the declared native-memory pixel format. */
static inline size_t nx_graphics_pixel_bytes(nx_image_format_t format) {
    return format == NX_IMAGE_RGB565     ? 2U
           : format == NX_IMAGE_ARGB8888 ? 4U
                                         : 0U;
}
/** \brief Check full row/stride extent and output byte-count arithmetic. */
static inline nx_result_t nx_graphics_extent(size_t width, size_t height,
                                             size_t pixel_bytes, size_t stride,
                                             size_t capacity) {
    if (width == 0U || height == 0U || pixel_bytes == 0U)
        return NX_ERROR_INVALID;
    if (width > SIZE_MAX / pixel_bytes)
        return NX_ERROR_EXHAUSTED;
    size_t row = width * pixel_bytes;
    if (height > SIZE_MAX / row ||
        (height > 1U && stride > (SIZE_MAX - row) / (height - 1U)))
        return NX_ERROR_EXHAUSTED;
    if (stride < row || capacity < (height - 1U) * stride + row)
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}
#endif /* NX_GRAPHICS_CHECKS_H */
