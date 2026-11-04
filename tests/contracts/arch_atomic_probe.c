/**
 * \file            arch_atomic_probe.c
 *
 * \brief           Retained compiler probes of the real public atomic port.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-11-04
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/atomic.h"

#define NX_PROBE __attribute__((noinline, used))

/** \brief Retain the relaxed publication load for instruction inspection. */
NX_PROBE uint32_t nx_probe_atomic_load_relaxed(const uint32_t* value) {
    return nx_atomic_u32_load_relaxed(value);
}

/** \brief Retain the acquiring publication load for instruction inspection. */
NX_PROBE uint32_t nx_probe_atomic_load_acquire(const uint32_t* value) {
    return nx_atomic_u32_load_acquire(value);
}

/** \brief Retain the relaxed publication store for instruction inspection. */
NX_PROBE void nx_probe_atomic_store_relaxed(uint32_t* value, uint32_t desired) {
    nx_atomic_u32_store_relaxed(value, desired);
}

/** \brief Retain the releasing publication store for instruction inspection. */
NX_PROBE void nx_probe_atomic_store_release(uint32_t* value, uint32_t desired) {
    nx_atomic_u32_store_release(value, desired);
}

/** \brief Retain the acquiring addition for instruction inspection. */
NX_PROBE uint32_t nx_probe_atomic_fetch_add_acq_rel(uint32_t* value,
                                                    uint32_t amount) {
    return nx_atomic_u32_fetch_add_acq_rel(value, amount);
}

/** \brief Retain the releasing addition for instruction inspection. */
NX_PROBE uint32_t nx_probe_atomic_fetch_add_release(uint32_t* value,
                                                    uint32_t amount) {
    return nx_atomic_u32_fetch_add_release(value, amount);
}

/** \brief Retain the acquiring subtraction for instruction inspection. */
NX_PROBE uint32_t nx_probe_atomic_fetch_sub_acq_rel(uint32_t* value,
                                                    uint32_t amount) {
    return nx_atomic_u32_fetch_sub_acq_rel(value, amount);
}

/** \brief Retain the releasing subtraction for instruction inspection. */
NX_PROBE uint32_t nx_probe_atomic_fetch_sub_release(uint32_t* value,
                                                    uint32_t amount) {
    return nx_atomic_u32_fetch_sub_release(value, amount);
}

/** \brief Retain strong comparison including the separate failure output. */
NX_PROBE bool nx_probe_atomic_compare_exchange_acq_rel(uint32_t* value,
                                                       uint32_t* expected,
                                                       uint32_t desired) {
    return nx_atomic_u32_compare_exchange_acq_rel(value, expected, desired);
}
