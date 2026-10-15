/**
 * \file            can_controller.h
 * \brief           Private static bxCAN mechanism and shared filter domain
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_CAN_CONTROLLER_H
#define NEXUS_PRIVATE_CAN_CONTROLLER_H
#include "nexus/arch/arch.h"
#include "nexus/io/can.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    NX_CAN_NORMAL,
    NX_CAN_LOOPBACK,
    NX_CAN_SILENT_LOOPBACK
} nx_can_mode_t;
typedef struct {
    uint32_t bitrate;
    uint16_t prescaler;
    uint8_t segment1;
    uint8_t segment2;
    uint8_t sjw;
} nx_can_timing_t;
typedef struct {
    uint32_t id;
    uint32_t mask;
    uint8_t bank;
    uint8_t controller;
    uint8_t fifo;
    nx_can_format_t format;
    bool remote;
    bool match_remote;
} nx_can_filter_t;
/**
 * \brief One statically assembled primary-clock/filter owner, never a registry.
 * \note Primary clock remains acquired while either active bit is set.
 *       Split/filter storage are immutable and outlive both controllers.
 *       Platform assembly owns clock/pin release only after successful stops.
 */
typedef struct {
    uintptr_t primary;
    volatile uint32_t* clock_enable;
    uint32_t primary_clock_mask;
    uint32_t secondary_clock_mask;
    const nx_can_filter_t* filters;
    size_t filter_count;
    uint8_t split;
    uint8_t active_mask;
    bool configured;
    bool retained;
} nx_can_filter_domain_t;
typedef struct {
    nx_can_tx_request_t* request;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
} nx_can_mailbox_t;
/** \brief Caller/assembly-owned state exists only for selected instances. */
typedef struct {
    uintptr_t registers;
    nx_can_filter_domain_t* domain;
    nx_can_event_t* events;
    size_t capacity;
    size_t head;
    size_t count;
    uint64_t lost;
    nx_time_us_t loss_time;
    uint32_t loss_flags;
    nx_can_mailbox_t slots[3];
    const nx_irq_wake_t* wake;
    nx_irq_policy_t irq_policy;
    nx_irq_source_t sources[4];
    nx_can_timing_t timing;
    uint32_t clock_hz;
    uint32_t drain_timeout_us;
    uint32_t poll_limit;
    uint32_t publishers;
    nx_can_mode_t mode;
    nx_can_state_t snapshot;
    uint8_t index;
    bool fifo_order;
} nx_can_controller_t;
extern const nx_can_ops_t nx_classic_can_ops;
/** \brief Private common classic value validator; no retained reference. */
bool nx_can_frame_valid(const nx_can_frame_t* frame);
/** \brief Cold task-only filter programming; peer controllers must be stopped.
 */
nx_result_t nx_can_filter_domain_initialize(nx_can_filter_domain_t* domain);
/** \brief Release both domain clocks only after every selected controller
 * stops.
 */
nx_result_t nx_can_filter_domain_stop(nx_can_filter_domain_t* domain);
/** \brief Cold task-only start; validates before any retained storage/effects.
 */
nx_result_t nx_can_controller_initialize(nx_can_controller_t* controller);
/** \brief Bounded IRQ executor; no request access after settlement publication.
 */
void nx_can_controller_irq(nx_can_controller_t* controller);
/** \brief Direct MCU register layout; no indirect access table on hot paths. */
#ifndef NX_CAN_READ
#define NX_CAN_READ(base, offset)                                              \
    (*(volatile uint32_t*)((uintptr_t)(base) + (uintptr_t)(offset)))
#endif
#ifndef NX_CAN_WRITE
#define NX_CAN_WRITE(base, offset, value)                                      \
    (NX_CAN_READ((base), (offset)) = (uint32_t)(value))
#endif
#ifndef NX_CAN_POLL
#define NX_CAN_POLL(base) ((void)(base))
#endif
#ifdef __cplusplus
}
#endif
#endif
