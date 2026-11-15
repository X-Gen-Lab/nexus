/**
 * \file            owner_observer.h
 * \brief           Explicit owner progress diagnostics without default state
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_OWNER_OBSERVER_H
#define NEXUS_COMPONENTS_OWNER_OBSERVER_H
#include "nexus/components/bus_owner.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Copied service facts without request, payload or slot references. */
typedef struct {
    nx_result_t service_result;
    nx_result_t diagnostic_result;
    nx_time_us_t observed_at;
    nx_time_us_t max_gap;
    uint32_t calls;
    uint32_t busy;
    uint32_t errors;
    uint32_t quarantines;
    uint32_t clock_errors;
    nx_owner_ticket_t active;
    nx_request_state_t active_state;
    bool has_active;
    bool idle;
    bool accepting;
    bool queued;
    bool slot_available;
} nx_owner_observation_t;
/**
 * \brief           Opt-in caller sidecar; no pointer added to default owner.
 * \note            Exact owner, slots and guard remain live until detach and
 *                  all observer calls exit. The same unique owner executor
 *                  calls this wrapper; producer calls retain the owner guard.
 *                  Never reinitialize/replace the bound owner underneath it.
 */
typedef struct {
    nx_bus_owner_t* owner;
    nx_time_us_t last_observed;
    nx_owner_observation_t observation;
    bool observed;
} nx_owner_observer_t;
/**
 * \brief           Bind an initialized exact owner without creating work.
 * \param[in,out]   observer: Fresh zero/detached sidecar, distinct from owner.
 * \param[in,out]   owner: Initialized stable owner, no concurrent lifecycle.
 * \return          SUCCESS or INVALID/STATE, no service or borrow on rejection.
 */
nx_result_t nx_owner_observer_bind(nx_owner_observer_t* observer,
                                   nx_bus_owner_t* owner);
/**
 * \brief           Execute exactly one real service step and copy diagnostics.
 * \param[in,out]   observer: Bound sidecar on the unique executor.
 * \param[in]       now: Monotonic timestamp, independent of service result.
 * \param[out]      out: Distinct stable output, no retained references.
 * \return          Actual service result; invalid bindings reject beforehand.
 * \note            Clock reversal never blocks drain or rewrites its result.
 *                  Snapshot uses the existing guard outside hardware service.
 *                  No callback, slot scan or worker occurs under that guard.
 *                  Quarantine and elapsed deadlines never release a loan.
 *                  Output also excludes accepted descriptors and payloads.
 *                  Only the owner/slot control spans are checked here.
 */
nx_result_t nx_owner_observer_service(nx_owner_observer_t* observer,
                                      nx_time_us_t now,
                                      nx_owner_observation_t* out);
/**
 * \brief           Drop only the observation binding, no owner/loan teardown.
 * \param[in,out]   observer: Caller-serialized sidecar, no remaining readers.
 */
void nx_owner_observer_detach(nx_owner_observer_t* observer);
#ifdef __cplusplus
}
#endif
#endif
