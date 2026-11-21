/**
 * \file            endian.h
 * \brief           Unaligned byte-order conversion without pointer punning
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_ENDIAN_H
#define NEXUS_ALGORITHMS_ENDIAN_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Read an unsigned integer from an unaligned byte sequence.
 * \param[in]       data: At least width readable bytes, borrowed for call.
 * \param[in]       width: Exactly 1, 2, 4 or 8 bytes.
 * \param[in]       little_endian: True selects least significant byte first.
 * \param[out]      out: Separate caller result, unchanged on invalid arguments.
 * \return          SUCCESS or INVALID; bounded byte math, no OS/heap/state.
 */
nx_result_t nx_endian_read(const void* data, size_t width, bool little_endian,
                           uint64_t* out);
/**
 * \brief           Write an unsigned integer without silently truncating it.
 * \param[out]      data: At least width writable bytes, changed on success
 * only. \param[in]       width: Exactly 1, 2, 4 or 8 bytes. \param[in]
 * little_endian: True selects least significant byte first. \param[in] value:
 * Value representable by the selected byte width. \return          SUCCESS,
 * INVALID or OVERFLOW; no retained references/heap.
 */
nx_result_t nx_endian_write(void* data, size_t width, bool little_endian,
                            uint64_t value);
#ifdef __cplusplus
}
#endif
#endif
