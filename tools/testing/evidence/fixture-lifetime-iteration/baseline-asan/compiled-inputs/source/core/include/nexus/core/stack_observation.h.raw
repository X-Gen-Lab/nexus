/**
 * \file            stack_observation.h
 * \brief           Bounded read-only observation of caller-painted stack spans
 * \author          Nexus Team
 */
#ifndef NEXUS_CORE_STACK_OBSERVATION_H
#define NEXUS_CORE_STACK_OBSERVATION_H

#include "nexus/core/status.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Byte-prefix paint observations, not an exact physical stack peak. */
typedef struct {
    size_t capacity_bytes;
    size_t untouched_bytes;
    size_t touched_bytes;
} nx_stack_observation_t;

/**
 * \brief           Read a bounded low-address paint prefix without mutation.
 * \param[in]       base: Live downward-growing stack region, caller-provenance.
 * \param[in]       bytes: Original positive painted region capacity in bytes.
 * \param[in]       paint: Exact fill used before the region became active.
 * \param[out]      out: Distinct output, unchanged on invalid span or alias.
 * \return          SUCCESS or INVALID, without allocation or retained pointers.
 * \note            Caller ensures this region is quiescent and painting had no
 *                  live frames. No Arch/OS dependency or internal mask occurs.
 *                  Work is at most bytes reads. Unwritten reservations or
 * writes equal to paint can hide usage; this is not overflow detection. MSP
 * qualification needs independently established boot paint and a quiescent
 * sampling point. The span excludes current SP, all active frames and
 * exception-stack reservation. It never paints or claims to read the physical
 * MSP register.
 */
nx_result_t nx_stack_observe(const void* base, size_t bytes, uint8_t paint,
                             nx_stack_observation_t* out);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_CORE_STACK_OBSERVATION_H */
