/**
 * \file            wait.h
 * \brief           Optional sequence-latched waiting without hidden workers
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_WAIT_H
#define NEXUS_OS_WAIT_H

#include "nexus/core/status.h"
#include "nexus/core/time.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Single-waiter notification port
 * \details         Caller owns storage and serializes arm/wait. Multiple wake
 *                  publishers are permitted by each backend's documented
 *                  context. Sequence is a hint; the predicate/request is
 *                  authoritative. There must be fewer than 2^32 wakes between
 *                  arm and recheck. All deadlines are absolute monotonic
 *                  microseconds in the backend's clock domain, with
 *                  NX_DEADLINE_NEVER explicitly disabling timeout. Clocks must
 *                  continue while waiting and never change domain; construct
 *                  relative budgets with nx_deadline_after to avoid overflow.
 *                  A timeout does not settle a request or release its buffers.
 *                  Before destroy,
 *                  stop and join every publisher and waiter; SETTLED alone does
 *                  not prove notification storage is reclaimable. arm snapshots
 *                  the sequence. wait must atomically observe a changed
 *                  sequence or a latched wake, including a wake between the
 *                  caller's recheck and sleeping. A changed sequence cannot
 *                  bypass a finite deadline: wait checks expiry on every call
 *                  and every return from blocking before reporting a hint.
 *                  A nonblocking backend may return BUSY while unexpired.
 *                  Only one owner may arm/wait; a second concurrent wait is
 *                  outside the port contract even when a backend rejects it.
 */
typedef struct {
    void* context;
    uint32_t (*arm)(void* context);
    nx_result_t (*wait)(void* context, uint32_t sequence, uint64_t deadline_us);
    nx_result_t (*wake)(void* context);
} nx_wait_port_t;

/**
 * \brief           Wait for an authoritative caller predicate
 * \param[in]       port: Live single-waiter notification port
 * \param[in]       ready: Bounded predicate, called in caller context
 * \param[in]       context: Predicate context, retained for this call only
 * \param[in]       deadline_us: Absolute deadline in port's clock domain, or
 *                  NX_DEADLINE_NEVER; no wall-clock or relative values
 * \return          NX_SUCCESS when predicate is observed true, including a
 *                  final observation after TIMEOUT; otherwise port error
 * \note            Task context. No service, queue, callback dispatch or task
 *                  creation is implicit. A shared-bus client must not use ready
 *                  to service the controller; its execution owner must remain
 *                  running during stop/drain.
 */
nx_result_t nx_wait_until(const nx_wait_port_t* port, bool (*ready)(void*),
                          void* context, uint64_t deadline_us);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_WAIT_H */
