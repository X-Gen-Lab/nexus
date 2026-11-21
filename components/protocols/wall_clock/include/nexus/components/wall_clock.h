/**
 * \file            wall_clock.h
 * \brief           Explicit UTC quality independent of monotonic deadlines
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_WALL_CLOCK_H
#define NEXUS_COMPONENTS_WALL_CLOCK_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Time quality; plain SNTP is a measurement, never authentication. */
typedef enum {
    NX_UTC_UNKNOWN,
    NX_UTC_MEASURED,
    NX_UTC_TRUSTED
} nx_utc_quality_t;
/** \brief Caller-owned immutable observation returned as a value. */
typedef struct {
    int64_t unix_us;
    uint64_t uncertainty_us;
    nx_time_us_t observed_at;
    nx_utc_quality_t quality;
} nx_utc_sample_t;
/** \brief Serialized affine bridge; initialize to zero, treat fields private.
 */
typedef struct {
    nx_utc_sample_t base;
    nx_time_us_t valid_until;
    uint32_t drift_ppm;
    bool valid;
} nx_wall_clock_t;
/**
 * \brief           Admit an explicit UTC measurement and expiry.
 * \param[in,out]   clock: Caller-owned state in one live monotonic domain.
 * \param[in]       sample: UTC/uncertainty/quality and finite observation time.
 * \param[in]       valid_until: Finite absolute expiry after observation.
 * \param[in]       drift_ppm: Upper oscillator drift bound, at most 1000000.
 * \return          SUCCESS or INVALID; unchanged on failure.
 * \note            Task-only serialized operation; no I/O, heap or clock read.
 *                  Product determines trust. UTC updates never modify
 * deadlines.
 */
nx_result_t nx_wall_clock_set(nx_wall_clock_t* clock,
                              const nx_utc_sample_t* sample,
                              nx_time_us_t valid_until, uint32_t drift_ppm);
/**
 * \brief           Project UTC and bounded uncertainty at a monotonic snapshot.
 * \param[in]       clock: Stable bridge, same live monotonic domain.
 * \param[in]       now: Finite monotonic time, not before observation.
 * \param[out]      sample: Distinct value output, unchanged on failure.
 * \return          SUCCESS, STATE for unknown/rollback, TIMEOUT at expiry,
 *                  or OVERFLOW; no hidden refresh or trust upgrade.
 * \note            Task-only, caller serializes updates. Trusted TLS consumers
 *                  explicitly require quality TRUSTED; MEASURED is
 * insufficient.
 */
nx_result_t nx_wall_clock_read(const nx_wall_clock_t* clock, nx_time_us_t now,
                               nx_utc_sample_t* sample);
/**
 * \brief           Validate one server reply and derive an SNTP observation.
 * \param[in]       packet: Exactly 48 bytes, not retained.
 * \param[in]       bytes: Must be 48 (extension/auth formats are unselected).
 * \param[in]       request_token: Eight unpredictable transmitted timestamp
 * bytes. \param[in]       sent_at: Finite request send time in monotonic
 * domain. \param[in]       received_at: Same-domain receive snapshot, not
 * before send. \param[in]       era: Explicit NTP era, zero or one; no 2036
 * guess. \param[out]      sample: Distinct output, unchanged on failure.
 * \return          SUCCESS, INVALID for bad mode/stratum/leap/originate, or
 *                  OVERFLOW; output quality always MEASURED.
 * \note            Task-only bounded pure parsing. Caller verifies source IP,
 *                  port and outstanding request identity. SNTP has no peer
 *                  authentication; request matching does not establish trust.
 */
nx_result_t nx_sntp_parse(const uint8_t* packet, size_t bytes,
                          const uint8_t request_token[8], nx_time_us_t sent_at,
                          nx_time_us_t received_at, uint8_t era,
                          nx_utc_sample_t* sample);
#ifdef __cplusplus
}
#endif
#endif
