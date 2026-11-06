/**
 * \file            stream_profile_storage.h
 * \brief           Private cold stream budget and caller storage validation
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_STREAM_PROFILE_STORAGE_H
#define NEXUS_PRIVATE_STREAM_PROFILE_STORAGE_H

#include "nexus/io/stream_budget.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Check the reviewed double-target budget with one free replacement.
 * \note The profile entry supplies a nonnull readonly budget.
 */
nx_result_t nx_stream_profile_budget_validate(const nx_stream_budget_t* budget);

/** \brief Validate a joined caller pool before its selected provider borrows.
 * \note The profile entry validates its budget first and joins all pool users.
 *       Cold checks retain nothing, change no loans and never access hardware.
 */
nx_result_t nx_stream_profile_pool_validate(const nx_stream_budget_t* budget,
                                            const void* profile,
                                            size_t profile_bytes,
                                            const void* port, size_t port_bytes,
                                            const nx_stream_t* stream);

#ifdef __cplusplus
}
#endif

#endif
