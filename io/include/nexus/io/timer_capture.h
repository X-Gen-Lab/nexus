/**
 * \file            timer_capture.h
 * \brief           Optional raw timer edge capture with caller-owned events
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_TIMER_CAPTURE_H
#define NEXUS_IO_TIMER_CAPTURE_H
#include "nexus/core/time.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observable loss incidents, not an exact count of physical edges. */
enum nx_timer_capture_flags {
    NX_TIMER_CAPTURE_OVERCAPTURE = 1U,
    NX_TIMER_CAPTURE_RING_LOSS = 2U
};
/** \brief Hardware modulo counter and the separate IRQ observation time. */
typedef struct {
    nx_time_us_t observed_at;
    uint32_t captured_ticks;
    uint32_t flags;
} nx_timer_capture_event_t;
/** \brief Fixed counter units and current software retention state. */
typedef struct {
    uint32_t tick_hz;
    uint32_t loss_incidents;
    size_t pending;
    uint8_t counter_bits;
    bool running;
    bool draining;
} nx_timer_capture_state_t;
/** \brief Shared immutable methods; no per-call acquisition or allocation. */
typedef struct {
    nx_result_t (*start)(void* context, nx_timer_capture_event_t* events,
                         size_t capacity);
    nx_result_t (*stop)(void* context);
    nx_result_t (*read)(void* context, nx_timer_capture_event_t* event);
    nx_result_t (*state)(const void* context, nx_timer_capture_state_t* state);
} nx_timer_capture_ops_t;
/** \brief Immutable identity; state and methods outlive calls and IRQs. */
struct nx_timer_capture {
    const nx_timer_capture_ops_t* ops;
    void* context;
};
/**
 * \brief Borrow an exact event ring and enable the fixed capture channel.
 * \param[in] port: One privileged task owner, serialized with stop/read.
 * \param[in,out] events: Aligned writable ring, capacity positive.
 * \param[in] capacity: Number of events; no hidden buffer or worker.
 * \return SUCCESS transfers the ring borrow; rejection retains nothing.
 * \note Ring remains borrowed until successful stop. ISR access is confined
 *       to this ring. Caller must not modify it or its provider while borrowed.
 *       Counter wrap is modulo the reported width; observation time is not
 *       an absolute edge timestamp or a clock accuracy qualification.
 */
nx_result_t nx_timer_capture_start(const nx_timer_capture_t* port,
                                   nx_timer_capture_event_t* events,
                                   size_t capacity);
/**
 * \brief Disable and read back channel/IRQ sources before returning storage.
 * \param[in] port: Privileged task owner, with generated IRQ owner retained.
 * \return SUCCESS is the final access to the ring; BUSY retains the borrow
 *         while queued events or failed hardware detachment remain.
 * \note Read drains events after capture stops. Failed detach never frees
 *       storage. Board pins, timer clock and shared vector leases are released
 *       by cold assembly only after successful stop and publisher quiescence.
 */
nx_result_t nx_timer_capture_stop(const nx_timer_capture_t* port);
/**
 * \brief Copy the oldest event in a short saved-mask metadata section.
 * \param[in] port: Single privileged task consumer.
 * \param[out] event: Separate writable output, unchanged on EMPTY/rejection.
 * \return SUCCESS, EMPTY, STATE or INVALID; no block release is required.
 */
nx_result_t nx_timer_capture_read(const nx_timer_capture_t* port,
                                  nx_timer_capture_event_t* event);
/**
 * \brief Query counter units and observable queue/loss facts.
 * \param[in] port: Privileged task owner; no concurrent lifecycle writer.
 * \param[out] state: Separate writable snapshot.
 * \return SUCCESS or INVALID/STATE/CONTEXT.
 * \note Hardware overcapture and ring saturation can coalesce edges. A loss
 *       incident does not estimate edges, edge frequency or actual IRQ WCET.
 */
nx_result_t nx_timer_capture_state(const nx_timer_capture_t* port,
                                   nx_timer_capture_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
