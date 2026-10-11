/**
 * \file            freertos.h
 * \brief           Per-object static FreeRTOS storage and optional thin
 *                  services
 * \author          Nexus Team
 * \note            Running task operations require privileged unmasked Thread
 *                  mode; IRQ wake has its own contract. Before scheduler start,
 *                  the selected Baseline port may retain PRIMASK=1 or the
 *                  Mainline port its exact syscall BASEPRI; all other masks
 *                  must be zero. Matching a value cannot prove mask provenance.
 *                  Unsafe contexts return CONTEXT before mutating kernel APIs
 *                  or object storage; a scheduler-state read may be needed.
 *                  Startup permits init, task creation, wake and zero-wait
 *                  queues. Wait, join and nonzero waits require a running
 *                  scheduler. Port binding and sequence snapshots remain reads.
 *                  MPU user-task gateways and cross-world secure task calls
 *                  are outside these privileged, current-security-domain APIs.
 */
#ifndef NEXUS_OS_FREERTOS_H
#define NEXUS_OS_FREERTOS_H
#include "FreeRTOS.h"
#include "nexus/core/time.h"
#include "nexus/os/wait.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Static binary latch; one waiter, independently quiesced
 *                  publishers.
 */
typedef struct {
    StaticSemaphore_t storage;
    SemaphoreHandle_t handle;
    uint32_t sequence;
    uint32_t waiting;
} nx_freertos_notify_t;

/**
 * \brief           Task has its own TCB; stack is supplied with its actual word
 *                  count.
 */
typedef struct {
    StaticTask_t control;
    StaticSemaphore_t finished_storage;
    SemaphoreHandle_t finished;
    TaskHandle_t handle;
    void (*entry)(void*);
    void* context;
} nx_freertos_task_t;

/** \brief Queue control only; actual depth/item byte storage is external. */
typedef struct {
    StaticQueue_t control;
    QueueHandle_t handle;
    uint32_t users;
} nx_freertos_queue_t;

/** \brief Fixed receiver and exclusive kernel notification slot; no latch. */
typedef struct {
    TaskHandle_t receiver;
    UBaseType_t index;
    uint32_t sequence;
    uint32_t waiting;
} nx_freertos_direct_notify_t;

/** \brief Permanent task control without a join completion semaphore. */
typedef struct {
    StaticTask_t control;
    TaskHandle_t handle;
} nx_freertos_permanent_task_t;

/** \brief Caller-owned single-operation latch for a closable queue. */
typedef struct nx_freertos_queue_waiter {
    StaticSemaphore_t storage;
    SemaphoreHandle_t handle;
    struct nx_freertos_queue_waiter* next;
    uint32_t active;
} nx_freertos_queue_waiter_t;

/** \brief Opt-in close broadcasts; raw queue storage remains unchanged. */
typedef struct {
    nx_freertos_queue_t queue;
    nx_freertos_queue_waiter_t* send_waiters;
    nx_freertos_queue_waiter_t* receive_waiters;
    uint32_t maximum_waiters;
    bool closed;
} nx_freertos_closable_queue_t;

/**
 * \brief           Test whether the current context may call kernel ISR APIs
 * \return          True for a privileged unmasked external Cortex-M IRQ within
 *                  the selected CPU/SoC bounds and kernel mask contract
 * \note            Native POSIX has no physical IRQ and returns false. NMI,
 *                  faults and kernel-owned exceptions are rejected before
 *                  entering FreeRTOS. Mainline priority/grouping must respect
 *                  the syscall ceiling. Baseline PRIMASK ports accept every
 *                  configurable IRQ and do not read Mainline priority grouping.
 *                  Nonsecure permission does not exclude Secure-side accesses.
 */
bool nx_freertos_isr_allowed(void);
/**
 * \brief           Initialize unused notification storage
 * \param[out]      notification: Caller storage, zeroed before first use
 * \return          SUCCESS or invalid/context/initialization error
 * \note            Task/startup context; no heap or timer daemon is needed.
 */
nx_result_t nx_freertos_notify_init(nx_freertos_notify_t* notification);
/**
 * \brief           Bind a latched single-waiter notification
 * \param[in]       notification: Initialized live storage
 * \return          Notification port using nx_time_now_us clock domain
 * \note            Task wake uses xSemaphoreGive. IRQ wake validates priority
 *                  then uses xSemaphoreGiveFromISR. Saturated latch is
 *                  harmless; sequence and request predicates remain
 *                  authoritative. No deferred daemon publishes wakes.
 */
nx_wait_port_t nx_freertos_notify_port(nx_freertos_notify_t* notification);
/**
 * \brief           Destroy after publishers and waiter have stopped and joined
 * \param[in,out]   notification: Live storage with no future users
 * \return          SUCCESS, BUSY if a waiter is still registered, or error
 * \note            Task context. Quiescence is required beyond this busy check.
 */
nx_result_t nx_freertos_notify_destroy(nx_freertos_notify_t* notification);
/**
 * \brief           Create one task from its exact caller-owned stack and TCB
 * \param[out]      task: Zero-initialized unused control storage
 * \param[in]       name: Kernel diagnostic name, copied by the kernel
 * \param[in]       stack: StackType_t array kept until successful join
 * \param[in]       stack_words: Actual capacity, at least the selected port
 *                  startup minimum; runtime stack budget remains caller-owned
 * \param[in]       priority: Priority below configMAX_PRIORITIES
 * \param[in]       entry: Application entry, may return after cooperative stop
 * \param[in]       context: Entry context kept until join
 * \return          SUCCESS or invalid/context/start error
 * \note            Task/startup context. No hidden worker or maximum pool.
 *                  Entry must return in privileged Thread mode, with manual
 *                  interrupt masks restored and the scheduler resumed after
 *                  any temporary suspension.
 */
nx_result_t nx_freertos_task_start(nx_freertos_task_t* task, const char* name,
                                   StackType_t* stack, size_t stack_words,
                                   UBaseType_t priority, void (*entry)(void*),
                                   void* context);
/**
 * \brief           Join a cooperatively returned task and reclaim kernel state
 * \param[in,out]   task: Started task, distinct from current task
 * \param[in]       deadline: Absolute wait deadline in nx_time_now_us domain
 * \return          SUCCESS only after external vTaskDelete removes the task;
 *                  TIMEOUT retains handle, TCB, stack and context; CONTEXT
 *                  rejects unsafe CPU context, self-join or stopped scheduler
 * \note            Single joiner, task context. The entry return parks its
 *                  wrapper; no self-delete/idle cleanup race releases caller
 *                  storage early. Stop producers, keep bus owner draining, join
 *                  producers, then join owner.
 */
nx_result_t nx_freertos_task_join(nx_freertos_task_t* task,
                                  nx_time_us_t deadline);
/**
 * \brief           Create an exact-sized static byte-copy queue
 * \param[out]      queue: Zero-initialized caller queue control
 * \param[in]       storage: Byte buffer kept until destroy
 * \param[in]       storage_bytes: Actual byte capacity
 * \param[in]       depth: Number of items
 * \param[in]       item_size: Bytes copied per item
 * \return          SUCCESS or invalid/context/start error
 */
nx_result_t nx_freertos_queue_init(nx_freertos_queue_t* queue, uint8_t* storage,
                                   size_t storage_bytes, size_t depth,
                                   size_t item_size);
/**
 * \brief           Copy an item with a bounded relative kernel wait
 * \param[in,out]   queue: Live queue
 * \param[in]       item: At least configured item bytes; not retained afterward
 * \param[in]       wait_ticks: Explicit relative wait; portMAX_DELAY prohibited
 * \return          SUCCESS, TIMEOUT, INVALID or CONTEXT
 * \note            Task context. Admission/stop belongs to the application's
 *                  explicit owner adapter, not this generic copy queue.
 */
nx_result_t nx_freertos_queue_send(nx_freertos_queue_t* queue, const void* item,
                                   TickType_t wait_ticks);
/**
 * \brief           Receive one byte-copy item
 * \param[in,out]   queue: Live queue
 * \param[out]      item: Configured item byte capacity, valid only on success
 * \param[in]       wait_ticks: Explicit relative wait; portMAX_DELAY prohibited
 * \return          SUCCESS, TIMEOUT, INVALID or CONTEXT
 */
nx_result_t nx_freertos_queue_receive(nx_freertos_queue_t* queue, void* item,
                                      TickType_t wait_ticks);
/**
 * \brief           Destroy after admission is stopped and all users joined
 * \param[in,out]   queue: Live drained queue, no future operations
 * \return          SUCCESS, BUSY while users/items remain, or invalid/context
 */
nx_result_t nx_freertos_queue_destroy(nx_freertos_queue_t* queue);

/**
 * \brief           Send using one absolute monotonic deadline
 * \param[in,out]   queue: Live raw queue
 * \param[in]       item: Configured bytes borrowed until return
 * \param[in]       deadline: nx_time_now_us domain, NEVER permits long waits
 * \return          SUCCESS, TIMEOUT or validation/context error
 * \note            Immediate readiness wins over expiration. Stop admission
 *                  and user quiescence remain external raw-queue obligations.
 */
nx_result_t nx_freertos_queue_send_until(nx_freertos_queue_t* queue,
                                         const void* item,
                                         nx_time_us_t deadline);
/**
 * \brief           Receive using one absolute monotonic deadline
 * \param[in,out]   queue: Live raw queue
 * \param[out]      item: Configured bytes valid only on SUCCESS
 * \param[in]       deadline: nx_time_now_us domain
 * \return          SUCCESS, TIMEOUT or validation/context error
 */
nx_result_t nx_freertos_queue_receive_until(nx_freertos_queue_t* queue,
                                            void* item, nx_time_us_t deadline);
/**
 * \brief           Bind an exclusive slot of one live receiver task
 * \param[out]      notification: Zeroed caller storage
 * \param[in]       receiver: Task retained until all publishers quiesce
 * \param[in]       index: Exclusively owned kernel notification slot
 * \return          SUCCESS, INVALID, CONTEXT or UNSUPPORTED by OS profile
 * \note            Caller ensures slot exclusivity. No global slot registry,
 *                  allocation, slot discovery or cross-world synchronization.
 */
nx_result_t
nx_freertos_direct_notify_init(nx_freertos_direct_notify_t* notification,
                               TaskHandle_t receiver, UBaseType_t index);
/**
 * \brief           Get a direct notification wait port
 * \param[in]       notification: Live explicitly bound storage
 * \return          Port using nx_time_now_us or an invalid zero port
 * \note            Only receiver may wait. Task/validated IRQ publishers use
 *                  the exclusive slot; notification remains a predicate hint.
 */
nx_wait_port_t
nx_freertos_direct_notify_port(nx_freertos_direct_notify_t* notification);
/**
 * \brief           Unbind after all publishers stop and join
 * \param[in,out]   notification: Live storage with no future users
 * \return          SUCCESS, BUSY if waiting, or validation/context error
 * \note            Does not delete receiver or clear its kernel slot. Receiver
 *                  and storage must outlive every retained port and publisher.
 */
nx_result_t
nx_freertos_direct_notify_destroy(nx_freertos_direct_notify_t* notification);
/**
 * \brief           Start a task whose entry must never return
 * \param[out]      task: Zeroed permanent control, retained until reset
 * \param[in]       name: Copied kernel diagnostic name
 * \param[in]       stack: Aligned actual stack retained until reset
 * \param[in]       stack_words: Capacity includes reviewed startup minimum
 * \param[in]       priority: Below configMAX_PRIORITIES
 * \param[in]       entry: Permanent entry; return violates kernel contract
 * \param[in]       context: Retained until reset
 * \return          SUCCESS or validation/context/start error
 * \note            No join latch, destroy or forced deletion. Choose joinable
 *                  tasks for cooperative shutdown and reclaimable storage.
 */
nx_result_t
nx_freertos_permanent_task_start(nx_freertos_permanent_task_t* task,
                                 const char* name, StackType_t* stack,
                                 size_t stack_words, UBaseType_t priority,
                                 void (*entry)(void*), void* context);
/**
 * \brief           Initialize one exact caller-owned queue waiter
 * \param[out]      waiter: Zeroed storage, one simultaneous operation maximum
 * \return          SUCCESS or validation/context/start error
 */
nx_result_t nx_freertos_queue_waiter_init(nx_freertos_queue_waiter_t* waiter);
/**
 * \brief           Destroy only after all operations stop and join
 * \param[in,out]   waiter: Live storage without future callers
 * \return          SUCCESS, BUSY if active, or validation/context error
 */
nx_result_t
nx_freertos_queue_waiter_destroy(nx_freertos_queue_waiter_t* waiter);
/**
 * \brief           Initialize an optional reliably closable byte-copy queue
 * \param[out]      queue: Zeroed control kept until destroy
 * \param[in]       storage: Payload buffer kept until destroy
 * \param[in]       storage_bytes: Actual payload capacity
 * \param[in]       depth: Positive item count
 * \param[in]       item_size: Positive byte-copy size
 * \param[in]       maximum_waiters: Explicit positive simultaneous-call budget
 * \return          SUCCESS or validation/context/start error
 * \note            No hidden worker or waiter pool. Each concurrent blocking
 *                  operation supplies its own initialized waiter. Broadcasting
 *                  pauses scheduling while visiting at most maximum_waiters
 *                  latches. Kernel payload copies and each latch signal have
 *                  their own short IRQ guards. Budget scheduling delay and IRQ
 *                  latency separately. Large payloads should queue stable
 *                  handles. The registry is single-core and task-only.
 */
nx_result_t nx_freertos_closable_queue_init(nx_freertos_closable_queue_t* queue,
                                            uint8_t* storage,
                                            size_t storage_bytes, size_t depth,
                                            size_t item_size,
                                            size_t maximum_waiters);
/**
 * \brief           Send with close-aware admission and absolute deadline
 * \param[in,out]   queue: Live closable queue
 * \param[in]       item: Bytes borrowed until return, never after timeout
 * \param[in]       deadline: nx_time_now_us domain, NEVER supported
 * \param[in,out]   waiter: Exclusive initialized caller waiter
 * \return          SUCCESS, CANCELLED after close, TIMEOUT, BUSY or error
 * \note            Readiness/admission and close serialize in a kernel task
 *                  scheduling guard. A latched wake closes wake-before-sleep.
 */
nx_result_t
nx_freertos_closable_queue_send_until(nx_freertos_closable_queue_t* queue,
                                      const void* item, nx_time_us_t deadline,
                                      nx_freertos_queue_waiter_t* waiter);
/**
 * \brief           Receive existing items after close, then CANCELLED
 * \param[in,out]   queue: Live closable queue
 * \param[out]      item: Configured bytes valid only on SUCCESS
 * \param[in]       deadline: nx_time_now_us domain
 * \param[in,out]   waiter: Exclusive initialized caller waiter
 * \return          SUCCESS, CANCELLED when closed/empty, TIMEOUT or error
 */
nx_result_t
nx_freertos_closable_queue_receive_until(nx_freertos_closable_queue_t* queue,
                                         void* item, nx_time_us_t deadline,
                                         nx_freertos_queue_waiter_t* waiter);
/**
 * \brief           Stop sends and wake every currently registered waiter
 * \param[in,out]   queue: Live queue
 * \return          SUCCESS, including repeated close, or validation/context
 * \note            Task-only, no forced cancellation or task deletion. Close
 *                  does not join users or authorize storage reclamation.
 */
nx_result_t
nx_freertos_closable_queue_close(nx_freertos_closable_queue_t* queue);
/**
 * \brief           Destroy a closed, drained and quiesced queue
 * \param[in,out]   queue: Live queue without future entrants
 * \return          SUCCESS, BUSY while open/users/items remain, or error
 * \note            Stop/join producers, drain with owner, join consumers and
 *                  then destroy queue and every waiter independently.
 */
nx_result_t
nx_freertos_closable_queue_destroy(nx_freertos_closable_queue_t* queue);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_FREERTOS_H */
