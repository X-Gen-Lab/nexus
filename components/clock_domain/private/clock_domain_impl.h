/**
 * \file            clock_domain_impl.h
 * \brief           Caller-owned clock admission state and bounded borrow scan
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_CLOCK_DOMAIN_IMPL_H
#define NEXUS_CLOCK_DOMAIN_IMPL_H
#include "nexus/components/clock_domain.h"
/** \brief Private storage; lifetime never resets its monotonically used epoch.
 */
struct nx_clock_domain {
    nx_clock_lease_t* head;
    uint64_t epoch;
    size_t count;
    size_t capacity;
    bool initialized;
    bool closing;
};
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Construct unused zero-initialized private domain storage
 * \note            Serialized privileged Thread. Positive exact capacity;
 *                  no allocation, hardware writes, global registry or worker.
 *                  Never overwrite a live domain to recycle its epoch.
 */
nx_result_t nx_clock_domain_construct(nx_clock_domain_t* domain,
                                      size_t capacity);
/**
 * \brief           Check a span against the domain and every retained token
 * \note            Internal sleep/output admission, called with Arch guard.
 *                  Does not validate an opaque application callback context.
 */
bool nx_clock_domain_span_valid(const nx_clock_domain_t* domain,
                                const void* span, size_t bytes);
/**
 * \brief           Acquire the one static Platform lifecycle consumer
 * \note            Serialized privileged Thread, saved incoming mask. Construct
 *                  unused zero storage with capacity one; no hardware effects.
 *                  A failed hardware drain must never call cold_release.
 */
nx_result_t nx_clock_domain_cold_acquire(nx_clock_domain_t* domain,
                                         nx_clock_lease_t* lease);
/** \brief Reopen a closed drained epoch, then acquire before cold effects. */
nx_result_t nx_clock_domain_cold_start(nx_clock_domain_t* domain,
                                       nx_clock_lease_t* lease);
/** \brief Close admission without releasing the held hardware responsibility.
 */
nx_result_t nx_clock_domain_cold_close(nx_clock_domain_t* domain);
/**
 * \brief           Release only after actual hardware and publisher drain
 * \note            Unused storage is a no-op. Foreign/copied tokens remain
 *                  retained on failure. Final close/quiescence errors survive.
 */
nx_result_t nx_clock_domain_cold_release(nx_clock_domain_t* domain,
                                         nx_clock_lease_t* lease);
/** \brief Close and reopen a genuinely drained, nonexhausted epoch. */
nx_result_t nx_clock_domain_cold_reopen(nx_clock_domain_t* domain);
/** \brief Read-only resume admission before all endpoint/resource effects. */
nx_result_t nx_clock_domain_cold_resume_admit(const nx_clock_lease_t* lease);
/**
 * \brief           Observe whether the trusted static token has an owner
 * \note            Called under the exclusive Platform lifecycle owner. This
 *                  is metadata routing, never evidence of hardware drain.
 */
bool nx_clock_domain_cold_has_lease(const nx_clock_lease_t* lease);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_CLOCK_DOMAIN_IMPL_H */
