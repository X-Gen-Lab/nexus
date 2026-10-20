/**
 * \file            retained_time.h
 * \brief           Private serialized retained-clock state, never public SDK
 * ABI \author          Nexus Team
 */
#ifndef NX_U575_RETAINED_TIME_PRIVATE_H
#define NX_U575_RETAINED_TIME_PRIVATE_H
#include "nexus/components/stm32u575_retained_time.h"
typedef struct {
    uint64_t ticks;
    uint64_t previous;
    uint64_t generation;
    const void* wake_owner;
    uint32_t max_gap_ticks;
    uint32_t magic;
    bool opened;
    bool tainted;
    bool alarm_a_copy;
} nx_stm32u575_retained_time_owner_t;
#endif
