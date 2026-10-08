/* Native mutex/semaphore/queue monitor.
 * All state and generation tokens are protected by global_lock. A condition
 * variable belongs to a static slot for the lifetime of the process, so delete
 * can wake old waiters even if that slot is immediately reused. */

typedef struct {
    uint32_t timeout_ms;
#ifdef _WIN32
    ULONGLONG expires;
#else
    struct timespec expires;
#endif
} native_deadline_t;

static native_deadline_t native_deadline(uint32_t ms) {
    native_deadline_t d;
    d.timeout_ms = ms;
#ifdef _WIN32
    d.expires = GetTickCount64() + ms;
#else
    clock_gettime(CLOCK_MONOTONIC, &d.expires);
    uint64_t ns = (uint64_t)d.expires.tv_nsec + (uint64_t)ms * 1000000u;
    d.expires.tv_sec += (time_t)(ns / 1000000000u);
    d.expires.tv_nsec = (long)(ns % 1000000000u);
#endif
    return d;
}

/* Called with exactly one global lock held. Reuses the original deadline on
 * every wake-up; notifications/spurious wakes never extend a finite wait. */
static osal_status_t native_wait(native_cond_t* cv, const native_deadline_t* d) {
    if (d->timeout_ms == OSAL_NO_WAIT)
        return OSAL_ERROR_TIMEOUT;
#ifdef _WIN32
    DWORD remaining = INFINITE;
    if (d->timeout_ms != OSAL_WAIT_FOREVER) {
        ULONGLONG now = GetTickCount64();
        if (now >= d->expires)
            return OSAL_ERROR_TIMEOUT;
        remaining = (DWORD)(d->expires - now);
    }
    if (SleepConditionVariableCS(cv, &s_global_cs, remaining))
        return OSAL_OK;
    return GetLastError() == ERROR_TIMEOUT ? OSAL_ERROR_TIMEOUT : OSAL_ERROR;
#else
    int result;
    if (d->timeout_ms == OSAL_WAIT_FOREVER) {
        result = pthread_cond_wait(cv, &s_global_mutex);
    } else {
#ifdef __APPLE__
        struct timespec now, remaining;
        clock_gettime(CLOCK_MONOTONIC, &now);
        remaining.tv_sec = d->expires.tv_sec - now.tv_sec;
        remaining.tv_nsec = d->expires.tv_nsec - now.tv_nsec;
        if (remaining.tv_nsec < 0) {
            --remaining.tv_sec;
            remaining.tv_nsec += 1000000000L;
        }
        if (remaining.tv_sec < 0)
            return OSAL_ERROR_TIMEOUT;
        result = pthread_cond_timedwait_relative_np(cv, &s_global_mutex, &remaining);
#else
        result = pthread_cond_timedwait(cv, &s_global_mutex, &d->expires);
#endif
    }
    return result == 0 ? OSAL_OK :
           result == ETIMEDOUT ? OSAL_ERROR_TIMEOUT : OSAL_ERROR;
#endif
}

static void native_notify(native_cond_t* cv) {
#ifdef _WIN32
    WakeAllConditionVariable(cv);
#else
    pthread_cond_broadcast(cv);
#endif
}

/* Tokens are opaque identifiers, never addresses to be dereferenced. A token
 * cannot alias a subsequent lifetime, even across different resource types.
 * Exhaustion fails rather than wrapping and accepting a stale handle. */
static uintptr_t s_sync_next_token = 1;
static void* native_token(osal_resource_type_t type) {
    if (s_sync_next_token > (UINTPTR_MAX >> 4))
        return NULL;
    return (void*)((s_sync_next_token++ << 4) | (uintptr_t)type);
}

#define NATIVE_LOOKUP(name, pool, limit, kind)                                  \
    static kind* name(void* handle) {                                         \
        for (size_t i = 0; i < (limit); ++i)                                   \
            if ((pool)[i].used && (pool)[i].token == handle)                    \
                return &(pool)[i];                                            \
        return NULL;                                                          \
    }
NATIVE_LOOKUP(native_mutex, s_mutexes, OSAL_MAX_MUTEXES, osal_mutex_internal_t)
NATIVE_LOOKUP(native_sem, s_sems, OSAL_MAX_SEMS, osal_sem_internal_t)
NATIVE_LOOKUP(native_queue, s_queues, OSAL_MAX_QUEUES, osal_queue_internal_t)
#undef NATIVE_LOOKUP

static native_thread_t native_current_thread(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}
static bool native_same_thread(native_thread_t a, native_thread_t b) {
#ifdef _WIN32
    return a == b;
#else
    return pthread_equal(a, b) != 0;
#endif
}

#define NATIVE_CHECK_HANDLE(handle)                                           \
    do {                                                                      \
        if (!(handle)) return OSAL_ERROR_NULL_POINTER;                         \
    } while (0)
#define NATIVE_STAT_INC(member)                                               \
    do {                                                                      \
        osal_stats_inc(&s_osal_stats.member);                                  \
    } while (0)
#define NATIVE_STAT_DEC(member)                                               \
    do {                                                                      \
        osal_stats_dec(&s_osal_stats.member);                                  \
    } while (0)
#if !OSAL_STATS_ENABLE
#undef NATIVE_STAT_INC
#undef NATIVE_STAT_DEC
#define NATIVE_STAT_INC(member) ((void)0)
#define NATIVE_STAT_DEC(member) ((void)0)
#endif

osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
    NATIVE_CHECK_HANDLE(handle);
    *handle = NULL;
    osal_status_t status = osal_init();
    if (status != OSAL_OK) return status;
    global_lock();
    for (int i = 0; i < OSAL_MAX_MUTEXES; ++i) {
        osal_mutex_internal_t* m = &s_mutexes[i];
        if (m->used) continue;
        void* token = native_token(OSAL_TYPE_MUTEX);
        if (!token) break;
        m->token = token;
        m->used = true;
        m->recursion = 0;
        m->owner = NULL;
        OSAL_HANDLE_INIT(&m->header, OSAL_TYPE_MUTEX);
        *handle = token;
        NATIVE_STAT_INC(mutexes);
        global_unlock();
        return OSAL_OK;
    }
    global_unlock();
    return OSAL_ERROR_NO_MEMORY;
}

osal_status_t osal_mutex_delete(osal_mutex_handle_t handle) {
    NATIVE_CHECK_HANDLE(handle);
    global_lock();
    osal_mutex_internal_t* m = native_mutex(handle);
    osal_status_t result = OSAL_ERROR_INVALID_PARAM;
    if (m) {
        if (m->recursion) {
            result = OSAL_ERROR_BUSY;
        } else {
            m->used = false;
            OSAL_HANDLE_DEINIT(&m->header);
            native_notify(&m->changed);
            NATIVE_STAT_DEC(mutexes);
            result = OSAL_OK;
        }
    }
    global_unlock();
    return result;
}

osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms) {
    NATIVE_CHECK_HANDLE(handle);
    native_deadline_t d = native_deadline(timeout_ms);
    native_thread_t self = native_current_thread();
    global_lock();
    osal_mutex_internal_t* m = native_mutex(handle);
    if (!m) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    while (m->recursion && (!native_same_thread(m->owner_thread, self) ||
                            m->owner != osal_task_get_current())) {
        osal_status_t wait = native_wait(&m->changed, &d);
        if (!m->used || m->token != handle) {
            global_unlock(); return OSAL_ERROR_CANCELLED;
        }
        if (wait != OSAL_OK) { global_unlock(); return wait; }
    }
    if (m->recursion == UINT32_MAX) {
        global_unlock(); return OSAL_ERROR_FULL;
    }
    ++m->recursion;
    m->owner_thread = self;
    m->owner = osal_task_get_current();
    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle) {
    NATIVE_CHECK_HANDLE(handle);
    global_lock();
    osal_mutex_internal_t* m = native_mutex(handle);
    osal_status_t result = OSAL_ERROR_INVALID_PARAM;
    if (m && m->recursion &&
        native_same_thread(m->owner_thread, native_current_thread()) &&
        m->owner == osal_task_get_current()) {
        if (--m->recursion == 0) {
            m->owner = NULL;
            native_notify(&m->changed);
        }
        result = OSAL_OK;
    }
    global_unlock();
    return result;
}

osal_task_handle_t osal_mutex_get_owner(osal_mutex_handle_t handle) {
    global_lock();
    osal_mutex_internal_t* m = native_mutex(handle);
    osal_task_handle_t owner = m ? (void*)m->owner : NULL;
    global_unlock();
    return owner;
}
bool osal_mutex_is_locked(osal_mutex_handle_t handle) {
    global_lock();
    osal_mutex_internal_t* m = native_mutex(handle);
    bool result = m && m->recursion != 0;
    global_unlock();
    return result;
}

osal_status_t osal_sem_create(uint32_t initial_count, uint32_t max_count,
                              osal_sem_handle_t* handle) {
    NATIVE_CHECK_HANDLE(handle);
    *handle = NULL;
    if (!max_count || initial_count > max_count) return OSAL_ERROR_INVALID_PARAM;
    osal_status_t status = osal_init();
    if (status != OSAL_OK) return status;
    global_lock();
    for (int i = 0; i < OSAL_MAX_SEMS; ++i) {
        osal_sem_internal_t* s = &s_sems[i];
        if (s->used) continue;
        void* token = native_token(OSAL_TYPE_SEM);
        if (!token) break;
        s->used = true;
        s->token = token;
        s->count = initial_count;
        s->max_count = max_count;
        OSAL_HANDLE_INIT(&s->header, OSAL_TYPE_SEM);
        *handle = token;
        NATIVE_STAT_INC(sems);
        global_unlock();
        return OSAL_OK;
    }
    global_unlock();
    return OSAL_ERROR_NO_MEMORY;
}
osal_status_t osal_sem_create_binary(uint32_t initial, osal_sem_handle_t* handle) {
    if (initial > 1) return OSAL_ERROR_INVALID_PARAM;
    return osal_sem_create(initial, 1, handle);
}
osal_status_t osal_sem_create_counting(uint32_t max_count, uint32_t initial,
                                       osal_sem_handle_t* handle) {
    return osal_sem_create(initial, max_count, handle);
}
osal_status_t osal_sem_delete(osal_sem_handle_t handle) {
    NATIVE_CHECK_HANDLE(handle);
    global_lock();
    osal_sem_internal_t* s = native_sem(handle);
    if (!s) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    s->used = false;
    OSAL_HANDLE_DEINIT(&s->header);
    native_notify(&s->changed);
    NATIVE_STAT_DEC(sems);
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms) {
    NATIVE_CHECK_HANDLE(handle);
    native_deadline_t d = native_deadline(timeout_ms);
    global_lock();
    osal_sem_internal_t* s = native_sem(handle);
    if (!s) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    while (!s->count) {
        osal_status_t wait = native_wait(&s->changed, &d);
        if (!s->used || s->token != handle) {
            global_unlock(); return OSAL_ERROR_CANCELLED;
        }
        if (wait != OSAL_OK) { global_unlock(); return wait; }
    }
    --s->count;
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_sem_give(osal_sem_handle_t handle) {
    NATIVE_CHECK_HANDLE(handle);
    global_lock();
    osal_sem_internal_t* s = native_sem(handle);
    if (!s) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    if (s->count == s->max_count) { global_unlock(); return OSAL_ERROR_FULL; }
    ++s->count;
    native_notify(&s->changed);
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle) {
    /* A host thread models a logical ISR event; this is not POSIX signal safe. */
    return osal_sem_give(handle);
}
uint32_t osal_sem_get_count(osal_sem_handle_t handle) {
    global_lock();
    osal_sem_internal_t* s = native_sem(handle);
    uint32_t count = s ? s->count : 0;
    global_unlock();
    return count;
}
osal_status_t osal_sem_reset(osal_sem_handle_t handle, uint32_t count) {
    NATIVE_CHECK_HANDLE(handle);
    global_lock();
    osal_sem_internal_t* s = native_sem(handle);
    if (!s || count > s->max_count) {
        global_unlock(); return OSAL_ERROR_INVALID_PARAM;
    }
    s->count = count;
    native_notify(&s->changed);
    global_unlock();
    return OSAL_OK;
}

osal_status_t osal_queue_create(size_t item_size, size_t item_count,
                                osal_queue_handle_t* handle) {
    NATIVE_CHECK_HANDLE(handle);
    *handle = NULL;
    if (!item_size || !item_count || item_size > SIZE_MAX / item_count ||
        item_size > OSAL_MAX_QUEUE_ITEM_SIZE || item_size > OSAL_MAX_QUEUE_BYTES / item_count)
        return OSAL_ERROR_INVALID_PARAM;
    osal_status_t status = osal_init();
    if (status != OSAL_OK) return status;
    global_lock();
    for (int i = 0; i < OSAL_MAX_QUEUES; ++i) {
        osal_queue_internal_t* q = &s_queues[i];
        if (q->used) continue;
        void* token = native_token(OSAL_TYPE_QUEUE);
        if (!token) break;
        uint8_t* buffer = malloc(item_size * item_count);
        if (!buffer) break;
        q->used = true;
        q->token = token;
        q->buffer = buffer;
        q->item_size = item_size;
        q->item_count = item_count;
        q->head = q->tail = q->count = 0;
        q->mode = OSAL_QUEUE_MODE_NORMAL;
        OSAL_HANDLE_INIT(&q->header, OSAL_TYPE_QUEUE);
        *handle = token;
        NATIVE_STAT_INC(queues);
        global_unlock();
        return OSAL_OK;
    }
    global_unlock();
    return OSAL_ERROR_NO_MEMORY;
}
osal_status_t osal_queue_delete(osal_queue_handle_t handle) {
    NATIVE_CHECK_HANDLE(handle);
    global_lock();
    osal_queue_internal_t* q = native_queue(handle);
    if (!q) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    q->used = false;
    free(q->buffer);
    q->buffer = NULL;
    OSAL_HANDLE_DEINIT(&q->header);
    native_notify(&q->not_empty);
    native_notify(&q->not_full);
    NATIVE_STAT_DEC(queues);
    global_unlock();
    return OSAL_OK;
}
static osal_status_t native_queue_put(osal_queue_handle_t handle, const void* item,
                                     uint32_t timeout_ms, bool front, bool overwrite) {
    NATIVE_CHECK_HANDLE(handle);
    NATIVE_CHECK_HANDLE(item);
    native_deadline_t d = native_deadline(timeout_ms);
    global_lock();
    osal_queue_internal_t* q = native_queue(handle);
    if (!q) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    while (q->count == q->item_count) {
        if (overwrite || q->mode == OSAL_QUEUE_MODE_OVERWRITE) {
            q->head = (q->head + 1) % q->item_count;
            --q->count;
            break;
        }
        if (timeout_ms == OSAL_NO_WAIT) {
            global_unlock(); return OSAL_ERROR_FULL;
        }
        osal_status_t wait = native_wait(&q->not_full, &d);
        if (!q->used || q->token != handle) {
            global_unlock(); return OSAL_ERROR_CANCELLED;
        }
        if (wait != OSAL_OK) { global_unlock(); return wait; }
    }
    size_t index;
    if (front) {
        q->head = q->head ? q->head - 1 : q->item_count - 1;
        index = q->head;
    } else {
        index = q->tail;
        q->tail = (q->tail + 1) % q->item_count;
    }
    memcpy(q->buffer + index * q->item_size, item, q->item_size);
    ++q->count;
    native_notify(&q->not_empty);
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_queue_send(osal_queue_handle_t handle, const void* item,
                              uint32_t timeout_ms) {
    return native_queue_put(handle, item, timeout_ms, false, false);
}
osal_status_t osal_queue_send_front(osal_queue_handle_t handle, const void* item,
                                    uint32_t timeout_ms) {
    return native_queue_put(handle, item, timeout_ms, true, false);
}
static osal_status_t native_queue_get(osal_queue_handle_t handle, void* item,
                                     uint32_t timeout_ms, bool peek) {
    NATIVE_CHECK_HANDLE(handle);
    NATIVE_CHECK_HANDLE(item);
    native_deadline_t d = native_deadline(timeout_ms);
    global_lock();
    osal_queue_internal_t* q = native_queue(handle);
    if (!q) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    while (!q->count) {
        if (timeout_ms == OSAL_NO_WAIT) {
            global_unlock(); return OSAL_ERROR_EMPTY;
        }
        osal_status_t wait = native_wait(&q->not_empty, &d);
        if (!q->used || q->token != handle) {
            global_unlock(); return OSAL_ERROR_CANCELLED;
        }
        if (wait != OSAL_OK) { global_unlock(); return wait; }
    }
    memcpy(item, q->buffer + q->head * q->item_size, q->item_size);
    if (!peek) {
        q->head = (q->head + 1) % q->item_count;
        --q->count;
        native_notify(&q->not_full);
    }
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_queue_receive(osal_queue_handle_t handle, void* item,
                                 uint32_t timeout_ms) {
    return native_queue_get(handle, item, timeout_ms, false);
}
osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item) {
    return native_queue_get(handle, item, OSAL_NO_WAIT, true);
}
size_t osal_queue_get_count(osal_queue_handle_t handle) {
    global_lock();
    osal_queue_internal_t* q = native_queue(handle);
    size_t count = q ? q->count : 0;
    global_unlock();
    return count;
}
bool osal_queue_is_empty(osal_queue_handle_t handle) {
    return osal_queue_get_count(handle) == 0;
}
bool osal_queue_is_full(osal_queue_handle_t handle) {
    global_lock();
    osal_queue_internal_t* q = native_queue(handle);
    bool result = q && q->count == q->item_count;
    global_unlock();
    return result;
}
size_t osal_queue_get_available_space(osal_queue_handle_t handle) {
    global_lock();
    osal_queue_internal_t* q = native_queue(handle);
    size_t count = q ? q->item_count - q->count : 0;
    global_unlock();
    return count;
}
osal_status_t osal_queue_reset(osal_queue_handle_t handle) {
    NATIVE_CHECK_HANDLE(handle);
    global_lock();
    osal_queue_internal_t* q = native_queue(handle);
    if (!q) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    q->head = q->tail = q->count = 0;
    native_notify(&q->not_full);
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_queue_set_mode(osal_queue_handle_t handle, osal_queue_mode_t mode) {
    NATIVE_CHECK_HANDLE(handle);
    if (mode != OSAL_QUEUE_MODE_NORMAL && mode != OSAL_QUEUE_MODE_OVERWRITE)
        return OSAL_ERROR_INVALID_PARAM;
    global_lock();
    osal_queue_internal_t* q = native_queue(handle);
    if (!q) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    q->mode = mode;
    native_notify(&q->not_full);
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle, const void* item) {
    return osal_queue_send(handle, item, OSAL_NO_WAIT);
}
osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle, void* item) {
    return osal_queue_receive(handle, item, OSAL_NO_WAIT);
}
osal_status_t osal_queue_peek_from_isr(osal_queue_handle_t handle, void* item) {
    return osal_queue_peek(handle, item);
}
#undef NATIVE_CHECK_HANDLE
#undef NATIVE_STAT_INC
#undef NATIVE_STAT_DEC

osal_status_t osal_queue_send_overwrite(osal_queue_handle_t handle, const void* item) {
    return native_queue_put(handle, item, 0, false, true);
}
