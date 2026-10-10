/**
 * \file            atomic.h
 *
 * \brief           Inline naturally aligned 32-bit publication and RMW ports.
 *
 * \author          Nexus Team
 *
 * \note            Storage is a live, naturally aligned uint32_t; all
 *                  concurrent accesses use these helpers or compatible
 *                  compiler atomics. Initialize before publishing storage.
 *                  Load/store use compiler acquire/release operations on all
 *                  maintained CPUs and never mask interrupts. Other than
 *                  ARMv6-M, RMW uses compiler lock-free operations; exclusive
 *                  instruction retries do not establish a fixed cycle bound.
 *
 * \note            ARMv6-M RMW is CPU-local, bounded saved-PRIMASK exclusion.
 *                  Only privileged Thread/configurable-IRQ contexts on one
 *                  CPU may access this domain. NMI/HardFault, unprivileged
 *                  callers, another core, DMA and another security domain
 *                  must not access its storage concurrently. It is not an
 *                  SMP or bus atomic operation. No callback, allocation,
 *                  blocking call or payload copy occurs while masked.
 */
#ifndef NEXUS_ARCH_ATOMIC_H
#define NEXUS_ARCH_ATOMIC_H
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#if !(defined(__GNUC__) || defined(__clang__))
#error "Nexus inline atomics require the maintained GCC/Clang compiler port"
#endif

#ifdef __cplusplus
static_assert(sizeof(uint32_t) == 4 && sizeof(unsigned) == 4 && CHAR_BIT == 8,
              "Nexus atomics require naturally aligned 32-bit unsigned words");
#else
_Static_assert(sizeof(uint32_t) == 4 && sizeof(unsigned) == 4 && CHAR_BIT == 8,
               "Nexus atomics require naturally aligned 32-bit unsigned words");
#endif

#if defined(__ARM_ARCH_6M__)
#include "nexus/arch/arch.h"
#elif __GCC_ATOMIC_INT_LOCK_FREE != 2
#error "Nexus RMW requires lock-free words or the reviewed ARMv6-M IRQ port"
#endif

/** \brief Read one word without publishing or acquiring other data. */
static inline uint32_t nx_atomic_u32_load_relaxed(const uint32_t* value) {
    return __atomic_load_n(value, __ATOMIC_RELAXED);
}

/** \brief Read one word and acquire data from the observed publication. */
static inline uint32_t nx_atomic_u32_load_acquire(const uint32_t* value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

/** \brief Store one word without publishing other data. */
static inline void nx_atomic_u32_store_relaxed(uint32_t* value,
                                               uint32_t desired) {
    __atomic_store_n(value, desired, __ATOMIC_RELAXED);
}

/** \brief Publish preceding data through one aligned release store. */
static inline void nx_atomic_u32_store_release(uint32_t* value,
                                               uint32_t desired) {
    __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

#if defined(__ARM_ARCH_6M__)
/**
 * \brief           Add one word under the reviewed CPU-local IRQ guard.
 * \internal        Fixed-order wrappers share this bounded implementation.
 */
static inline uint32_t nx_atomic_u32_add_guarded(uint32_t* value,
                                                 uint32_t amount) {
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    uint32_t observed = nx_atomic_u32_load_acquire(value);
    nx_atomic_u32_store_release(value, (uint32_t)(observed + amount));
    nx_arch_irq_restore(previous);
    return observed;
}
#endif

/**
 * \brief           Strong acq_rel compare/exchange, acquire on failure.
 * \param[in,out]   expected: Separate, private caller storage; failure replaces
 *                  it with the observed value. Success leaves it unchanged.
 * \return          True only when the original word equals expected and the
 *                  desired word was stored. No spurious failure is allowed.
 */
static inline bool nx_atomic_u32_compare_exchange_acq_rel(uint32_t* value,
                                                          uint32_t* expected,
                                                          uint32_t desired) {
#if defined(__ARM_ARCH_6M__)
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    uint32_t observed = nx_atomic_u32_load_acquire(value);
    bool matched = observed == *expected;
    if (matched) {
        nx_atomic_u32_store_release(value, desired);
    } else {
        *expected = observed;
    }
    nx_arch_irq_restore(previous);
    return matched;
#else
    return __atomic_compare_exchange_n(value, expected, desired, false,
                                       __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

/** \brief Add modulo 2^32 with acq_rel ordering, returning the original word.
 */
static inline uint32_t nx_atomic_u32_fetch_add_acq_rel(uint32_t* value,
                                                       uint32_t amount) {
#if defined(__ARM_ARCH_6M__)
    return nx_atomic_u32_add_guarded(value, amount);
#else
    return __atomic_fetch_add(value, amount, __ATOMIC_ACQ_REL);
#endif
}

/**
 * \brief           Add modulo 2^32 with release ordering; return original word.
 * \note            ARMv6-M conservatively acquires the observed word too.
 */
static inline uint32_t nx_atomic_u32_fetch_add_release(uint32_t* value,
                                                       uint32_t amount) {
#if defined(__ARM_ARCH_6M__)
    return nx_atomic_u32_add_guarded(value, amount);
#else
    return __atomic_fetch_add(value, amount, __ATOMIC_RELEASE);
#endif
}

/**
 * \brief           Subtract modulo 2^32 with acq_rel ordering; return original.
 */
static inline uint32_t nx_atomic_u32_fetch_sub_acq_rel(uint32_t* value,
                                                       uint32_t amount) {
#if defined(__ARM_ARCH_6M__)
    return nx_atomic_u32_add_guarded(value, (uint32_t)(0u - amount));
#else
    return __atomic_fetch_sub(value, amount, __ATOMIC_ACQ_REL);
#endif
}

/**
 * \brief           Subtract modulo 2^32 with release ordering; return original.
 * \note            ARMv6-M conservatively acquires the observed word too.
 */
static inline uint32_t nx_atomic_u32_fetch_sub_release(uint32_t* value,
                                                       uint32_t amount) {
#if defined(__ARM_ARCH_6M__)
    return nx_atomic_u32_add_guarded(value, (uint32_t)(0u - amount));
#else
    return __atomic_fetch_sub(value, amount, __ATOMIC_RELEASE);
#endif
}
#endif
