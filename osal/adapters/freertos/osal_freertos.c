/** FreeRTOS OSAL adapter: bounded registry, opaque lifetime tokens and pins.
 * Pins protect kernel objects for the duration of waits. Delete returns BUSY
 * while an operation or owner still uses an object; stale handles never refer
 * to an allocation or a new lifetime. Supports the pinned single-core port. */
#include "FreeRTOS.h"
#include "osal/osal.h"
#include "osal/osal_internal.h"
#include "arch/nx_arch.h"
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
#define RTOS_EVENT_MASK OSAL_EVENT_BITS_MASK

#if configSUPPORT_STATIC_ALLOCATION != 1
#error "The OSAL FreeRTOS backend requires static kernel object creation"
#endif
#define RTOS_STACK_WORDS (OSAL_FREERTOS_TASK_STACK_BYTES / sizeof(StackType_t))
#define RTOS_QUEUE_BYTES (OSAL_FREERTOS_QUEUE_STORAGE_BYTES < OSAL_MAX_QUEUE_BYTES ? \
                         OSAL_FREERTOS_QUEUE_STORAGE_BYTES : OSAL_MAX_QUEUE_BYTES)
#define RTOS_QUEUE_ITEM_BYTES (OSAL_MAX_QUEUE_ITEM_SIZE < RTOS_QUEUE_BYTES ? \
                              OSAL_MAX_QUEUE_ITEM_SIZE : RTOS_QUEUE_BYTES)
_Static_assert(configNUMBER_OF_CORES == 1,
               "The OSAL FreeRTOS lifetime contract supports single-core ports only");
_Static_assert(RTOS_STACK_WORDS >= configMINIMAL_STACK_SIZE,
               "OSAL static task storage is smaller than the kernel minimum");
_Static_assert(OSAL_MAX_TASKS <= 64 && OSAL_MAX_MUTEXES <= 64 &&
               OSAL_MAX_SEMS <= 64 && OSAL_MAX_QUEUES <= 64 &&
               OSAL_MAX_EVENTS <= 64 && OSAL_MAX_TIMERS <= 64,
               "OSAL storage slot mask capacity exceeded");

typedef struct {
    StaticTask_t control;
    _Alignas(portBYTE_ALIGNMENT) StackType_t stack[RTOS_STACK_WORDS];
} rtos_task_storage_t;
typedef struct {
    StaticQueue_t control;
    uint8_t payload[RTOS_QUEUE_BYTES];
    uint8_t scratch[RTOS_QUEUE_ITEM_BYTES];
} rtos_queue_storage_t;
static rtos_task_storage_t s_task_storage[OSAL_MAX_TASKS];
static StaticSemaphore_t s_mutex_storage[OSAL_MAX_MUTEXES];
static StaticSemaphore_t s_sem_storage[OSAL_MAX_SEMS];
static rtos_queue_storage_t s_queue_storage[OSAL_MAX_QUEUES];
static StaticEventGroup_t s_event_storage[OSAL_MAX_EVENTS];
static StaticTimer_t s_timer_storage[OSAL_MAX_TIMERS];

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
    unsigned storage_index;
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
static bool s_mem_sealed;
static bool s_initialized;
static uint32_t s_boot_critical_depth;
static nx_arch_irq_state_t s_boot_critical_state;

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
    return nx_arch_in_isr();
}
static UBaseType_t rtos_lock(void) {
    /* The Cortex-M kernel intentionally starts with a sentinel critical
     * nesting value. Its task critical API can keep BASEPRI masked until
     * scheduler start. Boot metadata queries must not freeze the HAL tick. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        return (UBaseType_t)nx_arch_irq_save().value;
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
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        nx_arch_irq_restore((nx_arch_irq_state_t){(uint32_t)mask});
    else if (osal_is_isr()) portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    else taskEXIT_CRITICAL();
}

typedef struct { bool active; UBaseType_t mask; } rtos_boot_kernel_guard_t;
static rtos_boot_kernel_guard_t rtos_boot_kernel_enter(void) {
    rtos_boot_kernel_guard_t guard = {
        .active = xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED, .mask = 0};
    if (guard.active) guard.mask = portSET_INTERRUPT_MASK_FROM_ISR();
    return guard;
}
static void rtos_boot_kernel_exit(rtos_boot_kernel_guard_t guard) {
    if (guard.active) portCLEAR_INTERRUPT_MASK_FROM_ISR(guard.mask);
}

/* Public critical regions are short nonblocking task regions. The HAL provides
 * saved PRIMASK primitives for mixed task/ISR interrupt masking on Cortex-M. */
void osal_enter_critical(void) {
    configASSERT(!osal_is_isr());
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        taskENTER_CRITICAL(); return;
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    configASSERT(s_boot_critical_depth != UINT32_MAX);
    if (!s_boot_critical_depth) s_boot_critical_state = previous;
    ++s_boot_critical_depth;
    if (s_boot_critical_depth > 1) nx_arch_irq_restore(previous);
}
void osal_exit_critical(void) {
    configASSERT(!osal_is_isr());
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        taskEXIT_CRITICAL(); return;
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    configASSERT(s_boot_critical_depth != 0);
    bool outer = --s_boot_critical_depth == 0;
    nx_arch_irq_restore(previous);
    if (outer) nx_arch_irq_restore(s_boot_critical_state);
}
osal_status_t osal_init(void) {
    if (osal_is_isr()) return OSAL_ERROR_ISR;
    UBaseType_t m = rtos_lock(); s_initialized = true; rtos_unlock(m);
    return OSAL_OK;
}
bool osal_is_initialized(void) {
    UBaseType_t m = rtos_lock(); bool initialized = s_initialized; rtos_unlock(m);
    return initialized;
}
void osal_start(void) {
    configASSERT(s_boot_critical_depth == 0);
    vTaskStartScheduler();
}
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
    bool isr = osal_is_isr();
    if (isr && xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        /* The Cortex-M port initializes its ISR priority validation only at
         * scheduler start. Never reach that kernel entry during boot. */
        *milliseconds = 0;
        return OSAL_ERROR_NOT_INIT;
    }
    TickType_t ticks = isr ? xTaskGetTickCountFromISR() : xTaskGetTickCount();
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
    uint64_t occupied = 0;
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i)
        if (s_resources[i].used && s_resources[i].type == type) {
            ++count;
            occupied |= UINT64_C(1) << s_resources[i].storage_index;
        }
    if (count >= rtos_limit(type) || s_next_token > OSAL_LIFETIME_TOKEN_LIMIT) {
        rtos_unlock(mask); return NULL;
    }
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i) {
        rtos_resource_t* r = &s_resources[i];
        if (r->used) continue;
        memset(r, 0, sizeof(*r));
        r->type = type;
        while (occupied & (UINT64_C(1) << r->storage_index))
            ++r->storage_index;
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

osal_status_t osal_deinit(void) {
    if (osal_is_isr()) return OSAL_ERROR_ISR;
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        return OSAL_ERROR_BUSY;
    UBaseType_t m = rtos_lock();
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i) {
        if (s_resources[i].used) {
            rtos_unlock(m);
            return OSAL_ERROR_BUSY;
        }
    }
    bool allocated = s_mem_count != 0;
    if (!allocated) s_initialized = false;
    rtos_unlock(m);
    return allocated ? OSAL_ERROR_BUSY : OSAL_OK;
}

osal_status_t osal_get_backend_info(osal_backend_info_t* info) {
    if (!info) return OSAL_ERROR_NULL_POINTER;
    *info = (osal_backend_info_t){
        .backend = OSAL_BACKEND_FREERTOS,
        .capabilities = OSAL_CAP_TASKS | OSAL_CAP_SOFTWARE_TIMERS |
            OSAL_CAP_STATIC_OBJECTS | OSAL_CAP_DYNAMIC_MEMORY |
            OSAL_CAP_PRIORITY_SCHEDULER |
            OSAL_CAP_MONOTONIC_CLOCK | OSAL_CAP_MEMORY_SEAL,
        .delete_policy = OSAL_DELETE_REQUIRES_IDLE,
        .max_tasks = OSAL_MAX_TASKS, .max_mutexes = OSAL_MAX_MUTEXES,
        .max_semaphores = OSAL_MAX_SEMS, .max_queues = OSAL_MAX_QUEUES,
        .max_events = OSAL_MAX_EVENTS, .max_timers = OSAL_MAX_TIMERS,
        .max_queue_item_bytes = RTOS_QUEUE_ITEM_BYTES,
        .max_queue_storage_bytes = RTOS_QUEUE_BYTES,
        .max_task_stack_bytes = RTOS_STACK_WORDS * sizeof(StackType_t),
        .reserved_object_bytes = sizeof(s_resources) + sizeof(s_task_storage) +
            sizeof(s_mutex_storage) + sizeof(s_sem_storage) + sizeof(s_queue_storage) +
            sizeof(s_event_storage) + sizeof(s_timer_storage),
        .event_bits_mask = RTOS_EVENT_MASK};
#if defined(__ARM_ARCH_7EM__)
    info->capabilities |= OSAL_CAP_HARDWARE_ISR;
#endif
    return OSAL_OK;
}

osal_status_t osal_get_execution_info(osal_execution_info_t* info) {
    if (!info) return OSAL_ERROR_NULL_POINTER;
    UBaseType_t mask = rtos_lock();
    BaseType_t state = xTaskGetSchedulerState();
    *info = (osal_execution_info_t){.backend = OSAL_BACKEND_FREERTOS,
        .initialized = s_initialized, .in_isr = osal_is_isr(),
        .scheduler_state = state == taskSCHEDULER_NOT_STARTED ? OSAL_SCHEDULER_NOT_STARTED :
            state == taskSCHEDULER_SUSPENDED ? OSAL_SCHEDULER_SUSPENDED : OSAL_SCHEDULER_RUNNING};
    rtos_unlock(mask);
    return OSAL_OK;
}

osal_status_t osal_get_resource_usage(osal_resource_usage_t* usage) {
    if (!usage) return OSAL_ERROR_NULL_POINTER;
    UBaseType_t mask = rtos_lock();
    *usage = (osal_resource_usage_t){
        .tasks.capacity = OSAL_MAX_TASKS, .mutexes.capacity = OSAL_MAX_MUTEXES,
        .semaphores.capacity = OSAL_MAX_SEMS, .queues.capacity = OSAL_MAX_QUEUES,
        .events.capacity = OSAL_MAX_EVENTS, .timers.capacity = OSAL_MAX_TIMERS,
        .lifetime_tokens_capacity = OSAL_LIFETIME_TOKEN_LIMIT,
        .lifetime_tokens_issued = s_next_token - 1u,
        .lifetime_tokens_remaining = OSAL_LIFETIME_TOKEN_LIMIT - (s_next_token - 1u)};
    osal_object_usage_t* classes[7] = {NULL, &usage->tasks, &usage->mutexes,
        &usage->semaphores, &usage->queues, &usage->events, &usage->timers};
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i)
        if (s_resources[i].used) ++classes[s_resources[i].type]->reserved;
    rtos_unlock(mask);
    return OSAL_OK;
}
static rtos_resource_t* rtos_pin(void* handle, osal_resource_type_t type) {
    /* Boot task queries may borrow metadata. ISR/runtime actions are gated
     * independently; kernel reads use a boot mask guard. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED && osal_is_isr()) return NULL;
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
    rtos_boot_kernel_guard_t guard = rtos_boot_kernel_enter();
    bool held = type == OSAL_TYPE_MUTEX && xSemaphoreGetMutexHolder(r->kernel);
    rtos_boot_kernel_exit(guard);
    if (r->references || r->command_pending || held) {
        rtos_unlock(mask); return OSAL_ERROR_BUSY;
    }
    r->closing = true;
    *resource = r;
    rtos_unlock(mask);
    return OSAL_OK;
}
#define RTOS_BOOT_CONTEXT() do { if (osal_is_isr()) return OSAL_ERROR_ISR; } while (0)
#define RTOS_TASK_ONLY() do {                                                  \
    RTOS_BOOT_CONTEXT();                                                       \
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)                  \
        return OSAL_ERROR_NOT_INIT;                                            \
    if (xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED)                    \
        return OSAL_ERROR_BUSY;                                                \
} while (0)
#define RTOS_PIN(handle, type, resource)                                      \
    do {                                                                     \
        if (!(handle)) return OSAL_ERROR_NULL_POINTER;                        \
        if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)             \
            return OSAL_ERROR_NOT_INIT;                                       \
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
