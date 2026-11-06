/**
 * \file            ili9341.h
 * \brief           Caller-owned ILI9341 RGB565 parallel-display controller
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_ILI9341_H
#define NEXUS_COMPONENTS_ILI9341_H

#include "nexus/io/parallel16.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Explicit native orientation and RGB/BGR selection
 * \note            madctl permits MY/MX/MV and BGR only (mask E8h). Native
 *                  addressing is 240 by 320; MV exchanges logical dimensions.
 *                  No module-specific gamma, power, scan or voltage settings
 *                  are guessed. Panel-specific additions require a reviewed
 *                  separate configuration; this is the controller reset
 *                  profile with 16-bit RGB565 DBI and display-on.
 * Configuration, endpoint, operations and clock context remain immutable and
 *                  alive until successful stop. Reinitialization of UNCERTAIN
 *                  state requires the same exact configuration object.
 */
typedef struct {
    const nx_parallel16_endpoint_t* endpoint;
    nx_clock_t clock;
    uint8_t madctl;
} nx_ili9341_config_t;

/** \brief Explicit cooperative initialization and uncertainty states. */
typedef enum {
    NX_ILI9341_STOPPED = 0,
    NX_ILI9341_RESET_WAIT,
    NX_ILI9341_SLEEP_WAIT,
    NX_ILI9341_READY,
    NX_ILI9341_UNCERTAIN
} nx_ili9341_phase_t;

/**
 * \brief           Exact caller-owned state, zero before first initialize
 * \note            One nonreentrant task owner; no concurrent calls. The
 *                  configuration reference survives initialization and
 *                  uncertainty until successful stop. Operations borrow spans
 *                  only during the call. Errors after the first command retain
 *                  binding as UNCERTAIN until stop succeeds or the same exact
 *                  binding is reinitialized. Fields are implementation state,
 *                  not permission to bypass lifecycle checks. Software READY
 *                  establishes completed commands, not optical readiness or
 *                  physical timing qualification. Caller spans are disjoint
 *                  from all transport and clock owner storage. Checks exclude
 *                  known readonly objects and context anchors; opaque owner
 *                  extents remain the caller's responsibility.
 */
typedef struct {
    const nx_ili9341_config_t* config;
    nx_time_us_t deadline;
    nx_time_us_t ready_at;
    nx_time_us_t last_clock;
    uint16_t width;
    uint16_t height;
    nx_ili9341_phase_t phase;
    bool in_call;
} nx_ili9341_t;

/**
 * \brief           Verify ID 9341h and begin software-reset initialization
 * \param[in,out]   device: Zero/stopped or same-binding uncertain state.
 * \param[in]       config: Stable immutable endpoint, clock and orientation.
 * \param[in]       deadline: Finite absolute deadline covering all phases.
 * \return          SUCCESS after SWRESET, BUSY for a live instance or exact
 *                  validation/transport failure. Malformed inputs and a budget
 *                  shorter than 240 ms cause no bus access. Once command D3h
 *                  is attempted, failure retains UNCERTAIN binding.
 * \note            ID4 reads four low bytes: dummy, manufacturer, 93h, 41h.
 *                  External NRST is never driven. Call service after at least
 *                  120 ms reset recovery and 120 ms sleep-out recovery. These
 *                  are elapsed clock intervals, never loop-count delays.
 */
nx_result_t nx_ili9341_initialize(nx_ili9341_t* device,
                                  const nx_ili9341_config_t* config,
                                  nx_time_us_t deadline);

/**
 * \brief           Progress at most one timed initialization transition
 * \param[in,out]   device: Same serialized live instance and clock epoch.
 * \return          BUSY before the required interval or after sleep-out;
 *                  SUCCESS after RGB565/orientation/display-on programming,
 *                  or exact failure retaining UNCERTAIN binding.
 * \note            No sleeping, spinning, scheduler, reset or backlight action.
 */
nx_result_t nx_ili9341_service(nx_ili9341_t* device);

/**
 * \brief           Write one complete in-bounds packed RGB565 rectangle
 * \param[in,out]   device: READY single-owner instance.
 * \param[in]       x: Logical left coordinate.
 * \param[in]       y: Logical top coordinate.
 * \param[in]       width: Nonzero rectangle width.
 * \param[in]       height: Nonzero rectangle height.
 * \param[in]       pixels: Stable packed native-halfword RGB565 span.
 * \param[in]       count: Exactly width times height, no implicit stride.
 * \param[out]      written: Distinct completed pixel prefix, set to zero after
 *                  admission and updated even on a pixel-data failure.
 * \param[in]       deadline: Finite absolute deadline for all bus operations.
 * \return          SUCCESS only for all pixels, INVALID for malformed bounds,
 *                  aliases or span, or exact transport/time error. Rejection
 *                  before bus access leaves written unchanged. Post-command
 *                  errors retain UNCERTAIN; no caller span survives return.
 * \note            No clipping, rotation copy, heap or framebuffer is hidden.
 *                  Interruption can leave a partially updated physical panel.
 */
nx_result_t nx_ili9341_write(nx_ili9341_t* device, uint16_t x, uint16_t y,
                             uint16_t width, uint16_t height,
                             const uint16_t* pixels, size_t count,
                             size_t* written, nx_time_us_t deadline);

/**
 * \brief           Send display-off and release the settled software binding
 * \param[in,out]   device: Stopped or same-owner live/uncertain instance.
 * \param[in]       deadline: Finite current deadline, independent of old init.
 * \return          SUCCESS after all port access finishes, BUSY during reset
 *                  or sleep-out quiet time, or exact failure retaining the
 *                  binding. An attempted command failure keeps UNCERTAIN for
 *                  explicit retry; validation never clears existing effects.
 * \note            Does not power down the module, pulse shared NRST, withdraw
 *                  the bus or set backlight. The caller stops all components
 *                  before releasing their transport and platform resources.
 */
nx_result_t nx_ili9341_stop(nx_ili9341_t* device, nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif
