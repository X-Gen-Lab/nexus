/**
 * \file            as5047p.h
 * \brief           Selected read-only AS5047P angle and explicit error clearing
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_AS5047P_H
#define NEXUS_COMPONENTS_AS5047P_H
#include "nexus/io/observation.h"
#include "nexus/io/spi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_AS5047P_COUNTS_PER_REVOLUTION 16384U
/**
 * \brief Immutable explicit sensor binding, retained through software stop.
 * \note Endpoint is settled polling SPI mode 1, MSB-first, <=10 MHz, with
 *       >=350 ns CS setup/high and datasheet hold timing. The binding owner
 *       proves those electrical facts; an ordinary endpoint does not assert
 *       them. No IRQ/async fallback or implicit delay is added. This instance
 *       and all frames to this sensor share one serialized task executor.
 *       Nonzero domain/epoch describe the supplied microsecond clock. External
 *       reset/clock discontinuity requires explicit stop and reinitialization.
 */
typedef struct {
    const nx_spi_endpoint_t* spi;
    nx_clock_t clock;
    uint32_t clock_domain;
    uint32_t clock_epoch;
    bool compensated;
} nx_as5047p_config_t;
/**
 * \brief Copied 14-bit modulo angle with a separate diagnostic observation.
 * \note Angle counts have 16384 positions/revolution; no mechanical zero,
 *       direction, unwrapping or speed policy is inferred. Diagnostics retain
 *       MAGH/MAGL warnings. Their read precedes the angle and is not an atomic
 *       physical snapshot. observed is software angle-read completion with
 *       unknown aperture/physical uncertainty, not a shaft-edge timestamp.
 */
typedef struct {
    uint16_t raw_angle;
    uint16_t diagnostics;
    nx_io_timestamp_t observed;
} nx_as5047p_sample_t;
/** \brief Zero-initialized caller storage; fields are private implementation.
 */
typedef struct {
    const nx_as5047p_config_t* config;
    nx_time_us_t last_clock;
    bool initialized;
    bool uncertain;
    bool entered;
} nx_as5047p_t;
/**
 * \brief Check factory/selected settings and ready diagnostics without writes.
 * \param[in,out] device: Fresh zero/stopped or same-binding inactive state.
 * \param[in] config: Stable aligned binding, disjoint from device and endpoint.
 * \param[in] deadline: Finite absolute budget for all frames.
 * \return SUCCESS, BUSY for offset loops, or checked protocol/transport error.
 * \note Task-only; no heap/worker/locks. No unique chip identity is available
 *       in this profile. Successful reads do not prove physical part fitting.
 *       Failed I/O retains this binding for explicit clear/reinit or stop.
 *       No OTP, zero-position, reset or settings writes are performed.
 */
nx_result_t nx_as5047p_init(nx_as5047p_t* device,
                            const nx_as5047p_config_t* config,
                            nx_time_us_t deadline);
/**
 * \brief Read diagnostics and the reply to this call's own angle command.
 * \param[in,out] device: Initialized serialized task owner and live binding.
 * \param[out] sample: Distinct aligned output, unchanged on every error.
 * \param[in] deadline: One finite budget for both two-frame register reads.
 * \return SUCCESS, BUSY for unready diagnostics, or checked error.
 * \note First-frame replies belong to an older command and are discarded.
 *       Current replies require even parity and EF=0; CORDIC overflow rejects.
 *       No temporary buffer is retained after any endpoint return. Protocol or
 *       transport failure closes reads until explicit clear/reinitialization.
 *       Config, endpoint, opaque contexts and output storage stay disjoint.
 */
nx_result_t nx_as5047p_read(nx_as5047p_t* device, nx_as5047p_sample_t* sample,
                            nx_time_us_t deadline);
/**
 * \brief Explicitly read/clear ERRFL, then verify no remaining error flags.
 * \param[in,out] device: Bound same-task state, including failed
 * initialization. \param[out] flags: Distinct aligned original
 * PARERR/INVCOMM/FRERR bits. \param[in] deadline: Separate finite cleanup
 * budget for both register reads. \return SUCCESS or checked error; flags is
 * unchanged on failure. \note Read-to-clear is an acknowledged device effect
 * even if this call fails. It is not bus recovery. Success requires fresh init
 * before samples resume; no automatic error clearing or measurement replay.
 */
nx_result_t nx_as5047p_clear_error(nx_as5047p_t* device, uint16_t* flags,
                                   nx_time_us_t deadline);
/**
 * \brief Drop the software binding after every consumer has joined.
 * \param[in,out] device: Same serialized task state, with no executing call.
 * \return SUCCESS, INVALID or BUSY while a callback is active.
 * \note No I/O, bus-owner release or physical power-down. External SPI/CS
 *       recovery responsibility survives this software stop.
 */
nx_result_t nx_as5047p_stop(nx_as5047p_t* device);
#ifdef __cplusplus
}
#endif
#endif
