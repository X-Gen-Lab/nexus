/* Native task lifetime. Stop is cooperative: deleting a running task requests
 * stop and returns BUSY. No thread is forcibly terminated while owning locks. */
static osal_task_internal_t* native_task(osal_task_handle_t handle) {
    for (size_t i = 0; i < OSAL_MAX_TASKS; ++i)
        if (s_tasks[i].used && s_tasks[i].token == handle)
            return &s_tasks[i];
    return NULL;
}
static osal_task_internal_t* native_current_task(void) {
    if (!s_osal_initialized) return NULL;
#ifdef _WIN32
    return TlsGetValue(s_tls_index);
#else
    return pthread_getspecific(s_tls_key);
#endif
}
static void native_task_finish(osal_task_internal_t* task) {
    global_lock();
    task->running = false;
    task->finished = true;
    native_notify(&task->changed);
    global_unlock();
}
#ifdef _WIN32
static unsigned __stdcall task_wrapper(void* arg) {
#else
static void* task_wrapper(void* arg) {
#endif
    osal_task_internal_t* task = arg;
#ifdef _WIN32
    TlsSetValue(s_tls_index, task);
#else
    pthread_setspecific(s_tls_key, task);
#endif
    global_lock();
    task->running = true;
    global_unlock();
    /* Even a stop requested before first scheduling enters the function.
     * Cooperative workers must be able to drain accepted work and release
     * their owned resources before they return. */
    task->func(task->arg);
    native_task_finish(task);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

osal_status_t osal_task_create(const osal_task_config_t* config,
                               osal_task_handle_t* handle) {
    if (!config || !handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    if (!config->func || config->priority > 31) return OSAL_ERROR_INVALID_PARAM;
    osal_status_t init = osal_init();
    if (init != OSAL_OK) return init;
    global_lock();
    for (size_t i = 0; i < OSAL_MAX_TASKS; ++i) {
        osal_task_internal_t* t = &s_tasks[i];
        if (t->used) continue;
        void* token = native_token(OSAL_TYPE_TASK);
        if (!token) break;
        t->token = token;
        t->used = true;
        t->running = t->finished = t->suspended = t->delete_pending = t->joining = false;
        t->func = config->func;
        t->arg = config->arg;
        t->priority = config->priority;
        /* Names are optional diagnostics and deliberately bounded/truncated. */
        const char* name = config->name ? config->name : "task";
        size_t name_length = 0;
        while (name_length < sizeof(t->name) - 1 && name[name_length]) ++name_length;
        memcpy(t->name, name, name_length);
        t->name[name_length] = '\0';
        OSAL_HANDLE_INIT(&t->header, OSAL_TYPE_TASK);
#ifdef _WIN32
        t->thread = (HANDLE)_beginthreadex(NULL, (unsigned)config->stack_size,
                                          task_wrapper, t, 0, NULL);
        bool failed = !t->thread;
#else
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        int attr_result = 0;
        if (config->stack_size) {
            size_t bytes = config->stack_size;
            if (bytes < (size_t)PTHREAD_STACK_MIN) bytes = PTHREAD_STACK_MIN;
            attr_result = pthread_attr_setstacksize(&attr, bytes);
        }
        int result = attr_result ? attr_result : pthread_create(&t->thread, &attr, task_wrapper, t);
        pthread_attr_destroy(&attr);
        bool failed = result != 0;
#endif
        if (failed) {
            t->used = false;
            OSAL_HANDLE_DEINIT(&t->header);
            global_unlock();
            return OSAL_ERROR_NO_MEMORY;
        }
#if OSAL_STATS_ENABLE
        osal_stats_inc(&s_osal_stats.tasks);
#endif
        *handle = token;
        global_unlock();
        return OSAL_OK;
    }
    global_unlock();
    return OSAL_ERROR_NO_MEMORY;
}

osal_status_t osal_task_request_stop(osal_task_handle_t handle) {
    global_lock();
    osal_task_internal_t* t = handle ? native_task(handle) : native_current_task();
    if (!t || !t->used) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    t->delete_pending = true;
    t->suspended = false;
    native_notify(&t->changed);
    global_unlock();
    return OSAL_OK;
}
bool osal_task_should_stop(void) {
    global_lock();
    osal_task_internal_t* t = native_current_task();
    bool stop = t && t->delete_pending;
    global_unlock();
    return stop;
}
osal_status_t osal_task_join(osal_task_handle_t handle, uint32_t timeout_ms) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    native_deadline_t d = native_deadline(timeout_ms);
    global_lock();
    osal_task_internal_t* t = native_task(handle);
    if (!t || t == native_current_task()) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    while (!t->finished) {
        osal_status_t wait = native_wait(&t->changed, &d);
        if (!t->used || t->token != handle) {
            global_unlock(); return OSAL_ERROR_CANCELLED;
        }
        if (wait != OSAL_OK) { global_unlock(); return wait; }
    }
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_task_delete(osal_task_handle_t handle) {
    global_lock();
    osal_task_internal_t* t = handle ? native_task(handle) : native_current_task();
    if (!t || !t->used) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    if (!t->finished || t->joining) {
        t->delete_pending = true;
        t->suspended = false;
        native_notify(&t->changed);
        global_unlock();
        return OSAL_ERROR_BUSY;
    }
    t->joining = true;
#ifdef _WIN32
    HANDLE thread = t->thread;
#else
    pthread_t thread = t->thread;
#endif
    global_unlock();
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    int joined = pthread_join(thread, NULL);
    if (joined) {
        global_lock(); t->joining = false; global_unlock();
        return OSAL_ERROR;
    }
#endif
    global_lock();
    t->used = false;
    OSAL_HANDLE_DEINIT(&t->header);
    native_notify(&t->changed);
#if OSAL_STATS_ENABLE
    osal_stats_dec(&s_osal_stats.tasks);
#endif
    global_unlock();
    return OSAL_OK;
}
static osal_status_t native_task_suspend(osal_task_handle_t handle, bool suspend) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    global_lock();
    osal_task_internal_t* t = native_task(handle);
    if (!t || t->finished || t->delete_pending) {
        global_unlock(); return OSAL_ERROR_INVALID_PARAM;
    }
    t->suspended = suspend;
    native_notify(&t->changed);
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_task_suspend(osal_task_handle_t handle) {
    return native_task_suspend(handle, true);
}
osal_status_t osal_task_resume(osal_task_handle_t handle) {
    return native_task_suspend(handle, false);
}
osal_status_t osal_task_delay(uint32_t ms) {
    native_deadline_t d = native_deadline(ms);
    global_lock();
    osal_task_internal_t* t = native_current_task();
    if (!t) {
        global_unlock();
#ifdef _WIN32
        Sleep(ms);
#else
        struct timespec ts = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
        while (nanosleep(&ts, &ts) && errno == EINTR) {}
#endif
        return OSAL_OK;
    }
    /* Notifications only recheck state; the delay retains its first deadline. */
    while (!t->delete_pending && ms != OSAL_NO_WAIT) {
        osal_status_t wait = native_wait(&t->changed, &d);
        if (wait == OSAL_ERROR_TIMEOUT) break;
        if (wait != OSAL_OK) { global_unlock(); return wait; }
    }
    native_deadline_t forever = native_deadline(OSAL_WAIT_FOREVER);
    while (t->suspended && !t->delete_pending) {
        osal_status_t wait = native_wait(&t->changed, &forever);
        if (wait != OSAL_OK) { global_unlock(); return wait; }
    }
    bool stop = t->delete_pending;
    global_unlock();
    return stop ? OSAL_ERROR_CANCELLED : OSAL_OK;
}
osal_status_t osal_task_yield(void) {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
    return osal_task_should_stop() ? OSAL_ERROR_CANCELLED : OSAL_OK;
}
osal_task_handle_t osal_task_get_current(void) {
    global_lock();
    osal_task_internal_t* t = native_current_task();
    void* token = t && t->used ? t->token : NULL;
    global_unlock();
    return token;
}
const char* osal_task_get_name(osal_task_handle_t handle) {
    /* A thread-local copy remains valid if the task is deleted concurrently. */
#ifdef _MSC_VER
    static __declspec(thread) char name[OSAL_TASK_NAME_MAX];
#else
    static _Thread_local char name[OSAL_TASK_NAME_MAX];
#endif
    global_lock();
    osal_task_internal_t* t = native_task(handle);
    if (t) memcpy(name, t->name, sizeof(name));
    global_unlock();
    return t ? name : NULL;
}
uint8_t osal_task_get_priority(osal_task_handle_t handle) {
    global_lock();
    osal_task_internal_t* t = native_task(handle);
    uint8_t priority = t ? t->priority : 0;
    global_unlock();
    return priority;
}
osal_status_t osal_task_set_priority(osal_task_handle_t handle, uint8_t priority) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    if (priority > 31) return OSAL_ERROR_INVALID_PARAM;
    global_lock();
    osal_task_internal_t* t = native_task(handle);
    if (t) t->priority = priority;
    global_unlock();
    /* Native records logical priority; host scheduler priority is unchanged. */
    return t ? OSAL_OK : OSAL_ERROR_INVALID_PARAM;
}
size_t osal_task_get_stack_watermark(osal_task_handle_t handle) {
    global_lock();
    bool valid = native_task(handle) != NULL;
    global_unlock();
    return valid ? SIZE_MAX : 0;
}
osal_task_state_t osal_task_get_state(osal_task_handle_t handle) {
    global_lock();
    osal_task_internal_t* t = native_task(handle);
    osal_task_state_t state = !t || t->finished ? OSAL_TASK_STATE_DELETED :
        t->suspended ? OSAL_TASK_STATE_SUSPENDED :
        t->running ? OSAL_TASK_STATE_RUNNING : OSAL_TASK_STATE_READY;
    global_unlock();
    return state;
}
