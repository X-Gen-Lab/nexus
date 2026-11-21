/**
 * \file            task.h
 * \brief           Portable static tasks and absolute execution budgets
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_TASK_H
#define NEXUS_OS_TASK_H
#include "nexus/core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Returning task; only storage.h completes the selected layout. */
typedef struct nx_task nx_task_t;
/** \brief Permanent task without join-completion storage. */
typedef struct nx_permanent_task nx_permanent_task_t;

/**
 * \brief           Enter the selected kernel through its real bootstrap
 * \param[in]       startup: Non-NULL application bootstrap callback
 * \param[in]       context: Borrowed bootstrap context
 * \return          INVALID/CONTEXT before entry; success never returns
 * \note            One cold privileged lifecycle owner. Platform startup is
 *                  separate. Bootstrap only uses declared nonblocking object
 *                  construction operations. Native parks this calling thread
 *                  after startup; it does not create a hidden coordinator.
 *                  Kernel return cannot authorize resource reclamation.
 */
nx_result_t nx_os_run(void (*startup)(void*), void* context);
/**
 * \brief           Read the selected OS deadline domain in microseconds
 * \return          Monotonic time; the physical clock must already be live
 * \note            Native uses CLOCK_MONOTONIC, independent of model clocks;
 *                  MCU backends use Core time. Keep the domain live through
 *                  waits, drain and join. Zero alone does not prove readiness.
 */
nx_time_us_t nx_os_now_us(void);
/**
 * \brief           Start a returning task in caller-owned static storage
 * \param[in,out]   task: Zeroed unused object, retained until successful join
 * \param[in]       name: Nonempty name retained until join
 * \param[in]       stack: Aligned native stack array, retained until join
 * \param[in]       stack_bytes: Actual byte capacity, not a word count
 * \param[in]       priority: Logical priority; larger values are higher
 * \param[in]       entry: Non-NULL application entry, allowed to return
 * \param[in]       context: Entry context retained until successful join
 * \return          SUCCESS or argument, context, creation or resource error
 * \note            Task/bootstrap only. Prepare all borrowed state before
 *                  calling: entry may run before start returns. One external
 *                  owner serializes start/join. Known control/stack overlap and
 *                  wrapping spans reject before kernel effects. A cleanup
 *                  failure retains its resources and the same object storage.
 */
nx_result_t nx_task_start(nx_task_t* task, const char* name, void* stack,
                          size_t stack_bytes, uint32_t priority,
                          void (*entry)(void*), void* context);
/**
 * \brief           Join only after the actual kernel releases task references
 * \param[in,out]   task: Live task, owned by one external joiner
 * \param[in]       deadline: Absolute OS deadline or NX_DEADLINE_NEVER
 * \return          SUCCESS permits reuse; TIMEOUT retains every resource;
 *                  CONTEXT rejects self-join or an unsafe execution context
 * \note            Running task only. No forced cancellation or detach. Entry
 *                  return and completion notification do not replace join.
 *                  Quiesce all other users before reclaiming caller storage.
 */
nx_result_t nx_task_join_until(nx_task_t* task, nx_time_us_t deadline);
/**
 * \brief           Withdraw a task that has never executed before scheduling
 * \param[in,out]   task: Created, unused task under its unique startup owner
 * \return          SUCCESS after actual deletion; CONTEXT outside the supported
 *                  pre-scheduler phase; failure retains its references
 * \note            Native tasks can execute immediately and reject this call.
 *                  This operation never cancels or joins an executed task.
 */
nx_result_t nx_task_discard_before_scheduler(nx_task_t* task);
/**
 * \brief           Start a permanent task without a completion object
 * \param[in,out]   task: Zeroed unused static control
 * \param[in]       name: Nonempty name retained for the task lifetime
 * \param[in]       stack: Aligned caller stack retained for the lifetime
 * \param[in]       stack_bytes: Actual byte capacity
 * \param[in]       priority: Logical priority supported by the selected backend
 * \param[in]       entry: Application entry that must never return
 * \param[in]       context: Context retained for the task lifetime
 * \return          SUCCESS or an admission/creation error
 * \note            Task/bootstrap only. No join, destroy or detach is implied.
 *                  Unexpected entry return enters fail-stop. Storage survives
 *                  until reset or separately reviewed external termination.
 */
nx_result_t nx_permanent_task_start(nx_permanent_task_t* task, const char* name,
                                    void* stack, size_t stack_bytes,
                                    uint32_t priority, void (*entry)(void*),
                                    void* context);
/**
 * \brief           Sleep until a finite absolute OS deadline
 * \param[in]       deadline: Finite deadline in nx_os_now_us domain
 * \return          SUCCESS after expiration; INVALID for NX_DEADLINE_NEVER;
 *                  CONTEXT when blocking is not permitted
 * \note            Running task only. Expired deadlines return immediately.
 *                  No device service or timer worker is implicit. Tick rounding
 *                  and scheduling can delay return; precision is not one us.
 */
nx_result_t nx_sleep_until(nx_time_us_t deadline);
/**
 * \brief           Offer a scheduling opportunity without fairness guarantees
 * \return          SUCCESS or CONTEXT outside an allowed running task
 * \note            No promise about the next task, lower priorities or
 * progress.
 */
nx_result_t nx_task_yield(void);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_TASK_H */
