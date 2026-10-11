/**
 * \file            threadx.c
 * \brief           Static-object services over the reviewed ThreadX kernel
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/threadx.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/atomic.h"
#include "nexus/core/fault.h"
#include "tx_initialize.h"
#include <limits.h>
#include <string.h>
#if defined(__arm__) || defined(__thumb__)
#include "nexus_config.h"
#endif
#ifdef NEXUS_THREADX_IRQ_MODEL
#include "private/irq_model.h"
#define NX_TX_IRQ_REGISTER(address, type)                                      \
    ((volatile type*)nx_threadx_irq_model_register(address))
#else
#define NX_TX_IRQ_REGISTER(address, type) ((volatile type*)(uintptr_t)(address))
#endif

#ifdef TX_DISABLE_PREEMPTION_THRESHOLD
#error "ThreadX byte queues require task preemption threshold guards"
#endif
#ifndef TX_TIMER_PROCESS_IN_ISR
#error "ThreadX adapter does not create an implicit timer worker"
#endif
#if defined(__arm__) || defined(__thumb__)
#ifndef NEXUS_CPU_EXTERNAL_IRQ_COUNT
#error "ThreadX requires reviewed CPU IRQ facts"
#endif
#ifndef NEXUS_IRQ_PRIORITY_BITS
#error "ThreadX requires reviewed CPU priority facts"
#endif
#endif

_Static_assert(sizeof(ULONG) == 4 && sizeof(UINT) == 4,
               "Reviewed ThreadX ports require 32-bit words");

/* These pinned kernel facts are observed, never forged or used as a global
 * suspend API. Bootstrap deletion uses narrow unchecked services only while
 * this adapter owns the actual tx_application_define callback scope. */
extern volatile ULONG _tx_thread_system_state;
extern UINT _tx_thread_preempt_disable;
static uint32_t s_phase;
static void (*s_startup)(void*);
static void* s_startup_context;
enum { NX_TX_COLD, NX_TX_BOOTSTRAP, NX_TX_RUNNING, NX_TX_FAILED };

/** \brief Product hook default retains effects and never releases ownership. */
__attribute__((weak)) void nx_threadx_fail_stop(nx_result_t reason) {
    (void)reason;
    for (;;) {
    }
}

/** \brief A returning hook does not grant reclamation. */
static void fail_stop(nx_result_t reason) {
    nx_fault_observe(NX_FAULT_THREADX_ADAPTER, reason, __FILE__, __LINE__);
    nx_threadx_fail_stop(reason);
    for (;;) {
    }
}

/** \brief Observe bootstrap only inside the actual kernel application callback.
 */
static bool bootstrap(void) {
    return nx_atomic_u32_load_acquire(&s_phase) == NX_TX_BOOTSTRAP &&
           _tx_thread_system_state == TX_INITIALIZE_IN_PROGRESS;
}

/** \brief Canonical context inside the actual initialization callback. */
static bool bootstrap_context(void) {
    if (nx_arch_in_isr() || !nx_arch_is_privileged()) {
        return false;
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.faultmask != 0) {
        return false;
    }
#ifdef TX_PORT_USE_BASEPRI
    return (masks.primask == 1 && masks.basepri == 0) ||
           (masks.primask == 0 &&
            (masks.basepri == 0 || masks.basepri == TX_PORT_BASEPRI));
#else
    return masks.basepri == 0 && masks.primask <= 1;
#endif
}

/** \brief Reject unsafe context before any kernel mutation. */
static bool task_allowed(bool running) {
    if (!running && bootstrap()) {
        return bootstrap_context();
    }
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    return !nx_arch_in_isr() && nx_arch_is_privileged() &&
           masks.faultmask == 0 &&
           nx_atomic_u32_load_acquire(&s_phase) == NX_TX_RUNNING &&
           masks.primask == 0 && masks.basepri == 0 &&
           _tx_thread_system_state == 0 && _tx_thread_preempt_disable == 0 &&
           tx_thread_identify() != TX_NULL;
}

/** \brief Reject wrapping and overlapping known caller storage intervals. */
static bool disjoint(const void* first, size_t first_bytes, const void* second,
                     size_t second_bytes) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    if (first_bytes > UINTPTR_MAX - a || second_bytes > UINTPTR_MAX - b) {
        return false;
    }
    return a + first_bytes <= b || b + second_bytes <= a;
}

bool nx_threadx_isr_allowed(void) {
    if (!nx_arch_is_privileged() || nx_arch_irq_is_masked()) {
        return false;
    }
#if defined(__arm__) || defined(__thumb__) || defined(NEXUS_THREADX_IRQ_MODEL)
    uint32_t exception = nx_arch_exception_number();
    if (exception < 16 || exception - 16 >= NEXUS_CPU_EXTERNAL_IRQ_COUNT) {
        return false;
    }
#ifdef TX_PORT_USE_BASEPRI
    uint32_t grouping = (*NX_TX_IRQ_REGISTER(0xe000ed0cu, uint32_t) >> 8) & 7u;
    uint32_t maximum =
        NEXUS_IRQ_PRIORITY_BITS >= 7 ? 0u : 7u - NEXUS_IRQ_PRIORITY_BITS;
    uint32_t priority =
        NX_TX_IRQ_REGISTER(0xe000e400u, uint8_t)[exception - 16];
    return grouping <= maximum && priority >= TX_PORT_BASEPRI;
#else
    return true;
#endif
#else
    return false;
#endif
}

nx_result_t nx_threadx_kernel_enter(void (*startup)(void*), void* context) {
    if (startup == NULL || nx_atomic_u32_load_acquire(&s_phase) != NX_TX_COLD) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || !nx_arch_is_privileged() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
#if defined(__arm__) || defined(__thumb__)
    uint32_t control;
    __asm__ volatile("mrs %0, control" : "=r"(control));
    if ((control & 7u) != 0) {
        return NX_ERROR_CONTEXT;
    }
#endif
    s_startup = startup;
    s_startup_context = context;
    /* Attempt is irreversible, even if kernel entry never reaches bootstrap. */
    nx_atomic_u32_store_release(&s_phase, NX_TX_FAILED);
    tx_kernel_enter();
    nx_atomic_u32_store_release(&s_phase, NX_TX_FAILED);
    fail_stop(NX_ERROR_IO);
    return NX_ERROR_IO;
}

/** \brief Kernel wiring invokes exactly one product-owned startup callback. */
void tx_application_define(void* unused_memory) {
    (void)unused_memory;
    if (nx_atomic_u32_load_acquire(&s_phase) != NX_TX_FAILED ||
        s_startup == NULL ||
        _tx_thread_system_state != TX_INITIALIZE_IN_PROGRESS ||
        !bootstrap_context()) {
        fail_stop(NX_ERROR_CONTEXT);
    }
    nx_atomic_u32_store_release(&s_phase, NX_TX_BOOTSTRAP);
    s_startup(s_startup_context);
    s_startup = NULL;
    nx_atomic_u32_store_release(&s_phase, NX_TX_RUNNING);
}

/** \brief Round remaining absolute budget without colliding with WAIT_FOREVER.
 */
static ULONG ticks_until(nx_time_us_t deadline) {
    if (deadline == NX_DEADLINE_NEVER) {
        return TX_WAIT_FOREVER;
    }
    nx_time_us_t now = nx_time_now_us();
    if (now >= deadline) {
        return TX_NO_WAIT;
    }
    uint64_t remaining = deadline - now;
    uint64_t seconds = remaining / 1000000u;
    uint64_t maximum = (uint64_t)TX_WAIT_FOREVER - 1u;
    if (seconds > maximum / TX_TIMER_TICKS_PER_SECOND) {
        return (ULONG)maximum;
    }
    uint64_t ticks = seconds * TX_TIMER_TICKS_PER_SECOND;
    ticks += ((remaining % 1000000u) * TX_TIMER_TICKS_PER_SECOND + 999999u) /
             1000000u;
    return (ULONG)(ticks > maximum ? maximum : ticks);
}

/** \brief Keep a finite Core deadline across bounded kernel wait chunks. */
static nx_result_t take_until(TX_SEMAPHORE* semaphore, nx_time_us_t deadline) {
    for (;;) {
        if (tx_semaphore_get(semaphore, TX_NO_WAIT) == TX_SUCCESS) {
            return NX_SUCCESS;
        }
        ULONG ticks = ticks_until(deadline);
        if (ticks == TX_NO_WAIT) {
            return NX_ERROR_TIMEOUT;
        }
        UINT status = tx_semaphore_get(semaphore, ticks);
        if (status == TX_SUCCESS) {
            return NX_SUCCESS;
        }
        if (status != TX_NO_INSTANCE) {
            return NX_ERROR_IO;
        }
    }
}

/** \brief Delete only after application users quiesce or in proven bootstrap.
 */
static UINT semaphore_delete(TX_SEMAPHORE* semaphore) {
    return bootstrap() ? _tx_semaphore_delete(semaphore)
                       : tx_semaphore_delete(semaphore);
}

/** \brief Snapshot hint without acquiring payload ownership. */
static uint32_t notify_arm(void* context) {
    return nx_atomic_u32_load_acquire(
        &((nx_threadx_notify_t*)context)->sequence);
}

/** \brief Deadline always precedes hint; latch closes wake-before-block race.
 */
static nx_result_t notify_wait(void* context, uint32_t sequence,
                               uint64_t deadline) {
    nx_threadx_notify_t* notification = context;
    if (!task_allowed(true)) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t expected = 0;
    if (!nx_atomic_u32_compare_exchange_acq_rel(&notification->waiting,
                                                &expected, 1)) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = NX_SUCCESS;
    for (;;) {
        ULONG ticks = ticks_until(deadline);
        if (ticks == TX_NO_WAIT) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        if (notify_arm(context) != sequence) {
            break;
        }
        UINT status = tx_semaphore_get(&notification->control, ticks);
        if (status != TX_SUCCESS && status != TX_NO_INSTANCE) {
            result = NX_ERROR_IO;
            break;
        }
    }
    nx_atomic_u32_store_release(&notification->waiting, 0);
    return result;
}

/** \brief Publish hint before binary ceiling put, valid task or reviewed IRQ.
 */
static nx_result_t notify_wake(void* context) {
    nx_threadx_notify_t* notification = context;
    if ((nx_arch_in_isr() && !nx_threadx_isr_allowed()) ||
        (!nx_arch_in_isr() && !task_allowed(false))) {
        return NX_ERROR_CONTEXT;
    }
    (void)nx_atomic_u32_fetch_add_release(&notification->sequence, 1);
    UINT status = tx_semaphore_ceiling_put(&notification->control, 1);
    return status == TX_SUCCESS || status == TX_CEILING_EXCEEDED ? NX_SUCCESS
                                                                 : NX_ERROR_IO;
}

nx_result_t nx_threadx_notify_init(nx_threadx_notify_t* notification) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (notification == NULL || notification->live) {
        return NX_ERROR_INVALID;
    }
    UINT status = tx_semaphore_create(&notification->control, (CHAR*)"wake", 0);
    if (status != TX_SUCCESS) {
        return NX_ERROR_IO;
    }
    notification->sequence = 0;
    notification->waiting = 0;
    notification->live = true;
    return NX_SUCCESS;
}

nx_wait_port_t nx_threadx_notify_port(nx_threadx_notify_t* notification) {
    if (notification == NULL || !notification->live) {
        return (nx_wait_port_t){0};
    }
    return (nx_wait_port_t){notification, notify_arm, notify_wait, notify_wake};
}

nx_result_t nx_threadx_notify_destroy(nx_threadx_notify_t* notification) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (notification == NULL || !notification->live) {
        return NX_ERROR_INVALID;
    }
    if (nx_atomic_u32_load_acquire(&notification->waiting) != 0) {
        return NX_ERROR_BUSY;
    }
    if (semaphore_delete(&notification->control) != TX_SUCCESS) {
        return NX_ERROR_IO;
    }
    notification->live = false;
    return NX_SUCCESS;
}

/** \brief Priority-zero time slicing cannot be excluded by threshold zero. */
static bool guard_allowed(void) {
    if (!task_allowed(true)) {
        return false;
    }
    TX_THREAD* current = tx_thread_identify();
    return current->tx_thread_priority != 0 ||
           current->tx_thread_new_time_slice == TX_NO_TIME_SLICE;
}

/** \brief Threshold zero excludes other tasks, leaving IRQ delivery available.
 */
static UINT guard_begin(void) {
    UINT previous;
    if (tx_thread_preemption_change(tx_thread_identify(), 0, &previous) !=
        TX_SUCCESS) {
        fail_stop(NX_ERROR_CONTEXT);
    }
    return previous;
}

/** \brief Restore exact incoming task threshold after consistent publication.
 */
static void guard_end(UINT previous) {
    UINT discarded;
    if (tx_thread_preemption_change(tx_thread_identify(), previous,
                                    &discarded) != TX_SUCCESS) {
        fail_stop(NX_ERROR_CONTEXT);
    }
}

nx_result_t nx_threadx_queue_init(nx_threadx_queue_t* queue, uint8_t* storage,
                                  size_t storage_bytes, size_t depth,
                                  size_t item_size) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || queue->live || storage == NULL || depth == 0 ||
        depth >= TX_WAIT_FOREVER || item_size == 0 ||
        depth > SIZE_MAX / item_size || storage_bytes != depth * item_size ||
        !disjoint(queue, sizeof *queue, storage, storage_bytes)) {
        return NX_ERROR_INVALID;
    }
    if (tx_semaphore_create(&queue->readable, (CHAR*)"items", 0) !=
        TX_SUCCESS) {
        return NX_ERROR_IO;
    }
    if (tx_semaphore_create(&queue->writable, (CHAR*)"spaces", (ULONG)depth) !=
        TX_SUCCESS) {
        if (semaphore_delete(&queue->readable) != TX_SUCCESS) {
            fail_stop(NX_ERROR_IO);
        }
        return NX_ERROR_IO;
    }
    queue->storage = storage;
    queue->depth = depth;
    queue->item_size = item_size;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->users = 0;
    queue->live = true;
    return NX_SUCCESS;
}

/** \brief Copy only after a kernel capacity/item token has been acquired. */
static nx_result_t queue_call(nx_threadx_queue_t* queue, void* item,
                              nx_time_us_t deadline, bool send) {
    if (!guard_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || !queue->live || item == NULL) {
        return NX_ERROR_INVALID;
    }
    UINT previous = guard_begin();
    if (queue->users == UINT32_MAX) {
        guard_end(previous);
        return NX_ERROR_OVERFLOW;
    }
    queue->users++;
    guard_end(previous);
    nx_result_t result =
        take_until(send ? &queue->writable : &queue->readable, deadline);
    previous = guard_begin();
    if (result == NX_SUCCESS) {
        if (send) {
            memcpy(queue->storage + queue->tail * queue->item_size, item,
                   queue->item_size);
            queue->tail = (queue->tail + 1) % queue->depth;
            queue->count++;
        } else {
            memcpy(item, queue->storage + queue->head * queue->item_size,
                   queue->item_size);
            queue->head = (queue->head + 1) % queue->depth;
            queue->count--;
        }
        if (tx_semaphore_put(send ? &queue->readable : &queue->writable) !=
            TX_SUCCESS) {
            fail_stop(NX_ERROR_IO);
        }
    }
    queue->users--;
    guard_end(previous);
    return result;
}

nx_result_t nx_threadx_queue_send_until(nx_threadx_queue_t* queue,
                                        const void* item,
                                        nx_time_us_t deadline) {
    return queue_call(queue, (void*)item, deadline, true);
}

nx_result_t nx_threadx_queue_receive_until(nx_threadx_queue_t* queue,
                                           void* item, nx_time_us_t deadline) {
    return queue_call(queue, item, deadline, false);
}

nx_result_t nx_threadx_queue_destroy(nx_threadx_queue_t* queue) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || !queue->live) {
        return NX_ERROR_INVALID;
    }
    if (queue->users != 0 || queue->count != 0) {
        return NX_ERROR_BUSY;
    }
    if (semaphore_delete(&queue->readable) != TX_SUCCESS ||
        semaphore_delete(&queue->writable) != TX_SUCCESS) {
        fail_stop(NX_ERROR_IO);
    }
    queue->live = false;
    return NX_SUCCESS;
}

/** \brief First member is the real kernel TCB, avoiding ULONG pointer casts. */
static void task_entry(ULONG input) {
    (void)input;
    nx_threadx_task_t* task = (nx_threadx_task_t*)tx_thread_identify();
    void (*entry)(void*) = task->entry;
    nx_atomic_u32_store_release(&task->entered, 1);
    task->entry = NULL;
    entry(task->context);
    if (!task_allowed(true)) {
        fail_stop(NX_ERROR_CONTEXT);
    }
    nx_atomic_u32_store_release(&task->returned, 1);
    if (tx_semaphore_put(&task->finished) != TX_SUCCESS) {
        fail_stop(NX_ERROR_IO);
    }
    /* Returning enters the real kernel completion path. The semaphore is
     * only a hint: join still requires delete to prove ready-list detachment.
     */
}

/** \brief Validate exact caller capacity before publishing kernel references.
 */
static bool task_arguments(const void* control, size_t control_bytes,
                           const char* name, void* stack, size_t stack_bytes,
                           UINT priority, void (*entry)(void*), void* context) {
    return name != NULL && entry != NULL && stack != NULL &&
           ((uintptr_t)stack & 7u) == 0 && stack_bytes >= TX_MINIMUM_STACK &&
           stack_bytes <= UINT32_MAX && priority < TX_MAX_PRIORITIES &&
           disjoint(control, control_bytes, stack, stack_bytes) &&
           disjoint(name, 1, stack, stack_bytes) &&
           disjoint(name, 1, control, control_bytes) &&
           (context == NULL || (disjoint(context, 1, stack, stack_bytes) &&
                                disjoint(context, 1, control, control_bytes)));
}

nx_result_t nx_threadx_task_start(nx_threadx_task_t* task, const char* name,
                                  void* stack, size_t stack_bytes,
                                  UINT priority, void (*entry)(void*),
                                  void* context) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (task == NULL || task->live ||
        !task_arguments(task, sizeof *task, name, stack, stack_bytes, priority,
                        entry, context)) {
        return NX_ERROR_INVALID;
    }
    if (tx_semaphore_create(&task->finished, (CHAR*)"finished", 0) !=
        TX_SUCCESS) {
        return NX_ERROR_IO;
    }
    task->entry = entry;
    task->context = context;
    task->entered = 0;
    task->returned = 0;
    UINT status = tx_thread_create(&task->control, (CHAR*)name, task_entry, 0,
                                   stack, (ULONG)stack_bytes, priority,
                                   priority, TX_NO_TIME_SLICE, TX_DONT_START);
    if (status != TX_SUCCESS) {
        if (semaphore_delete(&task->finished) != TX_SUCCESS) {
            fail_stop(NX_ERROR_IO);
        }
        task->entry = NULL;
        task->context = NULL;
        return NX_ERROR_IO;
    }
    task->live = true;
    /* Once resumed, a higher-priority task may already run/return before this
     * function returns. Failure retains control rather than guessing deletion.
     */
    return tx_thread_resume(&task->control) == TX_SUCCESS ? NX_SUCCESS
                                                          : NX_ERROR_IO;
}

nx_result_t nx_threadx_task_join(nx_threadx_task_t* task,
                                 nx_time_us_t deadline) {
    if (!task_allowed(true) ||
        (task != NULL && tx_thread_identify() == &task->control)) {
        return NX_ERROR_CONTEXT;
    }
    if (task == NULL || !task->live) {
        return NX_ERROR_INVALID;
    }
    for (;;) {
        if (nx_atomic_u32_load_acquire(&task->returned) == 0) {
            nx_result_t waited = take_until(&task->finished, deadline);
            if (waited != NX_SUCCESS) {
                return waited;
            }
        }
        /* TX_COMPLETED alone is not enough. The pinned delete checks the
         * suspending transition before removing every kernel reference. */
        UINT status = tx_thread_delete(&task->control);
        if (status == TX_SUCCESS) {
            if (tx_semaphore_delete(&task->finished) != TX_SUCCESS) {
                fail_stop(NX_ERROR_IO);
            }
            task->live = false;
            task->entry = NULL;
            task->context = NULL;
            return NX_SUCCESS;
        }
        if (status != TX_DELETE_ERROR) {
            return NX_ERROR_IO;
        }
        if (ticks_until(deadline) == TX_NO_WAIT) {
            return NX_ERROR_TIMEOUT;
        }
        /* A just-returned wrapper or higher-priority joiner must let the
         * completion transition finish; immediate deletion never proves it. */
        if (tx_thread_sleep(1) != TX_SUCCESS) {
            return NX_ERROR_IO;
        }
    }
}

nx_result_t nx_threadx_task_discard_before_scheduler(nx_threadx_task_t* task) {
    if (!bootstrap() || !task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (task == NULL || !task->live || task->entry == NULL ||
        nx_atomic_u32_load_acquire(&task->entered) != 0) {
        return NX_ERROR_INVALID;
    }
    /* Checked public deletion rejects initialization. These two pinned
     * services run only in the callback scope before any task can execute.
     * Termination first removes the scheduler candidate, then delete releases
     * the created list and real Linux pthread resources synchronously. */
    if (_tx_thread_terminate(&task->control) != TX_SUCCESS ||
        _tx_thread_delete(&task->control) != TX_SUCCESS ||
        _tx_semaphore_delete(&task->finished) != TX_SUCCESS) {
        return NX_ERROR_IO;
    }
    task->live = false;
    task->entry = NULL;
    task->context = NULL;
    return NX_SUCCESS;
}

/** \brief Permanent entry has no return latch or hidden reaper. */
static void permanent_entry(ULONG input) {
    (void)input;
    nx_threadx_permanent_task_t* task =
        (nx_threadx_permanent_task_t*)tx_thread_identify();
    task->entry(task->context);
    fail_stop(NX_ERROR_STATE);
}

nx_result_t nx_threadx_permanent_task_start(nx_threadx_permanent_task_t* task,
                                            const char* name, void* stack,
                                            size_t stack_bytes, UINT priority,
                                            void (*entry)(void*),
                                            void* context) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (task == NULL || task->live ||
        !task_arguments(task, sizeof *task, name, stack, stack_bytes, priority,
                        entry, context)) {
        return NX_ERROR_INVALID;
    }
    task->entry = entry;
    task->context = context;
    UINT status = tx_thread_create(&task->control, (CHAR*)name, permanent_entry,
                                   0, stack, (ULONG)stack_bytes, priority,
                                   priority, TX_NO_TIME_SLICE, TX_DONT_START);
    if (status != TX_SUCCESS) {
        task->entry = NULL;
        task->context = NULL;
        return NX_ERROR_IO;
    }
    task->live = true;
    return tx_thread_resume(&task->control) == TX_SUCCESS ? NX_SUCCESS
                                                          : NX_ERROR_IO;
}

nx_result_t nx_threadx_queue_waiter_init(nx_threadx_queue_waiter_t* waiter) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (waiter == NULL || waiter->active != 0 || waiter->next != NULL) {
        return NX_ERROR_INVALID;
    }
    return nx_threadx_notify_init(&waiter->notification);
}

nx_result_t nx_threadx_queue_waiter_destroy(nx_threadx_queue_waiter_t* waiter) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (waiter == NULL) {
        return NX_ERROR_INVALID;
    }
    if (waiter->active != 0) {
        return NX_ERROR_BUSY;
    }
    return nx_threadx_notify_destroy(&waiter->notification);
}

nx_result_t nx_threadx_closable_queue_init(nx_threadx_closable_queue_t* queue,
                                           uint8_t* storage,
                                           size_t storage_bytes, size_t depth,
                                           size_t item_size,
                                           size_t maximum_waiters) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || maximum_waiters == 0 ||
        maximum_waiters >= UINT32_MAX || storage == NULL ||
        !disjoint(queue, sizeof *queue, storage, storage_bytes)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_threadx_queue_init(&queue->queue, storage,
                                               storage_bytes, depth, item_size);
    if (result == NX_SUCCESS) {
        queue->send_waiters = NULL;
        queue->receive_waiters = NULL;
        queue->maximum_waiters = maximum_waiters;
        queue->closed = false;
    }
    return result;
}

/** \brief Threshold guard prevents awakened tasks from unlinking this visit. */
static void broadcast(nx_threadx_queue_waiter_t* waiter) {
    while (waiter != NULL) {
        if (notify_wake(&waiter->notification) != NX_SUCCESS) {
            fail_stop(NX_ERROR_IO);
        }
        waiter = waiter->next;
    }
}

nx_result_t
nx_threadx_closable_queue_close(nx_threadx_closable_queue_t* queue) {
    if (!guard_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || !queue->queue.live) {
        return NX_ERROR_INVALID;
    }
    UINT previous = guard_begin();
    queue->closed = true;
    broadcast(queue->send_waiters);
    broadcast(queue->receive_waiters);
    guard_end(previous);
    return NX_SUCCESS;
}

/** \brief Serialize admission, registry and payload commit in one task domain.
 */
static nx_result_t closable_call(nx_threadx_closable_queue_t* queue, void* item,
                                 nx_time_us_t deadline,
                                 nx_threadx_queue_waiter_t* waiter, bool send) {
    if (!guard_allowed()) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || !queue->queue.live || item == NULL || waiter == NULL ||
        !waiter->notification.live) {
        return NX_ERROR_INVALID;
    }
    UINT previous = guard_begin();
    if (waiter->active != 0 || queue->queue.users >= queue->maximum_waiters) {
        guard_end(previous);
        return NX_ERROR_BUSY;
    }
    nx_threadx_queue_waiter_t** list =
        send ? &queue->send_waiters : &queue->receive_waiters;
    waiter->active = 1;
    waiter->next = *list;
    *list = waiter;
    queue->queue.users++;
    nx_result_t result;
    nx_result_t waited = NX_SUCCESS;
    for (;;) {
        uint32_t sequence = notify_arm(&waiter->notification);
        if (send && queue->closed) {
            result = NX_ERROR_CANCELLED;
            break;
        }
        result = queue_call(&queue->queue, item, 0, send);
        if (result == NX_SUCCESS) {
            broadcast(send ? queue->receive_waiters : queue->send_waiters);
            break;
        }
        if (result != NX_ERROR_TIMEOUT) {
            break;
        }
        if (queue->closed) {
            result = NX_ERROR_CANCELLED;
            break;
        }
        if (waited != NX_SUCCESS) {
            result = waited;
            break;
        }
        if (ticks_until(deadline) == TX_NO_WAIT) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        guard_end(previous);
        waited = notify_wait(&waiter->notification, sequence, deadline);
        previous = guard_begin();
    }
    while (*list != waiter) {
        if (*list == NULL) {
            fail_stop(NX_ERROR_STATE);
        }
        list = &(*list)->next;
    }
    *list = waiter->next;
    waiter->next = NULL;
    waiter->active = 0;
    queue->queue.users--;
    guard_end(previous);
    return result;
}

nx_result_t
nx_threadx_closable_queue_send_until(nx_threadx_closable_queue_t* queue,
                                     const void* item, nx_time_us_t deadline,
                                     nx_threadx_queue_waiter_t* waiter) {
    return closable_call(queue, (void*)item, deadline, waiter, true);
}

nx_result_t
nx_threadx_closable_queue_receive_until(nx_threadx_closable_queue_t* queue,
                                        void* item, nx_time_us_t deadline,
                                        nx_threadx_queue_waiter_t* waiter) {
    return closable_call(queue, item, deadline, waiter, false);
}

nx_result_t
nx_threadx_closable_queue_destroy(nx_threadx_closable_queue_t* queue) {
    if (!task_allowed(false)) {
        return NX_ERROR_CONTEXT;
    }
    if (queue == NULL || !queue->queue.live) {
        return NX_ERROR_INVALID;
    }
    if (!queue->closed || queue->send_waiters != NULL ||
        queue->receive_waiters != NULL) {
        return NX_ERROR_BUSY;
    }
    return nx_threadx_queue_destroy(&queue->queue);
}
