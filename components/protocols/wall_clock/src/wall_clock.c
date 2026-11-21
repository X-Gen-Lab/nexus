/**
 * \file            wall_clock.c
 * \brief           UTC projection and explicitly unauthenticated SNTP parsing
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/wall_clock.h"
#include <limits.h>
#include <string.h>

/** \brief Reject aliased outputs and address arithmetic overflow. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (an != 0 && bn != 0 && x < y + bn && y < x + an);
}
/** \brief Admit one exact UTC provenance and validity interval. */
nx_result_t nx_wall_clock_set(nx_wall_clock_t* clock,
                              const nx_utc_sample_t* sample,
                              nx_time_us_t valid_until, uint32_t drift_ppm) {
    if (clock == NULL || sample == NULL ||
        overlap(clock, sizeof(*clock), sample, sizeof(*sample)) ||
        sample->observed_at == NX_DEADLINE_NEVER ||
        valid_until == NX_DEADLINE_NEVER ||
        valid_until <= sample->observed_at || drift_ppm > 1000000 ||
        (sample->quality != NX_UTC_MEASURED &&
         sample->quality != NX_UTC_TRUSTED)) {
        return NX_ERROR_INVALID;
    }
    *clock = (nx_wall_clock_t){*sample, valid_until, drift_ppm, true};
    return NX_SUCCESS;
}
/** \brief Project UTC without ever changing the underlying monotonic domain. */
nx_result_t nx_wall_clock_read(const nx_wall_clock_t* clock, nx_time_us_t now,
                               nx_utc_sample_t* sample) {
    if (clock == NULL || sample == NULL || now == NX_DEADLINE_NEVER ||
        overlap(clock, sizeof(*clock), sample, sizeof(*sample))) {
        return NX_ERROR_INVALID;
    }
    if (!clock->valid || now < clock->base.observed_at) {
        return NX_ERROR_STATE;
    }
    if (now >= clock->valid_until) {
        return NX_ERROR_TIMEOUT;
    }
    uint64_t elapsed = now - clock->base.observed_at;
    if (elapsed > INT64_MAX ||
        clock->base.unix_us > INT64_MAX - (int64_t)elapsed) {
        return NX_ERROR_OVERFLOW;
    }
    uint64_t whole = elapsed / UINT64_C(1000000);
    if (clock->drift_ppm != 0 && whole > UINT64_MAX / clock->drift_ppm) {
        return NX_ERROR_OVERFLOW;
    }
    uint64_t fraction =
        ((elapsed % UINT64_C(1000000)) * clock->drift_ppm + 999999) /
        UINT64_C(1000000);
    uint64_t growth = whole * clock->drift_ppm;
    if (fraction > UINT64_MAX - growth ||
        clock->base.uncertainty_us > UINT64_MAX - growth - fraction) {
        return NX_ERROR_OVERFLOW;
    }
    *sample = clock->base;
    sample->unix_us += (int64_t)elapsed;
    sample->uncertainty_us += growth + fraction;
    sample->observed_at = now;
    return NX_SUCCESS;
}
/** \brief Read wire fields without structure alignment or host endian. */
static uint32_t get32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
/** \brief Validate reply identity and retain its untrusted measurement quality.
 */
nx_result_t nx_sntp_parse(const uint8_t* packet, size_t bytes,
                          const uint8_t request_token[8], nx_time_us_t sent_at,
                          nx_time_us_t received_at, uint8_t era,
                          nx_utc_sample_t* sample) {
    if (packet == NULL || bytes != 48 || request_token == NULL ||
        sample == NULL || sent_at == NX_DEADLINE_NEVER ||
        received_at == NX_DEADLINE_NEVER || received_at < sent_at || era > 1 ||
        overlap(packet, bytes, sample, sizeof(*sample)) ||
        overlap(request_token, 8, sample, sizeof(*sample))) {
        return NX_ERROR_INVALID;
    }
    unsigned version = (packet[0] >> 3) & 7U;
    if ((packet[0] & 7U) != 4 || (version != 3 && version != 4) ||
        (packet[0] >> 6) == 3 || packet[1] == 0 || packet[1] > 15 ||
        memcmp(packet + 24, request_token, 8) != 0 ||
        (get32(packet + 40) == 0 && get32(packet + 44) == 0)) {
        return NX_ERROR_INVALID;
    }
    int64_t seconds = (int64_t)get32(packet + 40) +
                      (int64_t)era * INT64_C(4294967296) - INT64_C(2208988800);
    int64_t utc = seconds * INT64_C(1000000) +
                  (int64_t)(((uint64_t)get32(packet + 44) * 1000000) >> 32);
    uint64_t round_trip = received_at - sent_at;
    uint64_t half = round_trip / 2;
    if (half > INT64_MAX || utc > INT64_MAX - (int64_t)half) {
        return NX_ERROR_OVERFLOW;
    }
    uint64_t dispersion =
        ((uint64_t)get32(packet + 8) * UINT64_C(1000000) + 65535) / 65536;
    uint32_t root_delay = get32(packet + 4);
    uint64_t delay = (root_delay & UINT32_C(0x80000000)) != 0
                         ? 0
                         : ((uint64_t)root_delay * 1000000 + 131071) / 131072;
    if (half > UINT64_MAX - dispersion - delay - 1) {
        return NX_ERROR_OVERFLOW;
    }
    *sample =
        (nx_utc_sample_t){utc + (int64_t)half, half + dispersion + delay + 1,
                          received_at, NX_UTC_MEASURED};
    return NX_SUCCESS;
}
