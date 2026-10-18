/**
 * \file            rtc_lptim_internal.h
 * \brief           Bounded span checks and raw calendar arithmetic
 * \author          Nexus Team
 */
#ifndef NEXUS_RTC_LPTIM_INTERNAL_H
#define NEXUS_RTC_LPTIM_INTERNAL_H
#include "nexus/io/rtc.h"
/** \brief Validate the entire span before arithmetic or typed access. */
static inline bool nx_time_span(const void* pointer, size_t size,
                                size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           size <= UINTPTR_MAX - address;
}
/** \brief Both complete spans have already passed the nonwrapping check. */
static inline bool nx_time_overlap(const void* first, size_t first_size,
                                   const void* second, size_t second_size) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    return a < b + second_size && b < a + first_size;
}
/** \brief Month is already 1..12; raw YY leap arithmetic has no century. */
static inline uint8_t nx_time_month_days(uint8_t year, uint8_t month) {
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
    return month == 2U && year % 4U == 0U ? 29U : days[month - 1U];
}
#endif
