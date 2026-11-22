/**
 * \file            lsm6dsox.h
 * \brief           Explicit settled LSM6DSOX six-axis and tagged FIFO profile
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LSM6DSOX_H
#define NEXUS_COMPONENTS_LSM6DSOX_H
#include "nexus/core/time.h"
#include "nexus/io/observation.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Settled register callbacks; every return releases all payload spans.
 * \note One serialized task owns device and bus access. SUCCESS transfers all
 *       requested bytes; no callback may retain local arrays. Ports and opaque
 *       context storage outlive calls and any uncertain physical recovery.
 *       No asynchronous fallback or hidden executor is permitted.
 */
typedef struct {
    void* context;
    nx_result_t (*read)(void* context, uint8_t reg, uint8_t* data, size_t bytes,
                        nx_time_us_t deadline);
    nx_result_t (*write)(void* context, uint8_t reg, const uint8_t* data,
                         size_t bytes, nx_time_us_t deadline);
    nx_clock_t clock;
} nx_lsm6dsox_transport_port_t;
/** \brief Fixed 104 Hz, +/-4 g and +/-500 deg/s profile; paths are exclusive.
 */
typedef enum { NX_LSM6DSOX_DIRECT, NX_LSM6DSOX_TAGGED_FIFO } nx_lsm6dsox_mode_t;
/** \brief Caller proves live clock continuity; no automatic epoch detection. */
typedef struct {
    uint32_t clock_domain;
    uint32_t clock_epoch;
    nx_lsm6dsox_mode_t mode;
} nx_lsm6dsox_config_t;
/**
 * \brief Copied six-axis observation in the sensor's native right-hand axes.
 * \note Both data-ready bits were observed before two separate BDU reads.
 *       This does not establish coincident apertures or a lossless stream.
 *       observed is software read completion, with unknown physical error.
 *       Sequence advances only on publication. Generation changes on verified
 *       reset. Neither value is a persistent identity across release/rebind.
 *       Calibration, mounting rotation and sample-age policy stay external.
 */
typedef struct {
    float acceleration_m_s2[3];
    float angular_rate_rad_s[3];
    uint32_t sequence;
    uint32_t generation;
    nx_io_timestamp_t observed;
} nx_lsm6dsox_sample_t;
/** \brief One uncompressed FIFO tag; timestamp entries are not vector pairs. */
typedef enum {
    NX_LSM6DSOX_ACCELERATION,
    NX_LSM6DSOX_ANGULAR_RATE,
    NX_LSM6DSOX_SENSOR_TIMESTAMP
} nx_lsm6dsox_fifo_kind_t;
/**
 * \brief One copied FIFO record with independent tag and publication sequence.
 * \note Vector tags have raw signed counts and SI values (m/s^2 or rad/s).
 *       Timestamp tags have only sensor_ticks: native wrapping 32-bit ticks,
 *       nominally 25 us each. No wrap extension, pairing or synchronization to
 *       observed is implied. FIFO overflow closes publication until explicit
 *       stop/reset/start; unsupported tags also require resynchronization.
 */
typedef struct {
    nx_lsm6dsox_fifo_kind_t kind;
    int16_t raw[3];
    float values[3];
    uint32_t sensor_ticks;
    uint32_t sequence;
    uint32_t generation;
    nx_io_timestamp_t observed;
} nx_lsm6dsox_fifo_sample_t;
/**
 * \brief Caller-owned zeroed instance; fields are private implementation state.
 * \note Task-only, no concurrent/reentrant calls. Uncertain state retains the
 *       binding and recovery duty, but settled ports retain no MCU payload
 *       loan. A time fault cannot be repaired by a numerically later deadline.
 */
typedef struct {
    nx_lsm6dsox_transport_port_t transport;
    nx_lsm6dsox_config_t config;
    nx_time_us_t operation_deadline;
    nx_time_us_t reset_deadline;
    nx_time_us_t last_clock;
    uint32_t sequence;
    uint32_t generation;
    nx_result_t error;
    bool initialized;
    bool resetting;
    bool prepared;
    bool running;
    bool uncertain;
    bool in_call;
    bool time_fault;
} nx_lsm6dsox_t;
/**
 * \brief           Validate identity 6Ch and bind without configuration writes.
 * \param[in,out]   device: Fresh zeroed, exclusive caller storage.
 * \param[in]       transport: Settled callbacks, copied only on success.
 * \param[in]       config: Separate immutable explicit clock/mode facts.
 * \param[in]       deadline: Finite absolute whole-operation deadline.
 * \return          SUCCESS, INVALID, BUSY, UNSUPPORTED or transport error.
 * \note            Caller supplies power-up readiness and exclusive device
 *                  ownership. Unknown identity retains no binding. No probing,
 *                  reset, calibration, sleep or hidden task.
 */
nx_result_t nx_lsm6dsox_init(nx_lsm6dsox_t* device,
                             nx_lsm6dsox_transport_port_t transport,
                             const nx_lsm6dsox_config_t* config,
                             nx_time_us_t deadline);
/**
 * \brief           Explicitly request destructive volatile software reset.
 * \param[in,out]   device: Initialized stopped instance, same task owner.
 * \param[in]       deadline: Fixed finite budget including later reset polls.
 * \return          SUCCESS starts reset; failure can retain uncertainty.
 * \note            Not reset completion or analog readiness. Reset discards
 *                  prior FIFO and configuration. No automatic reset retry.
 */
nx_result_t nx_lsm6dsox_reset_begin(nx_lsm6dsox_t* device,
                                    nx_time_us_t deadline);
/**
 * \brief           Observe the pending reset under its original deadline.
 * \param[in,out]   device: Same instance and serialized task executor.
 * \return          SUCCESS after reset clears, BUSY, or retained error.
 * \note            Completion advances generation and clears sequence. No
 *                  wait or renewed deadline; caller explicitly schedules polls.
 */
nx_result_t nx_lsm6dsox_reset_poll(nx_lsm6dsox_t* device);
/**
 * \brief           Configure and verify the exact selected measurement profile.
 * \param[in,out]   device: Verified reset-complete instance.
 * \param[in]       deadline: Finite budget shared by configuration/readback.
 * \return          SUCCESS or error retaining physical cleanup responsibility.
 * \note            Enables BDU/autoincrement and selected 104 Hz outputs. FIFO
 *                  selects uncompressed stream plus timestamp entries. Success
 *                  is configuration, not settled analog accuracy. The product
 *                  observes startup/thermal/electrical qualification
 * separately.
 */
nx_result_t nx_lsm6dsox_start(nx_lsm6dsox_t* device, nx_time_us_t deadline);
/**
 * \brief           Copy one direct observation when both outputs are ready.
 * \param[in,out]   device: Running DIRECT profile on the same task owner.
 * \param[out]      sample: Separate aligned output, unchanged on any error.
 * \param[in]       deadline: One finite budget across both reads.
 * \return          SUCCESS, BUSY, STATE, EXHAUSTED or transport error.
 * \note            Transport failure closes publication; explicit stop/reset
 *                  is required. No cached zero or automatic replay is returned.
 */
nx_result_t nx_lsm6dsox_read(nx_lsm6dsox_t* device,
                             nx_lsm6dsox_sample_t* sample,
                             nx_time_us_t deadline);
/**
 * \brief           Pop one selected FIFO record with overflow observation.
 * \param[in,out]   device: Running TAGGED_FIFO profile, same executor.
 * \param[out]      sample: Separate aligned output, unchanged on failure.
 * \param[in]       deadline: Finite whole-operation budget.
 * \return          SUCCESS, EMPTY, OVERFLOW, UNSUPPORTED or retained error.
 * \note            Output bytes are copied, never borrowed. Sensor overrun or
 *                  a failure after a FIFO pop can lose records; both close
 *                  publication. Consumer owns pairing and rate/age assumptions.
 */
nx_result_t nx_lsm6dsox_fifo_pop(nx_lsm6dsox_t* device,
                                 nx_lsm6dsox_fifo_sample_t* sample,
                                 nx_time_us_t deadline);
/**
 * \brief           Disable both ODRs, discard FIFO and verify withdrawal.
 * \param[in,out]   device: Initialized or uncertain same binding.
 * \param[in]       deadline: Independent finite cleanup budget.
 * \return          SUCCESS only after OFF/BYPASS readback; error keeps duty.
 * \note            No bus shutdown or power-off. In-progress reset returns
 *                  BUSY. Restart requires a new explicit verified reset.
 */
nx_result_t nx_lsm6dsox_stop(nx_lsm6dsox_t* device, nx_time_us_t deadline);
/**
 * \brief           Drop software binding after every caller has joined.
 * \param[in,out]   device: Quiescent caller-owned instance.
 * \note            No I/O and no action during callback reentrancy. This never
 *                  proves physical stop: complete stop or externally recover
 *                  the device first, or retain that external responsibility.
 *                  Rebinding requires a new external session identity.
 */
void nx_lsm6dsox_release(nx_lsm6dsox_t* device);
#ifdef __cplusplus
}
#endif
#endif
