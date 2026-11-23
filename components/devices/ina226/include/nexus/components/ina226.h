/**
 * \file            ina226.h
 * \brief           Exact INA226 triggered monitoring with explicit calibration
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_INA226_H
#define NEXUS_COMPONENTS_INA226_H
#include "nexus/io/i2c.h"
#include "nexus/io/observation.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Explicit hardware scale and selected single-shot conversion profile.
 * \note I2C is a settled polling endpoint for the actual 40h..4Fh wiring.
 *       One task serializes this device, bus and all register accesses. The
 *       component owns conversion/configuration/calibration, with no external
 *       ALERT owner. Averaging and both conversion-time codes are 0..7 as
 *       defined by INA226. Domain/epoch describe the same live microsecond
 *       clock as I2C deadlines. Shunt/current units determine a truncated
 *       15-bit calibration; the product owns tolerance and physical limits.
 *       This low-rate monitor is not a motor phase-current sampler.
 */
typedef struct {
    const nx_i2c_endpoint_t* i2c;
    nx_clock_t clock;
    uint32_t clock_domain;
    uint32_t clock_epoch;
    uint32_t shunt_microohms;
    uint32_t current_lsb_nanoamps;
    uint8_t averaging;
    uint8_t bus_conversion;
    uint8_t shunt_conversion;
} nx_ina226_config_t;
/**
 * \brief Complete copied one-shot results, published only on success.
 * \note Current is signed; power is the unsigned device power result. Bus
 *       and shunt conversions are sequential, not simultaneous. observed is
 *       software read completion, with unknown physical/aperture uncertainty.
 *       The complete conversion was triggered no earlier than started_at.
 *       Integer scaling uses the requested current LSB; calibration truncation,
 *       shunt tolerance and analog error remain explicit product error terms.
 */
typedef struct {
    int32_t shunt_nanovolts;
    uint32_t bus_microvolts;
    int64_t current_nanoamps;
    uint64_t power_nanowatts;
    nx_time_us_t started_at;
    nx_io_timestamp_t observed;
} nx_ina226_sample_t;
/** \brief Fresh zero caller state; bindings remain live through recovery. */
typedef struct {
    const nx_ina226_config_t* config;
    nx_time_us_t deadline;
    nx_time_us_t last_clock;
    nx_time_us_t started_at;
    uint16_t calibration;
    uint16_t configuration;
    bool initialized;
    bool measuring;
    bool uncertain;
    bool entered;
} nx_ina226_t;
/**
 * \brief Compute floor(5120000000000 / (shunt_uohm * current_lsb_nA)).
 * \param[in] shunt_microohms: Positive physical resistor scale.
 * \param[in] current_lsb_nanoamps: Positive chosen current unit.
 * \param[out] calibration: Distinct aligned 1..32767 result on SUCCESS only.
 * \return SUCCESS, INVALID or EXHAUSTED outside the calibration range.
 * \note Pure task/IRQ arithmetic; no device access, heap, time or retention.
 */
nx_result_t nx_ina226_calibration(uint32_t shunt_microohms,
                                  uint32_t current_lsb_nanoamps,
                                  uint16_t* calibration);
/**
 * \brief Verify identity, configure shutdown, and program/read back scaling.
 * \param[in,out] device: Fresh zero/stopped or same-binding inactive state.
 * \param[in] config: Stable disjoint hardware/scale facts, retained until stop.
 * \param[in] deadline: Finite absolute budget for the full initialization.
 * \return SUCCESS or exact validation/identity/transport/time error.
 * \note Task-only; no scan, reset, worker, heap or implicit wait. Manufacturer
 *       5449h and die 2260h/2261h are accepted, not unique identity proof.
 *       Existing ALERT controls reject before writes. Once any configuration
 *       write is issued, failure retains this binding as uncertain; shutdown
 *       with a separate cleanup budget is required before stop/rebinding.
 */
nx_result_t nx_ina226_init(nx_ina226_t* device,
                           const nx_ina226_config_t* config,
                           nx_time_us_t deadline);
/**
 * \brief Trigger one shunt+bus conversion using the verified calibration.
 * \param[in,out] device: Initialized nonmeasuring serialized task state.
 * \param[in] deadline: Finite budget including later poll/read operations.
 * \return SUCCESS, BUSY, STATE, TIMEOUT or checked transport error.
 * \note Configuration write clears stale conversion-ready. No busy wait or
 *       data buffer is retained. Uncertain issued writes require shutdown.
 */
nx_result_t nx_ina226_begin(nx_ina226_t* device, nx_time_us_t deadline);
/**
 * \brief Poll readiness and consume one complete one-shot measurement.
 * \param[in,out] device: Same task owner after a successful begin.
 * \param[out] sample: Distinct aligned output, unchanged on every error.
 * \return SUCCESS, BUSY before ready, STATE or checked error.
 * \note CVRF is read-to-clear; a failure after readiness cannot retry that
 *       sample. OVF, reset/calibration drift, short I2C progress and impossible
 *       bus range close sampling until shutdown/reinit. Every endpoint return
 *       ends temporary buffer borrowing, including errors and timeout.
 */
nx_result_t nx_ina226_poll(nx_ina226_t* device, nx_ina226_sample_t* sample);
/**
 * \brief Write and verify shutdown under a separate cleanup deadline.
 * \param[in,out] device: Live or uncertain same-binding serialized state.
 * \param[in] deadline: Finite cleanup budget in the original clock epoch.
 * \return SUCCESS only after shutdown readback; failures retain uncertainty.
 * \note No reset or owner release. Success retires the conversion and requires
 *       fresh initialization before another begin; no borrowed MCU span exists.
 */
nx_result_t nx_ina226_shutdown(nx_ina226_t* device, nx_time_us_t deadline);
/**
 * \brief Drop a quiescent software binding after its users have joined.
 * \param[in,out] device: Same-task inactive, certain caller state.
 * \return SUCCESS, BUSY during a call, or STATE with live/uncertain conversion.
 * \note No I/O or I2C owner release. Complete shutdown first when required.
 */
nx_result_t nx_ina226_stop(nx_ina226_t* device);
#ifdef __cplusplus
}
#endif
#endif
