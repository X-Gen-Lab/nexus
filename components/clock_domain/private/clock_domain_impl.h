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
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_CLOCK_DOMAIN_IMPL_H */
