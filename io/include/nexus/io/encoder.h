/**
 * \file            encoder.h
 * \brief           Optional raw modulo quadrature encoder observations
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_ENCODER_H
#define NEXUS_IO_ENCODER_H
#include "nexus/core/time.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Logical A/B phase counting, never a freely combinable mask.
 * \note A and B follow the statically bound input order. x2 counts both
 *       edges of the selected phase; x4 counts both edges of both phases.
 */
typedef enum {
    NX_ENCODER_MODE_UNKNOWN = 0,
    NX_ENCODER_MODE_X2_A,
    NX_ENCODER_MODE_X2_B,
    NX_ENCODER_MODE_X4
} nx_encoder_mode_t;
/** \brief One raw modulo counter and separate software observation time. */
typedef struct {
    nx_time_us_t observed_at;
    uint32_t count;
} nx_encoder_sample_t;
/** \brief Input qualification in the reported filter clock domain.
 * \note A zero divisor and sample count mean bypass. Otherwise the filter
 *       samples at filter_clock_hz / clock_divisor and requires the declared
 *       consecutive equal samples. This describes the selected digital filter,
 *       not an exact debounce time, physical edge bandwidth or sensor quality.
 */
typedef struct {
    uint8_t clock_divisor;
    uint8_t consecutive_samples;
} nx_encoder_filter_t;
/** \brief Immutable input/filter facts and actual counter enable state. */
typedef struct {
    uint32_t filter_clock_hz;
    nx_encoder_mode_t mode;
    uint8_t counter_bits;
    nx_encoder_filter_t filter_a;
    nx_encoder_filter_t filter_b;
    bool running;
} nx_encoder_state_t;
/** \brief Shared methods; no ring, request, allocation, callback or worker. */
typedef struct {
    nx_result_t (*start)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*read)(void* context, nx_encoder_sample_t* sample);
    nx_result_t (*state)(const void* context, nx_encoder_state_t* state);
} nx_encoder_ops_t;
/** \brief Immutable identity; exact private state owns the whole controller. */
struct nx_encoder {
    const nx_encoder_ops_t* ops;
    void* context;
};
/**
 * \brief Enable only the two cold-selected encoder inputs and counter.
 * \param[in] port: One serialized privileged Thread-mode owner.
 * \return SUCCESS or error; a failed write keeps recovery responsibility.
 * \note The provider never changes pins, rate, filtering or mode at runtime.
 *       Clock, pin and vector leases belong to cold assembly through close.
 */
nx_result_t nx_encoder_start(const nx_encoder_t* port);
/**
 * \brief Observe counter and input-source withdrawal before returning.
 * \param[in] port: The same serialized privileged Thread-mode owner.
 * \return SUCCESS proves source withdrawal; failure retains the session.
 * \note Stop preserves the raw position. It does not release cold resources.
 */
nx_result_t nx_encoder_stop(const nx_encoder_t* port);
/**
 * \brief Read one modulo position without inferring travel across wraps.
 * \param[in] port: Serialized privileged Thread-mode owner.
 * \param[out] sample: Aligned separate output, unchanged on rejection.
 * \return SUCCESS or INVALID/STATE/CONTEXT/UNSUPPORTED.
 * \note observed_at is software observation time, not a sensor edge aperture.
 *       Direction, speed, mechanical zero and unbounded signed position need
 *       external sampling bounds and product policy. A read cannot reconstruct
 *       missed full wraps. No separately read DIR bit is an atomic edge
 * history.
 */
nx_result_t nx_encoder_read(const nx_encoder_t* port,
                            nx_encoder_sample_t* sample);
/**
 * \brief Report counter width and semantic input qualification facts.
 * \param[in] port: Serialized privileged Thread-mode owner.
 * \param[out] state: Aligned separate output, unchanged on rejection.
 * \return SUCCESS or INVALID/STATE/CONTEXT/UNSUPPORTED.
 * \note Declared sampling and their kernel clock do not qualify noise
 * rejection, physical edge rate or electrical sensor/Board behavior.
 */
nx_result_t nx_encoder_state(const nx_encoder_t* port,
                             nx_encoder_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
