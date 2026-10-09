/** Caller-owned bounded terminal completion dispatch. No worker or allocation. */
#ifndef NX_HAL_COMPLETION_H
#define NX_HAL_COMPLETION_H

#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nx_hal_completion_queue_s nx_hal_completion_queue_t;

/** Identity is local to this queue's lifetime. Zero is never a valid ticket. */
typedef struct nx_hal_completion_ticket_s {
    const nx_hal_completion_queue_t* queue;
    uint64_t sequence;
    uint32_t slot;
} nx_hal_completion_ticket_t;

/** A producer supplies a real terminal result. settled=false is rejected:
 * cancellation requested, abort failure and unknown ownership are not terminal.
 * This adapter does not poll, cancel, drain hardware or authenticate settlement.
 * The device/provider remains the authority for its own buffer lease. */
typedef struct nx_hal_completion_result_s {
    nx_status_t status;
    bool settled;
} nx_hal_completion_result_t;

/** Runs only from dispatch(), outside metadata locks in the consumer thread.
 * ticket/result are values, with result valid only during this callback.
 * The registered context remains borrowed until the callback returns. */
typedef void (*nx_hal_completion_callback_t)(void* context,
    nx_hal_completion_ticket_t ticket, const nx_hal_completion_result_t* result);

/* Caller-provided metadata. Fields are private to this implementation: do not
 * modify/copy active queues or slots. They expose storage sizes, not device state. */
typedef struct nx_hal_completion_slot_s {
    nx_hal_completion_callback_t callback;
    void* context;
    uint64_t sequence;
    nx_hal_completion_result_t result;
    uint32_t next_free;
    uint8_t state;
} nx_hal_completion_slot_t;

struct nx_hal_completion_queue_s {
    nx_hal_completion_slot_t* slots;
    uint32_t* entries;
    uint64_t last_sequence;
    uint32_t slot_capacity;
    uint32_t queue_capacity;
    uint32_t free_head;
    uint32_t head;
    uint32_t tail;
    uint32_t queued;
    uint32_t outstanding;
    bool initialized;
    bool dispatching;
};

/** Zero-initialize once before the object's first init. Keep the object at one
 * address for its lifetime; re-init after deinit preserves sequence exhaustion.
 * Never clear/reuse a live object's memory to reset its ticket identities.
 * Tickets cannot cross object destruction or a new object at the same address. */
#define NX_HAL_COMPLETION_QUEUE_INITIALIZER {0}

/** Task-only, no allocation. Caller owns distinct, writable storage arrays and
 * the queue until successful deinit. Capacities are nonzero, queue<=slots.
 * Lifecycle calls must be serialized by the caller. O(slot_capacity) init runs
 * before publishing the queue; producer/consumer sections are O(1) per event. */
nx_status_t nx_hal_completion_init(nx_hal_completion_queue_t* queue,
    nx_hal_completion_slot_t* slots, uint32_t slot_capacity,
    uint32_t* entries, uint32_t queue_capacity);
/** Task-only. BUSY retains all ownership while any armed/queued/dispatching
 * callback exists. There is no implicit cancellation or discard of callbacks.
 * The owner must quiesce producers before destroying the queue or its storage;
 * successful deinit does not disable hardware IRQs or stop producer threads. */
nx_status_t nx_hal_completion_deinit(nx_hal_completion_queue_t* queue);

/** Task-only no-wait admission. NO_RESOURCE means no free callback slot or
 * exhausted nonrepeating ticket identities; out is invalid on every failure.
 * Arm before handing a ticket to the operation's producer. An admission failure
 * with no device lease can post its actual settled failure result. There is no
 * abandon/cancel shortcut: an unresolved operation keeps its slot owned. */
nx_status_t nx_hal_completion_arm(nx_hal_completion_queue_t* queue,
    nx_hal_completion_callback_t callback, void* context,
    nx_hal_completion_ticket_t* out);

/** Configurable Cortex-M ISR or task producer. Fixed O(1) work, no waits,
 * callbacks, allocation, HAL or OSAL calls. Arch masks CPU-local configurable
 * exceptions for metadata: NMI/HardFault and SMP producers are unsupported.
 * Native's real mutex supports thread race tests, not POSIX signal handlers or
 * a claim of bounded hardware ISR latency. Existing interrupt masks are restored.
 *
 * FULL leaves the ticket armed: retain/latch the result and retry explicitly;
 * never drop it or release callback context. One successful post per ticket;
 * duplicates/stale/wrong-queue tickets return INVALID_STATE. A ticket currently
 * in its callback returns BUSY. An unsettled result returns INVALID_STATE and
 * changes nothing, even when status is CANCELLED/TIMEOUT. */
nx_status_t nx_hal_completion_post(nx_hal_completion_queue_t* queue,
    nx_hal_completion_ticket_t ticket, nx_hal_completion_result_t result);

/** Single task consumer, called explicitly by its owner. Dispatch up to limit
 * FIFO callbacks; zero limit is INVALID_PARAM. Empty returns OK with count zero.
 * Calls do not wait for producers. Concurrent/reentrant dispatch returns BUSY.
 * Callback duration is caller policy and is not bounded by this adapter.
 * A successful post is delivered at most once; progress needs this pump and a
 * callback that returns. The slot is returned only after its callback returns.
 * Task calls reject ISR context or an active architecture interrupt mask. */
nx_status_t nx_hal_completion_dispatch(nx_hal_completion_queue_t* queue,
    uint32_t limit, uint32_t* dispatched);

#ifdef __cplusplus
}
#endif
#endif
