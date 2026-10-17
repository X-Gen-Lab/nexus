/**
 * \file            stream_profile.h
 * \brief           Optional readonly continuous-stream cold admission profiles
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_STREAM_PROFILE_H
#define NEXUS_IO_STREAM_PROFILE_H

#include "nexus/io/adc.h"
#include "nexus/io/stream_budget.h"
#include "nexus/io/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Selected UART face and immutable byte-stream assumptions.
 * \note            Reviewed assembly binds the actual continuous provider.
 *                  Two hardware targets and one replacement are required.
 *                  A profile never allocates or owns the caller's pool. The
 *                  normal byte face and provider state retain their layouts.
 *                  Cold admission rechecks payload disjointness in O(n^2);
 *                  no additional check enters the IRQ or steady I/O path.
 */
typedef struct {
    const nx_uart_port_t* port;
    nx_stream_budget_t budget;
} nx_uart_stream_profile_t;

/**
 * \brief           Selected ADC face and complete-scan stream assumptions.
 * \note            One unit is a complete selected scan, stored as one
 *                  uint16_t per channel. Resolution bits never replace storage
 *                  width. Geometry and rates are reviewed assembly inputs;
 *                  they establish neither sample accuracy nor an aperture time.
 */
typedef struct {
    const nx_adc_port_t* port;
    nx_stream_budget_t budget;
} nx_adc_stream_profile_t;

/**
 * \brief           Validate an actual pool before the bound UART may borrow it.
 * \param[in]       profile: Stable readonly reviewed binding and assumptions.
 * \param[in,out]   stream: Initialized unborrowed pool with exact declared
 *                  count and equal declared byte capacities, kept live through
 *                  the original provider's stop and all consumer releases.
 * \return          INVALID for mismatched geometry/alias, STATE for stopping
 *                  metadata, BUSY for existing loans, CONTEXT outside an
 *                  unmasked task, EXHAUSTED for overflow/insufficient budget,
 *                  UNSUPPORTED for target latency; otherwise provider result.
 * \note            Caller joins all pool users before this cold pool check.
 *                  Failure before provider dispatch creates no borrow, epoch
 *                  change or hardware effect. Rate, FIFO-frontier hold and IRQ
 *                  latency remain declared assumptions, never measurements.
 *                  This creates no external UART flow control or losslessness.
 *                  Underlying unprofiled APIs retain their explicit contracts.
 */
nx_result_t
nx_uart_stream_profile_start(const nx_uart_stream_profile_t* profile,
                             nx_stream_t* stream);

/**
 * \brief           Validate pool and whole-scan rate before ADC admission.
 * \param[in]       profile: Stable readonly binding to the selected ADC face.
 * \param[in,out]   stream: Initialized unborrowed exact pool, as for UART.
 * \param[in]       trigger_hz: Positive nominal complete scans per second,
 *                  no greater than the declared production-rate upper bound.
 * \return          Cold-check errors as for UART, or the original info/start
 *                  result. Provider scan, timer, DMA/cache and alias checks
 *                  still apply; no rejected operation retains new references.
 * \note            ADC stopping its trigger creates a sampling gap. No budget
 *                  makes that pause preserve wall-clock samples or prove the
 *                  future consumer/IRQ bounds. No observer/queue is created.
 */
nx_result_t nx_adc_stream_profile_start(const nx_adc_stream_profile_t* profile,
                                        nx_stream_t* stream,
                                        uint32_t trigger_hz);

#ifdef __cplusplus
}
#endif
#endif
