/** FreeRTOS OSAL adapter: bounded registry, opaque lifetime tokens and pins.
 * Pins protect kernel objects for the duration of waits. Delete returns BUSY
 * while an operation or owner still uses an object; stale handles never refer
 * to an allocation or a new lifetime. Supports the pinned single-core port. */
#include "osal/osal.h"
#include "osal/osal_internal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "timers.h"
#include "event_groups.h"
#include <string.h>
#include <stddef.h>

#ifndef OSAL_COMMAND_TIMEOUT_MS
#define OSAL_COMMAND_TIMEOUT_MS 1000u
#endif
#define RTOS_RESOURCE_MAX (OSAL_MAX_TASKS + OSAL_MAX_MUTEXES + OSAL_MAX_SEMS + \
                           OSAL_MAX_QUEUES + OSAL_MAX_TIMERS + OSAL_MAX_EVENTS)
#define RTOS_EVENT_MASK 0x00ffffffu

typedef struct {
    osal_resource_type_t type;
    bool used, closing;
    void* token;
    void* kernel;
    uint32_t references;
    bool stop_requested, finished;
    osal_task_func_t task_func;
    void* arg;
    char name[configMAX_TASK_NAME_LEN];
    osal_queue_mode_t queue_mode;
    size_t capacity;
    void* scratch;
    osal_timer_callback_t callback;
    bool command_pending, command_done, ack_enqueued, delete_enqueued;
    uint32_t period_ms;
} rtos_resource_t;

static rtos_resource_t s_resources[RTOS_RESOURCE_MAX];
static uintptr_t s_next_token = 1;
static uint16_t s_counts[7], s_peaks[7];
static osal_error_callback_t s_error_callback;
static size_t s_mem_allocated, s_mem_peak, s_mem_count;

/* The supported toolchain is GCC/Clang. Applications may replace these hooks
 * with a board fail-stop that places outputs in their safe state. The callback
 * must be nonblocking and must not allocate or call other OSAL APIs. */
#if defined(__GNUC__) || defined(__clang__)
#define RTOS_WEAK __attribute__((weak))
#define RTOS_NORETURN __attribute__((noreturn))
#else
#define RTOS_WEAK
#define RTOS_NORETURN
#endif

RTOS_WEAK RTOS_NORETURN void vAssertCalled(const char* file, int line) {
    taskDISABLE_INTERRUPTS();
    osal_error_callback_t callback = s_error_callback;
    if (callback) callback(OSAL_ERROR, file, (uint32_t)line);
    for (;;) {
        /* A kernel invariant failure cannot safely resume application work. */
    }
}

#if configCHECK_FOR_STACK_OVERFLOW > 0
RTOS_WEAK void vApplicationStackOverflowHook(TaskHandle_t task, char* name) {
    (void)task;
    (void)name;
    vAssertCalled(__FILE__, __LINE__);
}
#endif

#if configUSE_MALLOC_FAILED_HOOK == 1
RTOS_WEAK void vApplicationMallocFailedHook(void) {
    /* heap_4 invokes this after resuming the scheduler and returns NULL. */
    osal_report_error(OSAL_ERROR_NO_MEMORY, __FILE__, __LINE__);
}
#endif

bool osal_is_isr(void) {
#if defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || \
    defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__)
    uint32_t ipsr;
    __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
    return ipsr != 0;
#else
    return false;
#endif
}
static UBaseType_t rtos_lock(void) {
    if (osal_is_isr()) {
#ifdef portASSERT_IF_INTERRUPT_PRIORITY_INVALID
        portASSERT_IF_INTERRUPT_PRIORITY_INVALID();
#endif
        return portSET_INTERRUPT_MASK_FROM_ISR();
    }
    taskENTER_CRITICAL();
    return 0;
}
static void rtos_unlock(UBaseType_t mask) {
    if (osal_is_isr()) portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    else taskEXIT_CRITICAL();
}

/* Public critical regions are short nonblocking task regions. The HAL provides
 * saved PRIMASK primitives for mixed task/ISR interrupt masking on Cortex-M. */
void osal_enter_critical(void) { taskENTER_CRITICAL(); }
void osal_exit_critical(void) { taskEXIT_CRITICAL(); }
osal_status_t osal_init(void) { return OSAL_OK; }
void osal_start(void) { vTaskStartScheduler(); }
bool osal_is_running(void) { return xTaskGetSchedulerState() == taskSCHEDULER_RUNNING; }

static TickType_t rtos_ticks(uint32_t ms) {
    if (ms == OSAL_WAIT_FOREVER) return portMAX_DELAY;
    uint64_t ticks = ((uint64_t)ms * configTICK_RATE_HZ + 999u) / 1000u;
    if (ticks >= (uint64_t)portMAX_DELAY) ticks = (uint64_t)portMAX_DELAY - 1u;
    return (TickType_t)ticks;
}
static uint32_t rtos_milliseconds(TickType_t ticks) {
    uint64_t ms = (uint64_t)ticks * 1000u / configTICK_RATE_HZ;
    return ms > UINT32_MAX ? UINT32_MAX : (uint32_t)ms;
}
osal_status_t osal_get_time_ms(uint32_t* milliseconds) {
    if (!milliseconds) return OSAL_ERROR_NULL_POINTER;
    TickType_t ticks = osal_is_isr() ? xTaskGetTickCountFromISR() : xTaskGetTickCount();
    *milliseconds = (uint32_t)((uint64_t)ticks * 1000u / configTICK_RATE_HZ);
    return OSAL_OK;
}
static unsigned rtos_limit(osal_resource_type_t type) {
    static const unsigned limits[7] = {0, OSAL_MAX_TASKS, OSAL_MAX_MUTEXES,
        OSAL_MAX_SEMS, OSAL_MAX_QUEUES, OSAL_MAX_EVENTS, OSAL_MAX_TIMERS};
    return limits[type];
}
static rtos_resource_t* rtos_find(void* handle, osal_resource_type_t type) {
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i)
        if (s_resources[i].used && s_resources[i].type == type &&
            s_resources[i].token == handle) return &s_resources[i];
    return NULL;
}
static rtos_resource_t* rtos_reserve(osal_resource_type_t type) {
    UBaseType_t mask = rtos_lock();
    unsigned count = 0;
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i)
        if (s_resources[i].used && s_resources[i].type == type) ++count;
    if (count >= rtos_limit(type) || s_next_token > (UINTPTR_MAX >> 4)) {
        rtos_unlock(mask); return NULL;
    }
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i) {
        rtos_resource_t* r = &s_resources[i];
        if (r->used) continue;
        memset(r, 0, sizeof(*r));
        r->type = type;
        r->used = r->closing = true;
        r->token = (void*)((s_next_token++ << 4) | (uintptr_t)type);
        rtos_unlock(mask);
        return r;
    }
    rtos_unlock(mask);
    return NULL;
}
static void rtos_publish(rtos_resource_t* r, void** handle) {
    UBaseType_t mask = rtos_lock();
    r->closing = false;
    if (++s_counts[r->type] > s_peaks[r->type]) s_peaks[r->type] = s_counts[r->type];
    *handle = r->token;
    rtos_unlock(mask);
}
static void rtos_abandon(rtos_resource_t* r) {
    UBaseType_t mask = rtos_lock(); r->used = false; rtos_unlock(mask);
}
static void rtos_reclaim(rtos_resource_t* r) {
    UBaseType_t mask = rtos_lock();
    --s_counts[r->type];
    r->used = false;
    rtos_unlock(mask);
}
static rtos_resource_t* rtos_pin(void* handle, osal_resource_type_t type) {
    UBaseType_t mask = rtos_lock();
    rtos_resource_t* r = rtos_find(handle, type);
    if (r && !r->closing && r->references != UINT32_MAX) ++r->references;
    else r = NULL;
    rtos_unlock(mask);
    return r;
}
static void rtos_unpin(rtos_resource_t* r) {
    UBaseType_t mask = rtos_lock(); --r->references; rtos_unlock(mask);
}
static osal_status_t rtos_close(void* handle, osal_resource_type_t type,
                                rtos_resource_t** resource) {
    if (osal_is_isr()) return OSAL_ERROR_ISR;
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    UBaseType_t mask = rtos_lock();
    rtos_resource_t* r = rtos_find(handle, type);
    if (!r || r->closing) { rtos_unlock(mask); return OSAL_ERROR_INVALID_PARAM; }
    if (r->references || r->command_pending ||
        (type == OSAL_TYPE_MUTEX && xSemaphoreGetMutexHolder(r->kernel))) {
        rtos_unlock(mask); return OSAL_ERROR_BUSY;
    }
    r->closing = true;
    *resource = r;
    rtos_unlock(mask);
    return OSAL_OK;
}
#define RTOS_TASK_ONLY() do { if (osal_is_isr()) return OSAL_ERROR_ISR; } while (0)
#define RTOS_PIN(handle, type, resource)                                      \
    do {                                                                     \
        if (!(handle)) return OSAL_ERROR_NULL_POINTER;                        \
        (resource) = rtos_pin((handle), (type));                              \
        if (!(resource)) return OSAL_ERROR_INVALID_PARAM;                     \
    } while (0)

#include "osal_freertos_sync.inc"
#include "osal_freertos_task.inc"
#include "osal_freertos_timer.inc"
#include "osal_freertos_mem.inc"

osal_status_t osal_get_stats(osal_stats_t* stats) {
    if (!stats) return OSAL_ERROR_NULL_POINTER;
    UBaseType_t m = rtos_lock();
    *stats = (osal_stats_t){.task_count = s_counts[OSAL_TYPE_TASK],
        .mutex_count = s_counts[OSAL_TYPE_MUTEX], .sem_count = s_counts[OSAL_TYPE_SEM],
        .queue_count = s_counts[OSAL_TYPE_QUEUE], .event_count = s_counts[OSAL_TYPE_EVENT],
        .timer_count = s_counts[OSAL_TYPE_TIMER], .task_watermark = s_peaks[OSAL_TYPE_TASK],
        .mutex_watermark = s_peaks[OSAL_TYPE_MUTEX], .sem_watermark = s_peaks[OSAL_TYPE_SEM],
        .queue_watermark = s_peaks[OSAL_TYPE_QUEUE], .event_watermark = s_peaks[OSAL_TYPE_EVENT],
        .timer_watermark = s_peaks[OSAL_TYPE_TIMER], .mem_allocated = s_mem_allocated,
        .mem_peak = s_mem_peak, .mem_alloc_count = s_mem_count};
    rtos_unlock(m);
    return OSAL_OK;
}
osal_status_t osal_reset_stats(void) {
    UBaseType_t m = rtos_lock(); memcpy(s_peaks, s_counts, sizeof(s_counts));
    s_mem_peak = s_mem_allocated; rtos_unlock(m); return OSAL_OK;
}
osal_status_t osal_set_error_callback(osal_error_callback_t callback) {
    UBaseType_t m = rtos_lock(); s_error_callback = callback; rtos_unlock(m); return OSAL_OK;
}
osal_error_callback_t osal_get_error_callback(void) {
    UBaseType_t m = rtos_lock(); osal_error_callback_t cb = s_error_callback;
    rtos_unlock(m); return cb;
}
void osal_report_error(osal_status_t error, const char* file, uint32_t line) {
    osal_error_callback_t cb = osal_get_error_callback();
    if (cb && error != OSAL_OK) cb(error, file, line);
}
void osal_assert_failed(const char* file, uint32_t line) {
    osal_report_error(OSAL_ERROR, file, line);
    /* This is a runtime panic, not a compile-time invariant. */
    // NOLINTNEXTLINE(cert-dcl03-c)
    configASSERT(0);
}
