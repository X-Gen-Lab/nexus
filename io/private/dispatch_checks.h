/**
 * \file            dispatch_checks.h
 * \brief           Pure extent checks for typed operation dispatch
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_DISPATCH_CHECKS_H
#define NEXUS_IO_DISPATCH_CHECKS_H
#include "nexus/core/status.h"

/** \brief Admit an aligned complete extent without dereferencing it. */
static inline bool nx_io_dispatch_span(const void* pointer, size_t bytes,
                                       size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)pointer;
}
/** \brief Both complete extents have already passed wrapping checks. */
static inline bool nx_io_dispatch_overlap(const void* first, size_t first_bytes,
                                          const void* second,
                                          size_t second_bytes) {
    return (uintptr_t)first < (uintptr_t)second + second_bytes &&
           (uintptr_t)second < (uintptr_t)first + first_bytes;
}
#endif
