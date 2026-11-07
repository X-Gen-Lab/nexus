/**
 * \file            infrared.h
 * \brief           Bounded NEC decoding from copied active-low EXTI edges
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_INFRARED_H
#define NEXUS_COMPONENTS_INFRARED_H

#include "nexus/io/exti.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Explicit address policy; no inverse-error fallback is permitted. */
typedef enum {
    NX_INFRARED_NEC_STANDARD,
    NX_INFRARED_NEC_EXTENDED
} nx_infrared_nec_protocol_t;

/** \brief Strict by default; IRQ sampling is an explicit software reference. */
typedef enum {
    NX_INFRARED_SOURCE_STRICT,
    NX_INFRARED_SOURCE_IRQ_OBSERVATION
} nx_infrared_source_policy_t;

/** \brief Source declarations/flags never attest physical capture integrity. */
typedef enum {
    NX_INFRARED_QUALITY_FLAG_FREE_SOURCE,
    NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN
} nx_infrared_quality_t;

/** \brief Inclusive authored microsecond limits, not measured tolerances. */
typedef struct {
    uint16_t minimum_us;
    uint16_t maximum_us;
} nx_infrared_window_t;

/**
 * \brief Immutable complete waveform policy, retained through stop.
 * \note All windows are nonzero and at most 20000 us. Zero/one and full/repeat
 *       space windows cannot overlap. frame_max_us admits the complete worst
 *       permitted 32-bit frame plus trailer, at most 1000000 us. Repeat age is
 *       bounded by repeat_max_us, at most 5000000 us since the last valid full
 *       frame or repeat. Input is already demodulated active-low, both edges,
 *       in one stable monotonic microsecond epoch. Carrier, receiver fitting,
 *       measured accuracy, remote model and product key mapping are external.
 *       source_policy defaults to STRICT (zero). IRQ_OBSERVATION explicitly
 *       permits COALESCED observations as a software reference; every result
 *       remains SAMPLED_UNCERTAIN even when particular inputs are flag-free.
 */
typedef struct {
    nx_infrared_window_t leader_mark;
    nx_infrared_window_t leader_space;
    nx_infrared_window_t repeat_space;
    nx_infrared_window_t bit_mark;
    nx_infrared_window_t zero_space;
    nx_infrared_window_t one_space;
    uint32_t frame_max_us;
    uint32_t repeat_max_us;
    uint8_t line;
    nx_infrared_nec_protocol_t protocol;
    nx_infrared_source_policy_t source_policy;
} nx_infrared_nec_config_t;

/**
 * \brief Checked command; timestamp is its final edge or IRQ observation.
 * \note quality and preserved source_flags describe source uncertainty, not
 *       physical capture proof. Repeat provenance includes its predecessor.
 *       NEC inverse checks cannot establish that an IRQ source lost no edges.
 */
typedef struct {
    nx_time_us_t timestamp_us;
    uint16_t address;
    uint8_t command;
    bool repeat;
    nx_infrared_quality_t quality;
    uint32_t source_flags;
} nx_infrared_nec_frame_t;

/** \brief Value result; frame is meaningful only with SUCCESS. */
typedef struct {
    nx_result_t result;
    nx_infrared_nec_frame_t frame;
} nx_infrared_nec_result_t;

/**
 * \brief Caller-owned exact decoder storage, zero before initialization.
 * \note One serialized task executor. No edge/request/output survives return.
 *       Config and any EXTI face/ops/opaque context remain live and disjoint
 *       from this owner. Fields are implementation state, not caller controls.
 */
typedef struct {
    const nx_infrared_nec_config_t* config;
    nx_time_us_t last_timestamp;
    nx_time_us_t frame_started;
    nx_time_us_t valid_at;
    uint32_t word;
    uint16_t address;
    uint8_t command;
    uint8_t stage;
    uint8_t bit_count;
    uint8_t source_flags;
    uint8_t valid_flags;
    bool initialized;
    bool in_call;
    bool timestamp_seen;
    bool last_valid;
    bool clock_fault;
} nx_infrared_nec_t;

/**
 * \brief Bind explicit timing/address policy without hardware access.
 * \param[in,out] owner: Fresh zero or successfully stopped, aligned storage.
 * \param[in] config: Stable readonly policy, disjoint from owner.
 * \return SUCCESS, INVALID or BUSY; rejection leaves both spans unchanged.
 */
nx_result_t nx_infrared_nec_init(nx_infrared_nec_t* owner,
                                 const nx_infrared_nec_config_t* config);

/**
 * \brief Consume one copied edge through the deterministic NEC state machine.
 * \param[in,out] owner: Initialized single consumer in its original epoch.
 * \param[in] event: Disjoint copied public edge, borrowed only during call.
 * \return Value SUCCESS only after 32 valid bits and trailer or valid repeat;
 *         EMPTY awaits more edges; OVERFLOW rejects LOSS in every policy and
 *         COALESCED in STRICT. IRQ_OBSERVATION retains sampled uncertainty.
 *         IO rejects malformed waveform/clock, TIMEOUT rejects a stale frame.
 * \note Wrong line, unknown flags/edge and invalid spans reject unchanged.
 *       LOSS invalidates frame and repeat eligibility; its report timestamp
 *       is not a recovered edge and never advances the source clock history.
 *       Accepted COALESCED samples retain their observation time and flags.
 *       Equal/backward accepted timestamps latch clock_fault until stop/init.
 *       Malformed/truncated frames never publish partial commands. No RC5,
 *       NEC2 interpretation, automatic protocol discovery or calibration.
 */
nx_infrared_nec_result_t nx_infrared_nec_feed(nx_infrared_nec_t* owner,
                                              const nx_exti_event_t* event);

/**
 * \brief Read at most one edge from a caller-owned public EXTI queue.
 * \param[in,out] owner: Same decoder; callback reentry returns BUSY.
 * \param[in] port: Stable exact both-edge source in the config clock epoch.
 * \return Same feed result, EMPTY or exact port error; no callback is retained.
 * \note Caller exclusively drains one physical source. This method does not
 *       initialize, stop, attach a wake, recover or otherwise own that source.
 *       A failed/inconsistent read invalidates frame and repeat history.
 */
nx_infrared_nec_result_t nx_infrared_nec_poll(nx_infrared_nec_t* owner,
                                              const nx_exti_port_t* port);

/**
 * \brief Drop software decode history after the serialized caller joins.
 * \param[in,out] owner: Stable storage with no active feed/poll callback.
 * \return SUCCESS or INVALID/BUSY. No GPIO, EXTI stop or hardware access.
 * \note Drain/rebind the external source before a new clock epoch is supplied
 *       to a reinitialized decoder. Stop does not establish hardware
 * quiescence.
 */
nx_result_t nx_infrared_nec_stop(nx_infrared_nec_t* owner);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_INFRARED_H */
