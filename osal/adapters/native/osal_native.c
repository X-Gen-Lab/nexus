/**
 * \file            osal_native.c
 * \brief           OSAL Native Platform Adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Native platform implementation of OSAL using pthreads.
 *                  Supports task management, mutexes, semaphores, and queues.
 */

/* Enable usleep and pthread on POSIX systems */
#ifndef _WIN32
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#endif

#include "osal/osal.h"
#include "osal/osal_internal.h"
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

/* macOS compatibility: Define PTHREAD_STACK_MIN if not available */
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384
#endif
#endif

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/* Use configuration from osal_config.h for resource limits */
/* Local configuration for native-specific settings */
#define OSAL_QUEUE_MAX_SIZE 256
#define OSAL_TASK_NAME_MAX  32

/* Resource slots and their wait primitives live for the whole process.
 * Every mutable field is protected by the global monitor. */
#ifdef _WIN32
typedef CONDITION_VARIABLE native_cond_t;
typedef DWORD native_thread_t;
#else
typedef pthread_cond_t native_cond_t;
typedef pthread_t native_thread_t;
#endif

typedef struct {
    osal_handle_header_t header;
    bool used, running, finished, suspended, delete_pending, joining;
    void* token;
    char name[OSAL_TASK_NAME_MAX];
    osal_task_func_t func;
    void* arg;
    uint8_t priority;
    native_cond_t changed;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
} osal_task_internal_t;

typedef struct {
    osal_handle_header_t header;
    bool used;
    void* token;
    uint32_t recursion;
    native_thread_t owner_thread;
    osal_task_handle_t owner;
    native_cond_t changed;
} osal_mutex_internal_t;

typedef struct {
    osal_handle_header_t header;
    bool used;
    void* token;
    uint32_t count, max_count;
    native_cond_t changed;
} osal_sem_internal_t;

typedef struct {
    osal_handle_header_t header;
    bool used;
    void* token;
    uint8_t* buffer;
    size_t item_size, item_count, head, tail, count;
    osal_queue_mode_t mode;
    native_cond_t not_empty, not_full;
} osal_queue_internal_t;

typedef struct {
    osal_handle_header_t header;
    bool used, active, delete_pending, callback_running;
    void* token;
    uint32_t period_ms;
    uint64_t epoch;
    uint64_t expires_ms;
    osal_timer_mode_t mode;
    osal_timer_callback_t callback;
    void* arg;
    char name[OSAL_TASK_NAME_MAX];
    native_cond_t changed;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
} osal_timer_internal_t;

typedef struct {
    osal_handle_header_t header;
    bool used;
    void* token;
    osal_event_bits_t bits;
    void* waiters;
    native_cond_t changed;
} osal_event_internal_t;

/*---------------------------------------------------------------------------*/
/* Static Variables                                                          */
/*---------------------------------------------------------------------------*/

static atomic_bool s_osal_initialized = false;
static atomic_bool s_osal_running = false;

static osal_task_internal_t s_tasks[OSAL_MAX_TASKS];
static osal_mutex_internal_t s_mutexes[OSAL_MAX_MUTEXES];
static osal_sem_internal_t s_sems[OSAL_MAX_SEMS];
static osal_queue_internal_t s_queues[OSAL_MAX_QUEUES];
static osal_timer_internal_t s_timers[OSAL_MAX_TIMERS];
static osal_event_internal_t s_events[OSAL_MAX_EVENTS];

#ifdef _WIN32
static CRITICAL_SECTION s_global_cs;
static CRITICAL_SECTION s_critical_cs;
static INIT_ONCE s_runtime_once = INIT_ONCE_STATIC_INIT;
static DWORD s_tls_index = TLS_OUT_OF_INDEXES;
#else
static pthread_mutex_t s_global_mutex;
static pthread_mutex_t s_critical_mutex;
static pthread_once_t s_runtime_once = PTHREAD_ONCE_INIT;
static pthread_key_t s_tls_key;
static bool s_tls_key_created = false;
#endif

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/* One-time initialization also makes first use from concurrent callers safe. */
#ifdef _WIN32
static BOOL CALLBACK native_runtime_init(PINIT_ONCE once, PVOID arg, PVOID* ctx) {
    (void)once; (void)arg; (void)ctx;
    InitializeCriticalSection(&s_global_cs);
    InitializeCriticalSection(&s_critical_cs);
    for (int i = 0; i < OSAL_MAX_TASKS; ++i)
        InitializeConditionVariable(&s_tasks[i].changed);
    for (int i = 0; i < OSAL_MAX_TIMERS; ++i)
        InitializeConditionVariable(&s_timers[i].changed);
    for (int i = 0; i < OSAL_MAX_EVENTS; ++i)
        InitializeConditionVariable(&s_events[i].changed);
    for (int i = 0; i < OSAL_MAX_MUTEXES; ++i)
        InitializeConditionVariable(&s_mutexes[i].changed);
    for (int i = 0; i < OSAL_MAX_SEMS; ++i)
        InitializeConditionVariable(&s_sems[i].changed);
    for (int i = 0; i < OSAL_MAX_QUEUES; ++i) {
        InitializeConditionVariable(&s_queues[i].not_empty);
        InitializeConditionVariable(&s_queues[i].not_full);
    }
    return TRUE;
}
static void native_runtime_ensure(void) {
    InitOnceExecuteOnce(&s_runtime_once, native_runtime_init, NULL, NULL);
}
#else
static void native_runtime_init(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&s_global_mutex, &attr);
    pthread_mutex_init(&s_critical_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    pthread_condattr_t ca;
    pthread_condattr_init(&ca);
#ifndef __APPLE__
    pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
#endif
    for (int i = 0; i < OSAL_MAX_TASKS; ++i)
        pthread_cond_init(&s_tasks[i].changed, &ca);
    for (int i = 0; i < OSAL_MAX_TIMERS; ++i)
        pthread_cond_init(&s_timers[i].changed, &ca);
    for (int i = 0; i < OSAL_MAX_EVENTS; ++i)
        pthread_cond_init(&s_events[i].changed, &ca);
    for (int i = 0; i < OSAL_MAX_MUTEXES; ++i)
        pthread_cond_init(&s_mutexes[i].changed, &ca);
    for (int i = 0; i < OSAL_MAX_SEMS; ++i)
        pthread_cond_init(&s_sems[i].changed, &ca);
    for (int i = 0; i < OSAL_MAX_QUEUES; ++i) {
        pthread_cond_init(&s_queues[i].not_empty, &ca);
        pthread_cond_init(&s_queues[i].not_full, &ca);
    }
    pthread_condattr_destroy(&ca);
}
static void native_runtime_ensure(void) {
    pthread_once(&s_runtime_once, native_runtime_init);
}
#endif

static void global_lock(void) {
    native_runtime_ensure();
#ifdef _WIN32
    EnterCriticalSection(&s_global_cs);
#else
    pthread_mutex_lock(&s_global_mutex);
#endif
}

static void global_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&s_global_cs);
#else
    pthread_mutex_unlock(&s_global_mutex);
#endif
}

/*---------------------------------------------------------------------------*/
/* Diagnostics - Resource Statistics Tracking                                */
/*---------------------------------------------------------------------------*/

#if OSAL_STATS_ENABLE

/**
 * \brief           Resource statistics structure for internal tracking
 */
typedef struct {
    volatile uint16_t count;     /**< Current count */
    volatile uint16_t watermark; /**< Peak count (high watermark) */
} osal_resource_stats_internal_t;

/**
 * \brief           Global statistics context
 */
typedef struct {
    osal_resource_stats_internal_t tasks;   /**< Task statistics */
    osal_resource_stats_internal_t mutexes; /**< Mutex statistics */
    osal_resource_stats_internal_t sems;    /**< Semaphore statistics */
    osal_resource_stats_internal_t queues;  /**< Queue statistics */
    osal_resource_stats_internal_t events;  /**< Event flags statistics */
    osal_resource_stats_internal_t timers;  /**< Timer statistics */
} osal_global_stats_t;

/** \brief          Global statistics instance */
static osal_global_stats_t s_osal_stats = {0};

/**
 * \brief           Increment resource count and update watermark
 * \param[in]       stats: Pointer to statistics structure
 */
static inline void osal_stats_inc(osal_resource_stats_internal_t* stats) {
    global_lock();
    stats->count++;
    if (stats->count > stats->watermark) {
        stats->watermark = stats->count;
    }
    global_unlock();
}

/**
 * \brief           Decrement resource count
 * \param[in]       stats: Pointer to statistics structure
 */
static inline void osal_stats_dec(osal_resource_stats_internal_t* stats) {
    global_lock();
    if (stats->count > 0) {
        stats->count--;
    }
    global_unlock();
}

#endif /* OSAL_STATS_ENABLE */

/*---------------------------------------------------------------------------*/
/* Diagnostics - Error Callback                                              */
/*---------------------------------------------------------------------------*/

/** \brief          Registered error callback function */
static osal_error_callback_t s_error_callback = NULL;

/*---------------------------------------------------------------------------*/
/* OSAL Core Functions                                                       */
/*---------------------------------------------------------------------------*/

osal_status_t osal_init(void) {
    global_lock();
    if (s_osal_initialized) {
        global_unlock();
        return OSAL_OK;
    }
#ifdef _WIN32
    s_tls_index = TlsAlloc();
    if (s_tls_index == TLS_OUT_OF_INDEXES) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
#else
    if (pthread_key_create(&s_tls_key, NULL) != 0) {
        global_unlock();
        return OSAL_ERROR_NO_MEMORY;
    }
    s_tls_key_created = true;
#endif
    s_osal_initialized = true;
    global_unlock();
    return OSAL_OK;
}

void osal_start(void) {
    s_osal_running = true;

    /* In native platform, we don't have a real scheduler.
     * Just keep the main thread alive while tasks run. */
    while (s_osal_running) {
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }
}

bool osal_is_running(void) {
    return s_osal_running;
}

void osal_enter_critical(void) {
    native_runtime_ensure();
#ifdef _WIN32
    EnterCriticalSection(&s_critical_cs);
#else
    pthread_mutex_lock(&s_critical_mutex);
#endif
}

void osal_exit_critical(void) {
#ifdef _WIN32
    LeaveCriticalSection(&s_critical_cs);
#else
    pthread_mutex_unlock(&s_critical_mutex);
#endif
}

bool osal_is_isr(void) {
    /* Native platform doesn't have ISR context */
    return false;
}

#include "osal_native_sync.inc"
#include "osal_native_task.inc"
#include "osal_native_timer.inc"
#include "osal_native_event.inc"

osal_status_t osal_get_time_ms(uint32_t* milliseconds) {
    if (!milliseconds) return OSAL_ERROR_NULL_POINTER;
    *milliseconds = (uint32_t)native_now_ms();
    return OSAL_OK;
}

/*---------------------------------------------------------------------------*/
/* Memory Management Internal Structures                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Memory allocation header for tracking
 * \details         Stores metadata about each allocation for statistics
 * tracking
 */
typedef struct osal_mem_header {
    size_t size;                  /**< Allocated size (excluding header) */
    size_t alignment;             /**< Alignment used (0 for normal alloc) */
    void* original_ptr;           /**< Original pointer (for aligned alloc) */
    struct osal_mem_header* next; /**< Next allocation in list */
    struct osal_mem_header* prev; /**< Previous allocation in list */
    max_align_t padding; /**< Preserve malloc alignment for the returned payload */
} osal_mem_header_t;

/**
 * \brief           Memory statistics tracking structure
 */
typedef struct {
    size_t total_allocated;  /**< Total bytes currently allocated */
    size_t peak_allocated;   /**< Peak bytes allocated (watermark) */
    size_t allocation_count; /**< Number of active allocations */
#ifdef _WIN32
    CRITICAL_SECTION cs; /**< Critical section for thread safety */
#else
    pthread_mutex_t mutex; /**< Mutex for thread safety */
#endif
    osal_mem_header_t* alloc_list; /**< Linked list of allocations */
    bool initialized; /**< Whether memory tracking is initialized */
} osal_mem_stats_internal_t;

/**
 * \brief           Simulated total heap size for native platform
 * \details         This is a simulated value since native platform uses system
 * heap
 */
#define OSAL_NATIVE_HEAP_SIZE (1024 * 1024) /* 1 MB simulated heap */

static osal_mem_stats_internal_t s_mem_stats = {0};

/*---------------------------------------------------------------------------*/
/* Memory Management Helper Functions                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize memory tracking if not already done
 */
static void mem_init_tracking(void) {
    global_lock();
    s_mem_stats.initialized = true;
    global_unlock();
}

/**
 * \brief           Lock memory statistics mutex
 */
static void mem_lock(void) {
    global_lock();
    s_mem_stats.initialized = true;
}

/**
 * \brief           Unlock memory statistics mutex
 */
static void mem_unlock(void) {
    global_unlock();
}

/**
 * \brief           Track a new allocation
 * \param[in]       header: Pointer to allocation header
 * \param[in]       size: Size of allocation (excluding header)
 */
static void mem_track_alloc(osal_mem_header_t* header, size_t size) {
    header->size = size;
    header->alignment = 0;
    header->original_ptr = NULL;

    mem_lock();

    /* Add to linked list */
    header->next = s_mem_stats.alloc_list;
    header->prev = NULL;
    if (s_mem_stats.alloc_list != NULL) {
        s_mem_stats.alloc_list->prev = header;
    }
    s_mem_stats.alloc_list = header;

    /* Update statistics */
    s_mem_stats.total_allocated += size;
    s_mem_stats.allocation_count++;

    /* Update peak if necessary */
    if (s_mem_stats.total_allocated > s_mem_stats.peak_allocated) {
        s_mem_stats.peak_allocated = s_mem_stats.total_allocated;
    }

    mem_unlock();
}

/**
 * \brief           Untrack an allocation
 * \param[in]       header: Pointer to allocation header
 */
static void mem_untrack_alloc(osal_mem_header_t* header) {
    mem_lock();

    /* Remove from linked list */
    if (header->prev != NULL) {
        header->prev->next = header->next;
    } else {
        s_mem_stats.alloc_list = header->next;
    }
    if (header->next != NULL) {
        header->next->prev = header->prev;
    }

    /* Update statistics */
    s_mem_stats.total_allocated -= header->size;
    s_mem_stats.allocation_count--;

    mem_unlock();
}

/*---------------------------------------------------------------------------*/
/* Memory Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Allocate memory
 *
 * \details         Allocates memory from the system heap using malloc().
 *                  Includes tracking wrapper for statistics.
 *                  This function is thread-safe.
 *
 * \note            Requirements: 5.1-5.6
 */
void* osal_mem_alloc(size_t size) {
    /* Return NULL for zero size allocation */
    if (size == 0) {
        return NULL;
    }

    /* Allocate memory with header for tracking */
    size_t total_size = sizeof(osal_mem_header_t) + size;

    /* Check for overflow */
    if (total_size < size) {
        return NULL;
    }

    osal_mem_header_t* header = (osal_mem_header_t*)malloc(total_size);
    if (header == NULL) {
        return NULL;
    }

    /* Track the allocation */
    mem_track_alloc(header, size);

    /* Return pointer to user data (after header) */
    return (void*)(header + 1);
}

/**
 * \brief           Free memory
 *
 * \details         Frees memory back to the system heap using free().
 *                  This function is thread-safe and safe to call with NULL.
 *
 * \note            Requirements: 5.4, 5.5
 */
void osal_mem_free(void* ptr) {
    /* Safe to call with NULL - just return */
    if (ptr == NULL) {
        return;
    }

    /* Get header from user pointer */
    osal_mem_header_t* header = ((osal_mem_header_t*)ptr) - 1;

    /* Check if this is an aligned allocation */
    if (header->alignment != 0 && header->original_ptr != NULL) {
        /* For aligned allocations, free the original pointer */
        void* original = header->original_ptr;
        mem_untrack_alloc(header);
        free(original);
    } else {
        /* Normal allocation - untrack and free */
        mem_untrack_alloc(header);
        free(header);
    }
}

/**
 * \brief           Allocate and zero-initialize memory
 *
 * \details         Allocates memory from the system heap and initializes
 *                  all bytes to zero. Implemented using malloc() + memset().
 *                  This function is thread-safe.
 *
 * \note            Requirements: 6.1
 */
void* osal_mem_calloc(size_t count, size_t size) {
    /* Return NULL for zero count or size */
    if (count == 0 || size == 0) {
        return NULL;
    }

    /* Calculate total size with overflow check */
    size_t total_size = count * size;

    /* Check for multiplication overflow */
    if (total_size / count != size) {
        return NULL;
    }

    /* Allocate memory using our tracked allocator */
    void* ptr = osal_mem_alloc(total_size);
    if (ptr == NULL) {
        return NULL;
    }

    /* Zero-initialize the memory */
    memset(ptr, 0, total_size);

    return ptr;
}

/**
 * \brief           Reallocate memory
 *
 * \details         Reallocates memory, preserving the original data up to
 *                  the minimum of old and new sizes.
 *
 *                  Special cases:
 *                  - If ptr is NULL, behaves like osal_mem_alloc(size)
 *                  - If size is 0, frees the memory and returns NULL
 *
 * \note            Requirements: 6.2, 6.4, 6.5
 */
void* osal_mem_realloc(void* ptr, size_t size) {
    /* If ptr is NULL, behave like malloc */
    if (ptr == NULL) {
        return osal_mem_alloc(size);
    }

    /* If size is 0, free the memory and return NULL */
    if (size == 0) {
        osal_mem_free(ptr);
        return NULL;
    }

    /* Get header from user pointer */
    osal_mem_header_t* old_header = ((osal_mem_header_t*)ptr) - 1;
    size_t old_size = old_header->size;

    /* Allocate new memory block */
    void* new_ptr = osal_mem_alloc(size);
    if (new_ptr == NULL) {
        /* Allocation failed - original memory is unchanged */
        return NULL;
    }

    /* Copy data from old block to new block */
    size_t copy_size = (old_size < size) ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);

    /* Free the old memory block */
    osal_mem_free(ptr);

    return new_ptr;
}

/**
 * \brief           Allocate aligned memory
 *
 * \details         Allocates memory with a specific alignment requirement.
 *                  Implemented by over-allocating and adjusting the returned
 * pointer.
 *
 *                  The implementation stores the original pointer in the header
 *                  so it can be freed correctly.
 *
 * \note            Requirements: 6.3
 */
void* osal_mem_alloc_aligned(size_t alignment, size_t size) {
    /* Return NULL for zero size */
    if (size == 0) {
        return NULL;
    }

    /* Validate alignment is a power of 2 */
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    /* Ensure minimum alignment of pointer size */
    if (alignment < _Alignof(osal_mem_header_t)) {
        alignment = _Alignof(osal_mem_header_t);
    }

    /*
     * Calculate total size needed:
     * - Header size
     * - Original size
     * - Extra space for alignment adjustment (alignment - 1 bytes max)
     */
    if (alignment - 1 > SIZE_MAX - sizeof(osal_mem_header_t) ||
        size > SIZE_MAX - (sizeof(osal_mem_header_t) + alignment - 1)) {
        return NULL;
    }
    size_t total_size = sizeof(osal_mem_header_t) + size + alignment - 1;

    /* Check for overflow */
    if (total_size < size) {
        return NULL;
    }

    /* Allocate the memory block */
    void* raw_ptr = malloc(total_size);
    if (raw_ptr == NULL) {
        return NULL;
    }

    /*
     * Calculate aligned pointer:
     * 1. Start after the header
     * 2. Add (alignment - 1) and mask off low bits to align
     */
    uintptr_t raw_addr = (uintptr_t)raw_ptr + sizeof(osal_mem_header_t);
    uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);

    /* Place header just before the aligned user data */
    osal_mem_header_t* header = (osal_mem_header_t*)(aligned_addr)-1;

    /* Track the allocation with alignment info */
    header->size = size;
    header->alignment = alignment;
    header->original_ptr = raw_ptr;

    mem_lock();

    /* Add to linked list */
    header->next = s_mem_stats.alloc_list;
    header->prev = NULL;
    if (s_mem_stats.alloc_list != NULL) {
        s_mem_stats.alloc_list->prev = header;
    }
    s_mem_stats.alloc_list = header;

    /* Update statistics */
    s_mem_stats.total_allocated += size;
    s_mem_stats.allocation_count++;

    /* Update peak if necessary */
    if (s_mem_stats.total_allocated > s_mem_stats.peak_allocated) {
        s_mem_stats.peak_allocated = s_mem_stats.total_allocated;
    }

    mem_unlock();

    return (void*)aligned_addr;
}

/**
 * \brief           Get memory statistics
 *
 * \details         Retrieves memory usage statistics from the tracking system.
 *
 * \note            Requirements: 7.1-7.4
 */
osal_status_t osal_mem_get_stats(osal_mem_stats_t* stats) {
    /* Parameter validation - NULL pointer check */
    if (stats == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

    mem_init_tracking();

    mem_lock();

    /*
     * For native platform, we simulate a fixed heap size.
     * The free size is calculated as total - allocated.
     * The min_free_size is calculated from peak allocation.
     */
    stats->total_size = OSAL_NATIVE_HEAP_SIZE;
    stats->free_size = OSAL_NATIVE_HEAP_SIZE - s_mem_stats.total_allocated;
    stats->min_free_size = OSAL_NATIVE_HEAP_SIZE - s_mem_stats.peak_allocated;

    mem_unlock();

    return OSAL_OK;
}

/**
 * \brief           Get free heap size
 *
 * \details         Returns the current free heap size in bytes.
 *                  For native platform, this is simulated based on tracked
 * allocations.
 *
 * \note            Requirements: 7.2
 */
size_t osal_mem_get_free_size(void) {
    mem_init_tracking();

    mem_lock();
    size_t free_size = OSAL_NATIVE_HEAP_SIZE - s_mem_stats.total_allocated;
    mem_unlock();

    return free_size;
}

/**
 * \brief           Get minimum ever free heap size
 *
 * \details         Returns the minimum free heap size that has existed since
 *                  the system started. This is useful for detecting heap usage
 *                  high-water marks.
 *
 * \note            Requirements: 7.3
 */
size_t osal_mem_get_min_free_size(void) {
    mem_init_tracking();

    mem_lock();
    size_t min_free = OSAL_NATIVE_HEAP_SIZE - s_mem_stats.peak_allocated;
    mem_unlock();

    return min_free;
}

/**
 * \brief           Get active allocation count
 *
 * \details         Returns the number of active memory allocations.
 *                  This is tracked internally in the allocation list.
 *
 * \note            Requirements: 6.1
 */
size_t osal_mem_get_allocation_count(void) {
    mem_init_tracking();

    mem_lock();
    size_t count = s_mem_stats.allocation_count;
    mem_unlock();

    return count;
}

/**
 * \brief           Check heap integrity
 *
 * \details         Performs a basic integrity check on the tracked allocations.
 *                  Validates the linked list structure and statistics
 * consistency.
 *
 * \note            Requirements: 6.3
 */
osal_status_t osal_mem_check_integrity(void) {
    mem_init_tracking();

    mem_lock();

    /* Basic sanity checks */
    if (s_mem_stats.total_allocated > OSAL_NATIVE_HEAP_SIZE) {
        mem_unlock();
        return OSAL_ERROR;
    }

    if (s_mem_stats.peak_allocated > OSAL_NATIVE_HEAP_SIZE) {
        mem_unlock();
        return OSAL_ERROR;
    }

    if (s_mem_stats.total_allocated > s_mem_stats.peak_allocated) {
        /* Current allocation should never exceed peak */
        mem_unlock();
        return OSAL_ERROR;
    }

    /* Walk the allocation list and verify count */
    size_t counted = 0;
    size_t total_size = 0;
    osal_mem_header_t* current = s_mem_stats.alloc_list;

    while (current != NULL) {
        counted++;
        total_size += current->size;

        /* Check for list corruption (circular reference) */
        if (counted > s_mem_stats.allocation_count + 1) {
            mem_unlock();
            return OSAL_ERROR;
        }

        /* Verify prev/next consistency */
        if (current->next != NULL && current->next->prev != current) {
            mem_unlock();
            return OSAL_ERROR;
        }

        current = current->next;
    }

    /* Verify count matches */
    if (counted != s_mem_stats.allocation_count) {
        mem_unlock();
        return OSAL_ERROR;
    }

    /* Verify total size matches (approximately - aligned allocations may
     * differ) */
    /* Allow some tolerance for alignment overhead */
    if (total_size >
        s_mem_stats.total_allocated + (s_mem_stats.allocation_count * 64)) {
        mem_unlock();
        return OSAL_ERROR;
    }

    mem_unlock();
    return OSAL_OK;
}

/**
 * \brief           Free aligned memory
 *
 * \details         Frees memory that was allocated with
 * osal_mem_alloc_aligned(). Retrieves the original pointer stored in the header
 * and frees it.
 *
 * \note            Requirements: 6.4
 */
void osal_mem_free_aligned(void* ptr) {
    /* Safe to call with NULL - just return */
    if (ptr == NULL) {
        return;
    }

    /* Get header from user pointer */
    osal_mem_header_t* header = ((osal_mem_header_t*)ptr) - 1;

    /* Verify this is an aligned allocation */
    if (header->alignment == 0 || header->original_ptr == NULL) {
        /*
         * This doesn't look like an aligned allocation.
         * Fall back to regular free behavior for safety.
         */
        osal_mem_free(ptr);
        return;
    }

    /* Get the original pointer and untrack the allocation */
    void* original = header->original_ptr;
    mem_untrack_alloc(header);

    /* Free the original allocation */
    free(original);
}

/*---------------------------------------------------------------------------*/
/* Diagnostics Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get OSAL resource statistics
 *
 * \details         Retrieves current resource counts and watermarks for all
 *                  OSAL resource types. This function is safe to call from
 *                  any context.
 *
 * \note            Requirements: 2.1, 2.2, 2.3, 2.5
 */
osal_status_t osal_get_stats(osal_stats_t* stats) {
    /* Parameter validation - NULL pointer check */
    if (stats == NULL) {
        return OSAL_ERROR_NULL_POINTER;
    }

#if OSAL_STATS_ENABLE
    /* Enter critical section to ensure consistent snapshot */
    global_lock();

    /* Copy current counts */
    stats->task_count = s_osal_stats.tasks.count;
    stats->mutex_count = s_osal_stats.mutexes.count;
    stats->sem_count = s_osal_stats.sems.count;
    stats->queue_count = s_osal_stats.queues.count;
    stats->event_count = s_osal_stats.events.count;
    stats->timer_count = s_osal_stats.timers.count;

    /* Copy watermarks */
    stats->task_watermark = s_osal_stats.tasks.watermark;
    stats->mutex_watermark = s_osal_stats.mutexes.watermark;
    stats->sem_watermark = s_osal_stats.sems.watermark;
    stats->queue_watermark = s_osal_stats.queues.watermark;
    stats->event_watermark = s_osal_stats.events.watermark;
    stats->timer_watermark = s_osal_stats.timers.watermark;

    /* Copy memory statistics from memory tracking */
    mem_init_tracking();
    mem_lock();
    stats->mem_allocated = s_mem_stats.total_allocated;
    stats->mem_peak = s_mem_stats.peak_allocated;
    stats->mem_alloc_count = s_mem_stats.allocation_count;
    mem_unlock();

    global_unlock();
#else
    /* Statistics disabled - return zeros */
    memset(stats, 0, sizeof(osal_stats_t));
#endif

    return OSAL_OK;
}

/**
 * \brief           Reset OSAL statistics watermarks
 *
 * \details         Resets all watermark values to current counts. This is
 *                  useful for monitoring peak usage over specific time periods.
 *                  This function is safe to call from any context.
 *
 * \note            Requirements: 2.3
 */
osal_status_t osal_reset_stats(void) {
#if OSAL_STATS_ENABLE
    /* Enter critical section to ensure atomic reset */
    global_lock();

    /* Reset watermarks to current counts */
    s_osal_stats.tasks.watermark = s_osal_stats.tasks.count;
    s_osal_stats.mutexes.watermark = s_osal_stats.mutexes.count;
    s_osal_stats.sems.watermark = s_osal_stats.sems.count;
    s_osal_stats.queues.watermark = s_osal_stats.queues.count;
    s_osal_stats.events.watermark = s_osal_stats.events.count;
    s_osal_stats.timers.watermark = s_osal_stats.timers.count;

    /* Reset memory peak to current allocation */
    mem_init_tracking();
    mem_lock();
    s_mem_stats.peak_allocated = s_mem_stats.total_allocated;
    mem_unlock();

    global_unlock();
#endif

    return OSAL_OK;
}

/**
 * \brief           Register error callback
 *
 * \details         Registers a callback function that will be invoked when
 *                  certain errors occur. Only one callback can be registered
 *                  at a time; registering a new callback replaces the previous
 *                  one. Pass NULL to disable the callback.
 *
 * \note            Requirements: 2.5
 */
osal_status_t osal_set_error_callback(osal_error_callback_t callback) {
    /* Enter critical section for atomic update */
    global_lock();
    s_error_callback = callback;
    global_unlock();

    return OSAL_OK;
}

/**
 * \brief           Get error callback
 *
 * \details         Returns the currently registered error callback function,
 *                  or NULL if no callback is registered.
 */
osal_error_callback_t osal_get_error_callback(void) {
    global_lock();
    osal_error_callback_t cb = s_error_callback;
    global_unlock();
    return cb;
}

/**
 * \brief           Report an error through the error callback
 *
 * \details         Invokes the registered error callback if one is set.
 *                  This function is intended for internal use by OSAL
 *                  implementations to report errors.
 *
 * \note            The callback may be invoked from any context, so it
 *                  should be kept short and should not block.
 */
void osal_report_error(osal_status_t error, const char* file, uint32_t line) {
    osal_error_callback_t callback = osal_get_error_callback();

    if (callback != NULL) {
        callback(error, file, line);
    }
}
