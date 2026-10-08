/** Baremetal OSAL: one control thread, hardware ISRs, bounded static resources.
 * This backend does not pretend to implement a stackful RTOS scheduler.
 * Boards install a real monotonic clock; finite waits otherwise fail explicitly.
 */
#include "osal/osal.h"
#include "osal/osal_baremetal.h"
#include "osal/osal_internal.h"
#include "arch/nx_arch.h"
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define OSAL_WEAK __attribute__((weak))
#else
#define OSAL_WEAK
#endif
#ifndef OSAL_QUEUE_MAX_SIZE
#define OSAL_QUEUE_MAX_SIZE 256u
#endif

static uint32_t s_critical_nesting;
static nx_arch_irq_state_t s_saved_irq;
OSAL_WEAK void osal_platform_delay_us(uint32_t us) {
    /* Only a yield hint. This loop never claims to be a time source. */
    for (volatile uint32_t i = 0; i < us; ++i) {}
}
void osal_enter_critical(void) {
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (!s_critical_nesting) s_saved_irq = previous;
    ++s_critical_nesting;
    /* Keep only the outer architecture region. The depth update itself is
     * masked, so an interrupt cannot observe a half-entered outer region. */
    if (s_critical_nesting > 1) nx_arch_irq_restore(previous);
}
void osal_exit_critical(void) {
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    bool restore_outer = s_critical_nesting && --s_critical_nesting == 0;
    nx_arch_irq_restore(previous);
    if (restore_outer) nx_arch_irq_restore(s_saved_irq);
}
bool osal_is_isr(void) { return nx_arch_in_isr(); }
static bool s_initialized;
osal_status_t osal_init(void) {
    if (osal_is_isr()) return OSAL_ERROR_ISR;
    osal_enter_critical(); s_initialized = true; osal_exit_critical();
    return OSAL_OK;
}
bool osal_is_initialized(void) {
    osal_enter_critical(); bool initialized = s_initialized; osal_exit_critical();
    return initialized;
}
void osal_start(void) { /* Product main loop owns scheduling. */ }
bool osal_is_running(void) { return false; }

static uintptr_t s_next_token = 1;
static osal_baremetal_clock_t s_clock;
static osal_error_callback_t s_error_callback;
static osal_stats_t s_stats;

osal_status_t osal_get_time_ms(uint32_t* milliseconds) {
    if (!milliseconds) return OSAL_ERROR_NULL_POINTER;
    if (!s_clock) return OSAL_ERROR_NOT_SUPPORTED;
    *milliseconds = s_clock();
    return OSAL_OK;
}

typedef struct { bool used; void* token; uint32_t recursion; } bare_mutex_t;
typedef struct { bool used; void* token; uint32_t count, maximum; } bare_sem_t;
typedef struct {
    bool used; void* token;
    size_t item_size, capacity, head, tail, count;
    osal_queue_mode_t mode;
    uint8_t data[OSAL_QUEUE_MAX_SIZE];
} bare_queue_t;
typedef struct { bool used; void* token; uint32_t bits; } bare_event_t;
static bare_mutex_t s_mutexes[OSAL_MAX_MUTEXES];
static bare_sem_t s_sems[OSAL_MAX_SEMS];
static bare_queue_t s_queues[OSAL_MAX_QUEUES];
static bare_event_t s_events[OSAL_MAX_EVENTS];
osal_status_t osal_deinit(void) {
    if (osal_is_isr()) return OSAL_ERROR_ISR;
    osal_enter_critical();
    if (s_stats.mutex_count || s_stats.sem_count || s_stats.queue_count || s_stats.event_count) {
        osal_exit_critical(); return OSAL_ERROR_BUSY;
    }
    s_initialized = false;
    osal_exit_critical();
    return OSAL_OK;
}
osal_status_t osal_get_backend_info(osal_backend_info_t* info) {
    if (!info) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical();
    *info = (osal_backend_info_t){
        .backend = OSAL_BACKEND_BAREMETAL,
        .capabilities = OSAL_CAP_STATIC_OBJECTS,
        .delete_policy = OSAL_DELETE_CANCELS_WAITERS,
        .max_mutexes = OSAL_MAX_MUTEXES, .max_semaphores = OSAL_MAX_SEMS,
        .max_queues = OSAL_MAX_QUEUES, .max_events = OSAL_MAX_EVENTS,
        .max_queue_item_bytes = OSAL_MAX_QUEUE_ITEM_SIZE < OSAL_QUEUE_MAX_SIZE ?
            OSAL_MAX_QUEUE_ITEM_SIZE : OSAL_QUEUE_MAX_SIZE,
        .max_queue_storage_bytes = OSAL_QUEUE_MAX_SIZE < OSAL_MAX_QUEUE_BYTES ?
            OSAL_QUEUE_MAX_SIZE : OSAL_MAX_QUEUE_BYTES,
        .reserved_object_bytes = sizeof(s_mutexes) + sizeof(s_sems) +
            sizeof(s_queues) + sizeof(s_events), .event_bits_mask = UINT32_MAX};
    if (s_clock) info->capabilities |= OSAL_CAP_MONOTONIC_CLOCK;
#if defined(__ARM_ARCH_7EM__)
    info->capabilities |= OSAL_CAP_HARDWARE_ISR;
#endif
    osal_exit_critical();
    return OSAL_OK;
}
static void* bare_token(unsigned type) {
    if (s_next_token > (UINTPTR_MAX >> 4)) return NULL;
    return (void*)((s_next_token++ << 4) | type);
}
#define BARE_LOOKUP(name, pool, max, type)                                    \
    static type* name(void* token) {                                         \
        for (unsigned i = 0; i < (max); ++i)                                 \
            if ((pool)[i].used && (pool)[i].token == token) return &(pool)[i]; \
        return NULL;                                                         \
    }
BARE_LOOKUP(bare_mutex, s_mutexes, OSAL_MAX_MUTEXES, bare_mutex_t)
BARE_LOOKUP(bare_sem, s_sems, OSAL_MAX_SEMS, bare_sem_t)
BARE_LOOKUP(bare_queue, s_queues, OSAL_MAX_QUEUES, bare_queue_t)
BARE_LOOKUP(bare_event, s_events, OSAL_MAX_EVENTS, bare_event_t)
#define BARE_TASK_ONLY() do { if (osal_is_isr()) return OSAL_ERROR_ISR; } while (0)
#define BARE_COUNT_INC(kind) do { if (++s_stats.kind##_count > s_stats.kind##_watermark) \
    s_stats.kind##_watermark = s_stats.kind##_count; } while (0)

osal_status_t osal_baremetal_set_clock(osal_baremetal_clock_t clock) {
    BARE_TASK_ONLY();
    if (!clock) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical();
    if (s_clock && s_clock != clock) { osal_exit_critical(); return OSAL_ERROR_BUSY; }
    s_clock = clock;
    osal_exit_critical();
    return OSAL_OK;
}
static osal_status_t bare_wait_setup(uint32_t ms, uint32_t* begin) {
    if (ms != OSAL_NO_WAIT && ms != OSAL_WAIT_FOREVER && !s_clock)
        return OSAL_ERROR_NOT_SUPPORTED;
    *begin = s_clock ? s_clock() : 0;
    return OSAL_OK;
}
static bool bare_expired(uint32_t ms, uint32_t begin) {
    return ms == OSAL_NO_WAIT ||
           (ms != OSAL_WAIT_FOREVER && (uint32_t)(s_clock() - begin) >= ms);
}

osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    osal_enter_critical();
    for (unsigned i = 0; i < OSAL_MAX_MUTEXES; ++i) {
        bare_mutex_t* m = &s_mutexes[i];
        if (m->used) continue;
        void* token = bare_token(OSAL_TYPE_MUTEX);
        if (!token) break;
        *m = (bare_mutex_t){.used = true, .token = token};
        *handle = token;
        BARE_COUNT_INC(mutex);
        osal_exit_critical(); return OSAL_OK;
    }
    osal_exit_critical(); return OSAL_ERROR_NO_MEMORY;
}
osal_status_t osal_mutex_delete(osal_mutex_handle_t handle) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical(); bare_mutex_t* m = bare_mutex(handle);
    if (!m) { osal_exit_critical(); return OSAL_ERROR_INVALID_PARAM; }
    if (m->recursion) { osal_exit_critical(); return OSAL_ERROR_BUSY; }
    m->used = false; --s_stats.mutex_count;
    osal_exit_critical(); return OSAL_OK;
}
osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t ms) {
    (void)ms;
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical(); bare_mutex_t* m = bare_mutex(handle);
    if (!m) { osal_exit_critical(); return OSAL_ERROR_INVALID_PARAM; }
    if (m->recursion == UINT32_MAX) { osal_exit_critical(); return OSAL_ERROR_FULL; }
    ++m->recursion; osal_exit_critical(); return OSAL_OK;
}
osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical(); bare_mutex_t* m = bare_mutex(handle);
    if (!m || !m->recursion) { osal_exit_critical(); return OSAL_ERROR_INVALID_PARAM; }
    --m->recursion; osal_exit_critical(); return OSAL_OK;
}
osal_task_handle_t osal_mutex_get_owner(osal_mutex_handle_t handle) { (void)handle; return NULL; }
bool osal_mutex_is_locked(osal_mutex_handle_t handle) {
    osal_enter_critical(); bare_mutex_t* m = bare_mutex(handle);
    bool locked = m && m->recursion != 0; osal_exit_critical(); return locked;
}

osal_status_t osal_sem_create(uint32_t initial, uint32_t maximum, osal_sem_handle_t* handle) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    if (!maximum || initial > maximum) return OSAL_ERROR_INVALID_PARAM;
    osal_enter_critical();
    for (unsigned i = 0; i < OSAL_MAX_SEMS; ++i) {
        bare_sem_t* s = &s_sems[i];
        if (s->used) continue;
        void* token = bare_token(OSAL_TYPE_SEM);
        if (!token) break;
        *s = (bare_sem_t){.used = true, .token = token, .count = initial, .maximum = maximum};
        *handle = token; BARE_COUNT_INC(sem);
        osal_exit_critical(); return OSAL_OK;
    }
    osal_exit_critical(); return OSAL_ERROR_NO_MEMORY;
}
osal_status_t osal_sem_create_binary(uint32_t initial, osal_sem_handle_t* handle) {
    return osal_sem_create(initial, 1, handle);
}
osal_status_t osal_sem_create_counting(uint32_t maximum, uint32_t initial, osal_sem_handle_t* handle) {
    return osal_sem_create(initial, maximum, handle);
}
osal_status_t osal_sem_delete(osal_sem_handle_t handle) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical(); bare_sem_t* s = bare_sem(handle);
    if (!s) { osal_exit_critical(); return OSAL_ERROR_INVALID_PARAM; }
    s->used = false; --s_stats.sem_count; osal_exit_critical(); return OSAL_OK;
}
osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t ms) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    uint32_t begin;
    osal_status_t setup = bare_wait_setup(ms, &begin);
    if (setup != OSAL_OK) return setup;
    bool seen = false;
    for (;;) {
        osal_enter_critical(); bare_sem_t* s = bare_sem(handle);
        if (!s) { osal_exit_critical(); return seen ? OSAL_ERROR_CANCELLED : OSAL_ERROR_INVALID_PARAM; }
        seen = true;
        if (s->count) { --s->count; osal_exit_critical(); return OSAL_OK; }
        osal_exit_critical();
        if (bare_expired(ms, begin)) return OSAL_ERROR_TIMEOUT;
        osal_platform_delay_us(1);
    }
}
static osal_status_t bare_sem_give(osal_sem_handle_t handle) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical(); bare_sem_t* s = bare_sem(handle);
    if (!s) { osal_exit_critical(); return OSAL_ERROR_INVALID_PARAM; }
    if (s->count == s->maximum) { osal_exit_critical(); return OSAL_ERROR_FULL; }
    ++s->count; osal_exit_critical(); return OSAL_OK;
}
osal_status_t osal_sem_give(osal_sem_handle_t handle) { BARE_TASK_ONLY(); return bare_sem_give(handle); }
osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle) { return bare_sem_give(handle); }
uint32_t osal_sem_get_count(osal_sem_handle_t handle) {
    osal_enter_critical(); bare_sem_t* s = bare_sem(handle);
    uint32_t count = s ? s->count : 0; osal_exit_critical(); return count;
}
osal_status_t osal_sem_reset(osal_sem_handle_t handle, uint32_t count) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical(); bare_sem_t* s = bare_sem(handle);
    if (!s || count > s->maximum) { osal_exit_critical(); return OSAL_ERROR_INVALID_PARAM; }
    s->count = count; osal_exit_critical(); return OSAL_OK;
}

osal_status_t osal_queue_create(size_t size, size_t capacity, osal_queue_handle_t* handle) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    if (!size || !capacity || size > OSAL_MAX_QUEUE_ITEM_SIZE ||
        size > OSAL_QUEUE_MAX_SIZE / capacity) return OSAL_ERROR_INVALID_PARAM;
    osal_enter_critical();
    for (unsigned i = 0; i < OSAL_MAX_QUEUES; ++i) {
        bare_queue_t* q = &s_queues[i];
        if (q->used) continue;
        void* token = bare_token(OSAL_TYPE_QUEUE);
        if (!token) break;
        q->used = true; q->token = token; q->item_size = size; q->capacity = capacity;
        q->head = q->tail = q->count = 0; q->mode = OSAL_QUEUE_MODE_NORMAL;
        *handle = token; BARE_COUNT_INC(queue);
        osal_exit_critical(); return OSAL_OK;
    }
    osal_exit_critical(); return OSAL_ERROR_NO_MEMORY;
}
osal_status_t osal_queue_delete(osal_queue_handle_t handle) {
    BARE_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    osal_enter_critical(); bare_queue_t* q = bare_queue(handle);
    if (!q) { osal_exit_critical(); return OSAL_ERROR_INVALID_PARAM; }
    q->used = false; --s_stats.queue_count; osal_exit_critical(); return OSAL_OK;
}
static osal_status_t bare_queue_put(osal_queue_handle_t handle, const void* item,
                                   uint32_t ms, bool front, bool isr, bool force_overwrite) {
    if (!isr) BARE_TASK_ONLY();
    if (!handle || !item) return OSAL_ERROR_NULL_POINTER;
    uint32_t begin;
    osal_status_t setup = bare_wait_setup(ms, &begin);
    if (setup != OSAL_OK) return setup;
    bool seen = false;
    for (;;) {
        osal_enter_critical(); bare_queue_t* q = bare_queue(handle);
        if (!q) { osal_exit_critical(); return seen ? OSAL_ERROR_CANCELLED : OSAL_ERROR_INVALID_PARAM; }
        seen = true;
        if (q->count == q->capacity && (force_overwrite || q->mode == OSAL_QUEUE_MODE_OVERWRITE)) {
            q->head = (q->head + 1) % q->capacity; --q->count;
        }
        if (q->count < q->capacity) {
            size_t index;
            if (front) { q->head = q->head ? q->head - 1 : q->capacity - 1; index = q->head; }
            else { index = q->tail; q->tail = (q->tail + 1) % q->capacity; }
            memcpy(q->data + index * q->item_size, item, q->item_size); ++q->count;
            osal_exit_critical(); return OSAL_OK;
        }
        osal_exit_critical();
        if (bare_expired(ms, begin)) return ms == OSAL_NO_WAIT ? OSAL_ERROR_FULL : OSAL_ERROR_TIMEOUT;
        osal_platform_delay_us(1);
    }
}
static osal_status_t bare_queue_get(osal_queue_handle_t handle, void* item,
                                   uint32_t ms, bool peek, bool isr) {
    if (!isr) BARE_TASK_ONLY();
    if (!handle || !item) return OSAL_ERROR_NULL_POINTER;
    uint32_t begin;
    osal_status_t setup = bare_wait_setup(ms, &begin);
    if (setup != OSAL_OK) return setup;
    bool seen = false;
    for (;;) {
        osal_enter_critical(); bare_queue_t* q = bare_queue(handle);
        if (!q) { osal_exit_critical(); return seen ? OSAL_ERROR_CANCELLED : OSAL_ERROR_INVALID_PARAM; }
        seen = true;
        if (q->count) {
            memcpy(item, q->data + q->head * q->item_size, q->item_size);
            if (!peek) { q->head = (q->head + 1) % q->capacity; --q->count; }
            osal_exit_critical(); return OSAL_OK;
        }
        osal_exit_critical();
        if (bare_expired(ms, begin)) return ms == OSAL_NO_WAIT ? OSAL_ERROR_EMPTY : OSAL_ERROR_TIMEOUT;
        osal_platform_delay_us(1);
    }
}
osal_status_t osal_queue_send(osal_queue_handle_t h, const void* i, uint32_t ms) { return bare_queue_put(h,i,ms,false,false,false); }
osal_status_t osal_queue_send_front(osal_queue_handle_t h, const void* i, uint32_t ms) { return bare_queue_put(h,i,ms,true,false,false); }
osal_status_t osal_queue_send_from_isr(osal_queue_handle_t h, const void* i) { return bare_queue_put(h,i,0,false,true,false); }
osal_status_t osal_queue_receive(osal_queue_handle_t h, void* i, uint32_t ms) { return bare_queue_get(h,i,ms,false,false); }
osal_status_t osal_queue_peek(osal_queue_handle_t h, void* i) { return bare_queue_get(h,i,0,true,false); }
osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t h, void* i) { return bare_queue_get(h,i,0,false,true); }
osal_status_t osal_queue_peek_from_isr(osal_queue_handle_t h, void* i) { return bare_queue_get(h,i,0,true,true); }
size_t osal_queue_get_count(osal_queue_handle_t h) { osal_enter_critical(); bare_queue_t* q=bare_queue(h); size_t n=q?q->count:0; osal_exit_critical(); return n; }
size_t osal_queue_get_available_space(osal_queue_handle_t h) { osal_enter_critical(); bare_queue_t* q=bare_queue(h); size_t n=q?q->capacity-q->count:0; osal_exit_critical(); return n; }
bool osal_queue_is_empty(osal_queue_handle_t h) { return osal_queue_get_count(h)==0; }
bool osal_queue_is_full(osal_queue_handle_t h) { osal_enter_critical(); bare_queue_t* q=bare_queue(h); bool full=q&&q->count==q->capacity; osal_exit_critical(); return full; }
osal_status_t osal_queue_reset(osal_queue_handle_t h) { BARE_TASK_ONLY(); if(!h)return OSAL_ERROR_NULL_POINTER; osal_enter_critical(); bare_queue_t* q=bare_queue(h); if(!q){osal_exit_critical();return OSAL_ERROR_INVALID_PARAM;} q->head=q->tail=q->count=0; osal_exit_critical();return OSAL_OK; }
osal_status_t osal_queue_set_mode(osal_queue_handle_t h, osal_queue_mode_t mode) { BARE_TASK_ONLY();if(!h)return OSAL_ERROR_NULL_POINTER;osal_enter_critical();bare_queue_t*q=bare_queue(h);if(!q||(mode!=OSAL_QUEUE_MODE_NORMAL&&mode!=OSAL_QUEUE_MODE_OVERWRITE)){osal_exit_critical();return OSAL_ERROR_INVALID_PARAM;}q->mode=mode;osal_exit_critical();return OSAL_OK; }

#include "osal_baremetal_optional.inc"

osal_status_t osal_queue_send_overwrite(osal_queue_handle_t handle, const void* item) {
    return bare_queue_put(handle, item, 0, false, false, true);
}
