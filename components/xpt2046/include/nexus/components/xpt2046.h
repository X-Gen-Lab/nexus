/**
 * \file            xpt2046.h
 * \brief           Explicit XPT2046 raw differential touch sampling
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_XPT2046_H
#define NEXUS_COMPONENTS_XPT2046_H

#include "nexus/core/time.h"
#include "nexus/io/exti.h"
#include "nexus/io/gpio.h"
#include "nexus/io/observation.h"
#include "nexus/io/spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Exact immutable touch binding and explicit clock identity
 * \note            SPI mode zero/MSB-first and actual settling/rate are owned
 *                  by the selected endpoint. Select exactly one GPIO pen face
 *                  with one-bit mask, or an EXTI pen face with mask zero.
 *                  EXTI must provide the optional live electrical-level
 *                  method; buffered edges never substitute for that predicate.
 *                  Pen is active-low, not an interrupt count. GPIO/pull,
 *                  endpoint, faces, opaque contexts and clock live through
 *                  successful device stop.
 *                  The deadline clock must be the same domain as the endpoint.
 *                  Nonzero domain/epoch change on reset or discontinuity.
 *                  Every 24-clock exchange, including init/stop PD=00, must
 *                  finish before the earlier of the caller deadline and
 *                  checked exchange start plus 1600 us. This conservative
 *                  full-exchange bound is postchecked even after SPI SUCCESS;
 *                  it is not a measured waveform or generic SPI edge limit.
 */
typedef struct {
    const nx_spi_endpoint_t* spi;
    const nx_gpio_port_t* pen;
    uint32_t pen_mask;
    nx_clock_t clock;
    uint32_t clock_domain;
    uint32_t clock_epoch;
    bool pressure;
    const nx_exti_port_t* pen_exti;
} nx_xpt2046_config_t;

/**
 * \brief           Copied 12-bit converter values, without display calibration
 * \note            x/y are channels 101/001; z1/z2 are 011/100 when requested.
 *                  Axis rotation, pressure resistance, filtering and gesture
 *                  policy are external. Pen was low before and after the
 *                  sequence, which does not prove simultaneous conversions or
 *                  a stable physical contact. observed is software completion
 *                  time with unknown uncertainty, never conversion timestamp.
 */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z1;
    uint16_t z2;
    bool pressure_valid;
    nx_io_timestamp_t observed;
} nx_xpt2046_sample_t;

/**
 * \brief           Zero-initialized caller-owned serialized touch state
 * \note            No payload pointer survives a call. Failed I/O retains the
 *                  binding as uncertain; recover the SPI owner first when
 *                  needed, then retry same-binding init or stop. No polling
 *                  worker, debounce, IRQ owner or allocation is introduced.
 */
typedef struct {
    const nx_xpt2046_config_t* config;
    nx_time_us_t last_clock;
    bool initialized;
    bool uncertain;
    bool in_call;
} nx_xpt2046_t;

/**
 * \brief           Send one complete PD=00 conversion to establish PENIRQ mode
 * \param[in,out]   device: Fresh zero/stopped or same-binding uncertain state
 * \param[in]       config: Stable disjoint exact binding, no chip-ID fallback
 * \param[in]       deadline: Finite absolute deadline, including the exchange
 * \return          SUCCESS or exact validation/transport error
 * \note            The chip has no identity read used here. Init does not
 *                  establish physical module fitting. A failed/short transfer
 *                  may have changed converter power state and retains recovery
 *                  responsibility. Rebinding an uncertain device is rejected.
 */
nx_result_t nx_xpt2046_init(nx_xpt2046_t* device,
                            const nx_xpt2046_config_t* config,
                            nx_time_us_t deadline);

/**
 * \brief           Read complete differential X/Y and optional Z1/Z2 samples
 * \param[in,out]   device: Live exclusive initialized instance
 * \param[out]      sample: Separate aligned output, unchanged on every error
 * \param[in]       deadline: One finite deadline for the complete sequence
 * \return          SUCCESS, EMPTY for pen-up, or exact validation/I/O error
 * \note            Each conversion has its own complete 24-clock CS interval,
 *                  differential 12-bit mode and PD=00. Any transfer error
 *                  closes sampling until same-binding init. IRQs are hints;
 *                  the selected live pen predicate is checked before/after I/O.
 *                  Nonzero null/trailing bits reject the exact 12-bit wire
 *                  format; this is not a chip identity or fitting test.
 *                  Output must also be disjoint from opaque SPI/pen/clock
 *                  context storage; only known spans are checked here.
 */
nx_result_t nx_xpt2046_sample(nx_xpt2046_t* device, nx_xpt2046_sample_t* sample,
                              nx_time_us_t deadline);

/**
 * \brief           Restore converter PD=00 before releasing this device binding
 * \param[in,out]   device: Initialized or uncertain exclusive instance
 * \param[in]       deadline: Separate finite cleanup deadline when necessary
 * \return          SUCCESS only after complete cleanup conversion; failure
 *                  preserves the binding and recovery responsibility
 * \note            Does not stop the shared SPI owner, change pad modes or join
 *                  external GPIO/IRQ observers; those remain caller duties.
 */
nx_result_t nx_xpt2046_stop(nx_xpt2046_t* device, nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif

#endif
