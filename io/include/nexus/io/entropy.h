/**
 * \file            entropy.h
 * \brief           Explicit entropy source and finite all-or-erased filling
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_ENTROPY_H
#define NEXUS_IO_ENTROPY_H

#include "nexus/core/time.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Shared immutable entropy-source methods
 * \note            try_read is task-only and bounded, never waits for hardware.
 *                  SUCCESS copies one complete health-admitted 32-bit word;
 *                  BUSY means no word yet, IO means a source/health failure,
 *                  STATE means unavailable/closed. Failure leaves output
 *                  unchanged. The provider owns hardware health and initial
 *                  sample handling. No deterministic fallback, conditioning,
 *                  DRBG or cryptographic certification is implied.
 */
typedef struct {
    nx_result_t (*try_read)(void* context, uint32_t* word);
} nx_entropy_ops_t;

/**
 * \brief           Static two-pointer source face with private mutable state
 * \note            Caller serializes use and lifecycle; face/table/state
 * outlive each call. No buffer or request is retained after return. Source
 * enable, clock dependencies, recovery and stop belong to its explicit
 * lifecycle owner, not to TLS or this face.
 */
typedef struct nx_entropy_port {
    const nx_entropy_ops_t* ops;
    void* context;
} nx_entropy_port_t;

/**
 * \brief           Try to copy one complete source word without waiting
 * \param[in]       port: Live immutable face on its serialized task executor
 * \param[out]      word: Naturally aligned writable word, disjoint from face,
 *                  table and the complete private provider state
 * \return          SUCCESS, BUSY, IO, STATE, INVALID or UNSUPPORTED
 * \note            Invalid public spans reject before provider access. This
 *                  wrapper cannot infer the extent of an opaque context; the
 *                  caller proves its complete disjointness. Provider rejection
 *                  never substitutes fixed values or timestamps for entropy.
 */
nx_result_t nx_entropy_port_try_read(const nx_entropy_port_t* port,
                                     uint32_t* word);

/**
 * \brief           Fill exactly the requested bytes before a finite deadline
 * \param[in]       port: Live serialized source; no concurrent lifecycle change
 * \param[out]      output: Writable byte span; NULL only for zero bytes
 * \param[in]       bytes: Exact required output size, not an entropy estimate
 * \param[in]       clock: Live monotonic microsecond clock in one stable domain
 * \param[in]       deadline: Finite absolute deadline in that clock domain
 * \return          SUCCESS only for a complete fill; TIMEOUT, IO, STATE or
 *                  source error; INVALID/UNSUPPORTED for preflight rejection
 * \note            Task-only synchronous polling; no heap, worker or OS wait.
 *                  BUSY causes another clock-bounded poll. The supplied clock
 *                  must continue advancing throughout the call; changing its
 *                  domain or stopping its timer violates the contract.
 *                  NX_DEADLINE_NEVER is rejected. Complete words are emitted
 *                  least-significant byte first; unused final-word bytes are
 *                  discarded. A zero-byte request validates face and clock but
 *                  performs no clock/source access and accepts NULL output.
 *                  Preflight rejection leaves output unchanged. Once admitted,
 *                  any error erases the complete output using volatile writes;
 *                  no partial entropy may be consumed. Output is disjoint from
 *                  face/table/clock and all their opaque live contexts. This
 *                  function detects known-span aliases, not opaque extents.
 *                  Crypto libraries own conditioning, DRBGs and trust policy.
 */
nx_result_t nx_entropy_fill(const nx_entropy_port_t* port, void* output,
                            size_t bytes, const nx_clock_t* clock,
                            nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_IO_ENTROPY_H */
