/**
 * \file            exti.h
 *
 * \brief           Static EXTI line bindings, bounded event storage and drain.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_EXTI_H
#define NEXUS_EXTI_H

#include "nexus/core/time.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_exti_port nx_exti_port_t;
typedef enum {
    NX_EXTI_RISING = 1,
    NX_EXTI_FALLING = 2,
    NX_EXTI_BOTH = 3
} nx_exti_edge_t;
#define NX_EXTI_EVENT_LOSS      1u
#define NX_EXTI_EVENT_COALESCED 2u
/**
 * \brief           Edge facts; timestamp precision and coalescing are
 *                  provider-specific.
 *
 * \note            LOSS is an independent boundary after older buffered facts
 *                  and before later facts. Its edge is invalid; its timestamp
 *                  is detection/report time, never a reconstructed missing
 *                  edge. For COALESCED facts, edge may derive from sampled pin
 *                  level after an IRQ and does not represent complete edge
 *                  history.
 */
typedef struct {
    nx_time_us_t timestamp_us;
    uint32_t flags;
    uint8_t line;
    nx_exti_edge_t edge;
} nx_exti_event_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*read)(void* context, nx_exti_event_t* events, size_t capacity,
                        size_t* count);
    nx_result_t (*stop)(void* context);
    nx_result_t (*attach_wake)(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling);
} nx_exti_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_exti_port {
    const nx_exti_ops_t* ops;
    void* context;
};

/**
 * \brief           Read queued EXTI events from an exact static line binding.
 *
 * \param[in,out]   port: One consumer; bounded configured IRQ-producer storage.
 *
 * \param[out]      events: Caller destination, never retained.
 *
 * \param[in]       capacity: Destination event count.
 *
 * \param[out]      count: Copied events; loss is observable via event flags.
 *
 * \return          Success, EMPTY or INVALID. No debounce policy or callbacks.
 */
nx_result_t nx_exti_port_read(const nx_exti_port_t* port,
                              nx_exti_event_t* events, size_t capacity,
                              size_t* count);
/**
 * \brief           Disable the line source, clear pending and drain its
 *                  handler.
 *
 * \param[in,out]   port: Execution owner; consumer/writers must quiesce first.
 *
 * \return          Success proves no remaining accesses to event storage; BUSY
 *                  retains it. Shared vectors preserve other maintained lines.
 */
nx_result_t nx_exti_port_stop(const nx_exti_port_t* port);
/**
 * \brief           Attach or detach an explicit event/loss IRQ wake target.
 *
 * \param[in]       port: Initialized line, controlled by one task executor.
 *
 * \param[in]       wake: Immutable live target, or NULL to detach.
 *
 * \param[in]       syscall_ceiling: Optional additional unshifted kernel-safe
 *                  priority floor; cannot weaken generated policy. PRIMASK
 *                  kernel targets require zero. Ignored without kernel calls.
 *
 * \return          Success or CONTEXT/STATE/PERMISSION/UNSUPPORTED.
 *
 * \note            Actual shared-vector priority must satisfy the target. Stop
 *                  and join publishers before freeing old target storage. Wake
 *                  callbacks observe already-latched event or loss facts.
 */
nx_result_t nx_exti_port_attach_wake(const nx_exti_port_t* port,
                                     const nx_irq_wake_t* wake,
                                     uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif

#endif
