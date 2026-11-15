/**
 * \file            clock_domain_owner.h
 * \brief           Exact static storage for a trusted clock domain owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_CLOCK_DOMAIN_OWNER_H
#define NEXUS_COMPONENTS_CLOCK_DOMAIN_OWNER_H
#include "nexus/components/clock_domain.h"
/**
 * \brief           Trusted static owner storage; fields are not controls
 * \note            Zero-initialize once. Storage outlives every consumer token
 *                  and copied barrier. Do not copy, overwrite or reclaim a live
 *                  domain, or recycle its monotonically used epoch. Ordinary
 *                  consumers use the opaque type in clock_domain.h instead.
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
 * \brief           Construct unused zero-initialized static owner storage
 * \return          SUCCESS or INVALID/STATE/CONTEXT without retained consumers
 * \note            Serialized privileged Thread. Positive exact capacity;
 *                  no allocation, hardware writes, global registry or worker.
 *                  Never overwrite a live domain to recycle its epoch. Caller
 *                  storage outlives every token and copied barrier that can
 *                  still be observed; construct does not stop hardware.
 */
nx_result_t nx_clock_domain_construct(nx_clock_domain_t* domain,
                                      size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_CLOCK_DOMAIN_OWNER_H */
