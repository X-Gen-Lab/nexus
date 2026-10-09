/**
 * \file            bus_owner.h
 * \brief           Explicit bounded multi-producer, single-executor adapter
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_BUS_OWNER_H
#define NEXUS_COMPONENTS_BUS_OWNER_H
#include "nexus/core/request.h"
#include "nexus/os/wait.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Explicit bounded metadata exclusion port
 * \note            Task producer calls only. enter/leave must serialize all
 *                  producers and the executor. Never hold this guard over
 *                  start/service/cancel, waiting, payload copies or
 *                  notification callbacks. State restores incoming interrupt
 *                  mask when an architecture guard is used. Port storage stays
 *                  live.
 */
typedef struct {
    void* context;
    uintptr_t (*enter)(void*);
    void (*leave)(void*, uintptr_t);
} nx_owner_guard_port_t;

/**
 * \brief           One real execution owner, injected without OS dependency
 * \details         start SUCCESS borrows operation until service SUCCESS. BUSY
 *                  or any start error retains zero operation
 *                  references/hardware effects; an error after partial start
 *                  must instead accept and report through service. service
 *                  SUCCESS proves all hardware/IRQ/deferred accesses stopped,
 *                  writes final result/count, and releases the operation. BUSY
 *                  still borrows. Other service errors retain ownership and
 *                  require continued recovery/service. cancel requests drain;
 *                  it never by itself releases operation. Callbacks run only in
 *                  the externally scheduled executor, outside adapter metadata
 *                  guard.
 * \note            terminal_authority selects providers that observe deadlines
 *                  and latch their first terminal hardware fact themselves.
 *                  Their final result survives late cancellation/publication.
 *                  Other executors use adapter deadline/cancellation results.
 *                  Queued deadlines always belong to the adapter.
 */
typedef struct {
    void* context;
    nx_result_t (*start)(void*, void* operation, nx_time_us_t deadline);
    nx_result_t (*service)(void*, nx_result_t* result, size_t* transferred);
    nx_result_t (*cancel)(void*);
    bool terminal_authority;
} nx_owner_executor_port_t;

/**
 * \brief           Stable control identity; never dereference a stale request
 *                  pointer.
 */
typedef struct {
    size_t slot;
    uint64_t epoch;
} nx_owner_ticket_t;

/** \brief Exact caller-owned queue slot; initialize only with owner init. */
typedef struct {
    nx_request_slot_t identity;
    void* operation;
    nx_wait_port_t completion;
    size_t next;
    bool cancel_requested;
    bool cancel_sent;
} nx_owner_slot_t;

/** \brief Explicit adapter state; external application supplies all storage. */
typedef struct {
    nx_owner_slot_t* slots;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t active;
    nx_owner_guard_port_t guard;
    nx_owner_executor_port_t executor;
    nx_clock_t clock;
    nx_wait_port_t wake;
    nx_result_t cancellation_result;
    bool accepting;
    bool servicing;
} nx_bus_owner_t;

/**
 * \brief           Initialize unused owner/slot storage
 * \param[out]      owner: Unused caller-owned adapter storage
 * \param[in]       slots: Exact queue capacity, no maximum pool
 * \param[in]       capacity: Number of slots, greater than zero
 * \param[in]       guard: Live short exclusion port
 * \param[in]       executor: Single real execution port
 * \param[in]       clock: Same monotonic domain as request deadlines
 * \param[in]       wake: Optional static executor notification, or NULL
 * \return          SUCCESS or INVALID
 * \note            Startup context; no task, worker, timer or heap is created.
 */
nx_result_t nx_bus_owner_init(nx_bus_owner_t* owner, nx_owner_slot_t* slots,
                              size_t capacity, nx_owner_guard_port_t guard,
                              nx_owner_executor_port_t executor,
                              nx_clock_t clock, const nx_wait_port_t* wake);
/**
 * \brief           Establish the chain's unique admission
 * \param[in,out]   owner: Live accepting adapter
 * \param[in,out]   request: Prepared READY storage, retained until SETTLED
 * \param[in]       operation: Executor-specific descriptor and payload storage
 * \param[in]       completion: Optional static caller wake port, or NULL
 * \param[out]      ticket: Stable cancellation identity, valid on SUCCESS
 * \return          SUCCESS means ACCEPTED; rejection retains zero references
 * \note            Task multi-producer context. Fixed slot scan is bounded by
 *                  capacity. Deadline includes queue residence. Caller must not
 *                  modify request, operation or buffers after admission. A
 *                  notification error after admission cannot change SUCCESS to
 *                  rejection; executor must recheck/pump periodically.
 */
nx_result_t nx_bus_owner_submit(nx_bus_owner_t* owner, nx_request_t* request,
                                void* operation,
                                const nx_wait_port_t* completion,
                                nx_owner_ticket_t* ticket);
/**
 * \brief           Submit cancellation through stable adapter storage
 * \param[in,out]   owner: Live adapter
 * \param[in]       ticket: Previously accepted slot/epoch
 * \return          SUCCESS when marked, STATE for stale/released identity
 * \note            Task multi-producer context. No request pointer is accepted.
 *                  Queued cancellation settles without starting. Active
 *                  cancellation drains cooperatively; an irreversible operation
 *                  may already have transferred data.
 */
nx_result_t nx_bus_owner_cancel(nx_bus_owner_t* owner,
                                nx_owner_ticket_t ticket);
/**
 * \brief           Advance at most one bounded execution step
 * \param[in,out]   owner: Live adapter, called by its unique executor
 * \return          SUCCESS for progress/idle, BUSY for provider
 *                  wait/contention, or provider error while retaining ownership
 *                  for continued recovery
 * \note            Task/explicit loop only. Consumers never service through
 *                  wait. Keep calling during stop until idle; no callback
 *                  creates a worker.
 */
nx_result_t nx_bus_owner_service(nx_bus_owner_t* owner);
/**
 * \brief           Close admission and request every accepted operation to stop
 * \param[in,out]   owner: Live adapter
 * \note            Task context. Executor must keep servicing/draining while
 *                  producers wait/join. Stop is not settlement or storage
 *                  reclamation.
 */
void nx_bus_owner_stop(nx_bus_owner_t* owner);
/**
 * \brief           Test whether execution and completion publication have
 *                  exited
 * \param[in,out]   owner: Live adapter
 * \return          True if no queue/active/service publisher remains
 * \note            Join producers and any wake publishers before reclaiming
 *                  owner, slots, guard or notifications. No future
 *                  submit/cancel is permitted.
 */
bool nx_bus_owner_idle(nx_bus_owner_t* owner);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_BUS_OWNER_H */
