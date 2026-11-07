/**
 * \file            timer_advanced_controller.h
 * \brief           Optional static timer storage and immutable cold facts
 * \author          Nexus Team
 */
#ifndef NX_TIMER_ADVANCED_CONTROLLER_H
#define NX_TIMER_ADVANCED_CONTROLLER_H
#include "nexus/io/advanced_pwm.h"
#include "nexus/io/timer_capture.h"
/** \brief Exact silicon input selection; filter is the reviewed hardware code.
 */
typedef enum {
    NX_TIMER_EDGE_RISING = 0,
    NX_TIMER_EDGE_FALLING,
    NX_TIMER_EDGE_BOTH
} nx_timer_edge_t;
/**
 * \brief Cold immutable capture facts from the single typed assembly.
 * \note Assembly owns the enabled clock, reviewed AF pin and vector lease.
 *       Exclusive requires every other controller writer to be quiescent.
 *       Clock frequency is the fixed SoC profile, not a physical measurement.
 */
typedef struct {
    uint32_t timer_hz;
    uint32_t tick_hz;
    nx_timer_edge_t edge;
    uint8_t filter;
    bool exclusive;
} nx_timer_capture_config_t;
/**
 * \brief Exact caller storage; zero initialize and never mutate internals.
 * \note IRQ receives this explicit identity from generated static dispatch.
 *       No global registry or implicit vector ownership exists.
 *       config != NULL marks an acquired cold session before any timer write;
 *       initialized marks successful readback and admits hot controls/IRQ.
 *       A failed constructor retains config until stop, publisher withdrawal
 *       and private release prove detachment. Pure refusal keeps zero storage.
 */
typedef struct {
    const nx_timer_capture_config_t* config;
    nx_timer_capture_event_t* events;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    uint32_t loss_incidents;
    uint32_t pending_flags;
    bool initialized;
    bool running;
    bool draining;
} nx_timer_capture_controller_t;
/**
 * \brief Fixed complementary pair and exact hardware break/deadtime policy.
 * \note break_input is the actual GPIO input data register of a reviewed
 *       silicon route; one-bit mask and polarity come from cold assembly.
 *       Assembly owns both AF outputs, BKIN, clock and shared vector leases.
 *       tDTS uses timer_hz with CKD zero, independent of PSC. Only data is
 *       caller owned; hardware may force MOE low asynchronously.
 */
typedef struct {
    const volatile uint32_t* break_input;
    uint32_t break_mask;
    uint32_t timer_hz;
    uint32_t tick_hz;
    uint32_t period_ticks;
    uint32_t deadtime_ticks;
    bool break_active_high;
    bool idle_high;
    bool idle_complement_high;
    bool exclusive;
} nx_advanced_pwm_config_t;
/**
 * \brief Static selected-mode control; default PWM storage is unchanged.
 * \note config is the acquired-session marker, initialized the ready marker.
 *       Failed cold construction retains the immutable config through cleanup.
 *       Assembly stops any acquired session, joins its publisher, releases
 *       this control, then proves Board inactive levels before giving up pins.
 */
typedef struct {
    const nx_advanced_pwm_config_t* config;
    uint32_t duty_ticks;
    bool initialized;
    bool fault;
} nx_advanced_pwm_controller_t;
#endif
