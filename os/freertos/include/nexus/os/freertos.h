/**
 * \file            freertos.h
 * \brief           Per-object static FreeRTOS storage and optional thin
 *                  services
 * \author          Nexus Team
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

/**
 * \brief           Test whether the current context may call kernel ISR APIs
 * \return          True for an external Cortex-M IRQ below the syscall ceiling
 * \note            Native POSIX has no physical IRQ and returns false. NMI,
 *                  faults and kernel-owned exceptions are rejected before
 *                  entering FreeRTOS. Runtime grouping must remain the
 *                  maintained all-preemption configuration.
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
 * \param[in]       stack_words: Actual stack capacity in words, not bytes
 * \param[in]       priority: Priority below configMAX_PRIORITIES
 * \param[in]       entry: Application entry, may return after cooperative stop
 * \param[in]       context: Entry context kept until join
 * \return          SUCCESS or invalid/context/start error
 * \note            Task/startup context. No hidden worker or maximum pool.
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
 *                  rejects ISR/self-join
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

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_FREERTOS_H */
