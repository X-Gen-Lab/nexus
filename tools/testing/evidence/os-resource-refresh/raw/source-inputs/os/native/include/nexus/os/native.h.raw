/**
 * \file            native.h
 * \brief           Explicit POSIX notification, queue and task storage
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_NATIVE_H
#define NEXUS_OS_NATIVE_H
#include "nexus/os/wait.h"
#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Single waiter, multiple task publishers; fields are private
 * \note            Initialize/destroy have one external lifecycle owner.
 *                  Stop and join all publishers and the waiter before destroy.
 *                  External pthread_cancel of a caller blocked in these APIs is
 *                  unsupported; use cooperative stop, publish and join.
 */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    uint32_t sequence;
    bool initialized;
    bool waiting;
} nx_native_notify_t;

/**
 * \brief           Caller-sized queue storage; no heap or global object pool
 * \note            One lifecycle owner initializes, closes and destroys.
 *                  Multiple producers/consumers are supported while live.
 *                  External pthread_cancel of blocked users is unsupported.
 */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t readable;
    pthread_cond_t writable;
    uint8_t* bytes;
    size_t depth;
    size_t item_size;
    size_t head;
    size_t count;
    size_t users;
    bool closed;
    bool initialized;
} nx_native_queue_t;

/**
 * \brief           Explicit task handle; stack storage is supplied at start
 * \note            Start/join are serialized by one external lifecycle owner;
 *                  there is one joiner. An entry may attempt a self-join, which
 *                  returns BUSY. External cancellation and detachment are
 *                  unsupported.
 */
typedef struct {
    pthread_t thread;
    void (*entry)(void*);
    void* context;
    bool started;
} nx_native_task_t;

/**
 * \brief           Read POSIX monotonic microseconds
 * \return          Clock value; clock_gettime failure is process-fatal
 */
uint64_t nx_native_now_us(void);
/**
 * \brief           Initialize a notification in unused caller storage
 * \param[out]      notification: Storage kept alive through all publishers
 * \return          NX_SUCCESS or an initialization error
 * \note            Task context; POSIX signal handlers are unsupported.
 */
nx_result_t nx_native_notify_init(nx_native_notify_t* notification);
/**
 * \brief           Bind a single-waiter latched notification port
 * \param[in]       notification: Initialized storage
 * \return          Port using CLOCK_MONOTONIC absolute microseconds and
 *                  NX_DEADLINE_NEVER for untimed waiting
 */
nx_wait_port_t nx_native_notify_port(nx_native_notify_t* notification);
/**
 * \brief           Destroy after external publisher quiescence
 * \param[in,out]   notification: Live storage with no future publishers
 * \return          NX_SUCCESS or BUSY when a waiter is still registered
 * \note            Stop and join publishers first. The check cannot authorize a
 *                  future call to race destruction. No storage access is
 *                  allowed afterward.
 */
nx_result_t nx_native_notify_destroy(nx_native_notify_t* notification);
/**
 * \brief           Initialize an explicit bounded byte-copy queue
 * \param[out]      queue: Unused caller storage
 * \param[in]       storage: Caller-owned byte buffer
 * \param[in]       storage_bytes: Actual byte capacity
 * \param[in]       depth: Number of items, greater than zero
 * \param[in]       item_size: Bytes per item, greater than zero
 * \return          NX_SUCCESS or invalid/initialization error
 * \note            Task context; caller keeps buffer until successful destroy.
 */
nx_result_t nx_native_queue_init(nx_native_queue_t* queue, void* storage,
                                 size_t storage_bytes, size_t depth,
                                 size_t item_size);
/**
 * \brief           Copy one item to the queue
 * \param[in,out]   queue: Live queue
 * \param[in]       item: At least item_size bytes, borrowed during call only
 * \param[in]       deadline_us: Absolute CLOCK_MONOTONIC deadline or
 *                  NX_DEADLINE_NEVER
 * \return          SUCCESS, TIMEOUT, STATE (closed) or error
 * \note            Immediately available capacity is used even at an expired
 *                  deadline. A finite deadline bounds waiting; spurious wakes
 *                  never restart its budget. No item is retained on failure.
 */
nx_result_t nx_native_queue_send(nx_native_queue_t* queue, const void* item,
                                 uint64_t deadline_us);
/**
 * \brief           Receive one item; closed queues can still be drained
 * \param[in,out]   queue: Live queue
 * \param[out]      item: At least item_size bytes; valid only on SUCCESS
 * \param[in]       deadline_us: Absolute CLOCK_MONOTONIC deadline or
 *                  NX_DEADLINE_NEVER
 * \return          SUCCESS, TIMEOUT, STATE (closed) or error
 * \note            An immediately available item can be received at an expired
 *                  deadline. On failure the output is unchanged. Closed queues
 *                  return STATE only after existing items have been drained.
 */
nx_result_t nx_native_queue_receive(nx_native_queue_t* queue, void* item,
                                    uint64_t deadline_us);
/**
 * \brief           Reject sends and wake all waiting callers
 * \param[in,out]   queue: Live queue
 * \return          NX_SUCCESS or invalid error
 * \note            Task context; caller must join queue users before destroy.
 */
nx_result_t nx_native_queue_close(nx_native_queue_t* queue);
/**
 * \brief           Destroy quiesced and drained queue storage
 * \param[in,out]   queue: Closed queue, no future users
 * \return          NX_SUCCESS or BUSY if users/items remain
 */
nx_result_t nx_native_queue_destroy(nx_native_queue_t* queue);
/**
 * \brief           Start a task using the supplied host stack
 * \param[out]      task: Zero-initialized unused caller handle
 * \param[in]       stack: Aligned storage accepted by pthread_attr_setstack
 * \param[in]       stack_bytes: Host stack size, at least PTHREAD_STACK_MIN
 * \param[in]       entry: Returning bounded application entry
 * \param[in]       context: Entry context, kept alive until join
 * \return          NX_SUCCESS or invalid/start error
 * \note            Task context. No default task, pool or hidden stack exists.
 *                  POSIX library bookkeeping is host-runtime allocation, not
 *                  MCU RAM evidence.
 */
nx_result_t nx_native_task_start(nx_native_task_t* task, void* stack,
                                 size_t stack_bytes, void (*entry)(void*),
                                 void* context);
/**
 * \brief           Join after requesting cooperative application stop
 * \param[in,out]   task: Started task; caller must not be the joined task
 * \return          NX_SUCCESS only after pthread_join confirms execution exit
 * \note            May block until entry returns. No forced cancellation.
 *                  Caller may release context, handle and stack only after
 *                  success. One external owner serializes start/join; multiple
 *                  simultaneous joiners and external pthread_cancel/detach are
 *                  unsupported. Stop/join publishers before destroying their
 *                  notification or queue storage.
 */
nx_result_t nx_native_task_join(nx_native_task_t* task);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_NATIVE_H */
