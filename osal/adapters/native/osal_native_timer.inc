/* Timers invoke callbacks outside the monitor. Stop/delete/callback changes
 * return BUSY while a callback owns its argument. This makes successful return
 * an ownership boundary and avoids self-join from a timer callback. */
static uint64_t native_now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}
static osal_timer_internal_t* native_timer(osal_timer_handle_t handle) {
    for (size_t i = 0; i < OSAL_MAX_TIMERS; ++i)
        if (s_timers[i].used && !s_timers[i].delete_pending &&
            s_timers[i].token == handle)
            return &s_timers[i];
    return NULL;
}
#ifdef _WIN32
static unsigned __stdcall timer_thread_func(void* arg) {
#else
static void* timer_thread_func(void* arg) {
#endif
    osal_timer_internal_t* t = arg;
    global_lock();
    native_deadline_t forever = native_deadline(OSAL_WAIT_FOREVER);
    while (!t->delete_pending) {
        while (!t->active && !t->delete_pending)
            native_wait(&t->changed, &forever);
        if (t->delete_pending) break;
        uint64_t epoch = t->epoch;
        native_deadline_t period = native_deadline(t->period_ms);
        t->expires_ms = native_now_ms() + t->period_ms;
        osal_status_t waited;
        do {
            waited = native_wait(&t->changed, &period);
        } while (waited == OSAL_OK && t->active && !t->delete_pending &&
                 t->epoch == epoch);
        if (t->delete_pending) break;
        if (!t->active || t->epoch != epoch || waited != OSAL_ERROR_TIMEOUT)
            continue;
        osal_timer_callback_t callback = t->callback;
        void* callback_arg = t->arg;
        if (t->mode == OSAL_TIMER_ONE_SHOT) t->active = false;
        t->callback_running = true;
        global_unlock();
        callback(callback_arg);
        global_lock();
        t->callback_running = false;
        native_notify(&t->changed);
    }
    global_unlock();
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
osal_status_t osal_timer_create(const osal_timer_config_t* config,
                                osal_timer_handle_t* handle) {
    if (!config || !handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    if (!config->callback || !config->period_ms ||
        config->period_ms == OSAL_WAIT_FOREVER ||
        (config->mode != OSAL_TIMER_ONE_SHOT && config->mode != OSAL_TIMER_PERIODIC))
        return OSAL_ERROR_INVALID_PARAM;
    osal_status_t init = osal_init();
    if (init != OSAL_OK) return init;
    global_lock();
    for (size_t i = 0; i < OSAL_MAX_TIMERS; ++i) {
        osal_timer_internal_t* t = &s_timers[i];
        if (t->used) continue;
        void* token = native_token(OSAL_TYPE_TIMER);
        if (!token) break;
        t->token = token;
        t->used = true;
        t->active = t->delete_pending = t->callback_running = false;
        t->epoch = t->expires_ms = 0;
        t->period_ms = config->period_ms;
        t->mode = config->mode;
        t->callback = config->callback;
        t->arg = config->arg;
        snprintf(t->name, sizeof(t->name), "%s", config->name ? config->name : "timer");
        OSAL_HANDLE_INIT(&t->header, OSAL_TYPE_TIMER);
#ifdef _WIN32
        t->thread = (HANDLE)_beginthreadex(NULL, 0, timer_thread_func, t, 0, NULL);
        bool failed = !t->thread;
#else
        bool failed = pthread_create(&t->thread, NULL, timer_thread_func, t) != 0;
#endif
        if (failed) {
            t->used = false;
            OSAL_HANDLE_DEINIT(&t->header);
            global_unlock(); return OSAL_ERROR_NO_MEMORY;
        }
#if OSAL_STATS_ENABLE
        osal_stats_inc(&s_osal_stats.timers);
#endif
        *handle = token;
        global_unlock();
        return OSAL_OK;
    }
    global_unlock();
    return OSAL_ERROR_NO_MEMORY;
}
osal_status_t osal_timer_delete(osal_timer_handle_t handle) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    global_lock();
    osal_timer_internal_t* t = native_timer(handle);
    if (!t) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    t->active = false;
    ++t->epoch;
    native_notify(&t->changed);
    if (t->callback_running) { global_unlock(); return OSAL_ERROR_BUSY; }
    t->delete_pending = true;
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
    if (pthread_join(thread, NULL)) return OSAL_ERROR;
#endif
    global_lock();
    t->used = false;
    OSAL_HANDLE_DEINIT(&t->header);
#if OSAL_STATS_ENABLE
    osal_stats_dec(&s_osal_stats.timers);
#endif
    global_unlock();
    return OSAL_OK;
}
static osal_status_t native_timer_active(osal_timer_handle_t handle, bool active) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    global_lock();
    osal_timer_internal_t* t = native_timer(handle);
    if (!t) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    t->active = active;
    ++t->epoch;
    t->expires_ms = active ? native_now_ms() + t->period_ms : 0;
    native_notify(&t->changed);
    bool busy = !active && t->callback_running;
    global_unlock();
    return busy ? OSAL_ERROR_BUSY : OSAL_OK;
}
osal_status_t osal_timer_start(osal_timer_handle_t handle) {
    return native_timer_active(handle, true);
}
osal_status_t osal_timer_stop(osal_timer_handle_t handle) {
    return native_timer_active(handle, false);
}
osal_status_t osal_timer_reset(osal_timer_handle_t handle) {
    return native_timer_active(handle, true);
}
osal_status_t osal_timer_set_period(osal_timer_handle_t handle, uint32_t period_ms) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    if (!period_ms || period_ms == OSAL_WAIT_FOREVER) return OSAL_ERROR_INVALID_PARAM;
    global_lock();
    osal_timer_internal_t* t = native_timer(handle);
    if (!t) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    t->period_ms = period_ms;
    ++t->epoch;
    if (t->active) t->expires_ms = native_now_ms() + period_ms;
    native_notify(&t->changed);
    global_unlock();
    return OSAL_OK;
}
bool osal_timer_is_active(osal_timer_handle_t handle) {
    global_lock();
    osal_timer_internal_t* t = native_timer(handle);
    bool active = t && t->active;
    global_unlock();
    return active;
}
osal_status_t osal_timer_start_from_isr(osal_timer_handle_t handle) {
    return osal_timer_start(handle);
}
osal_status_t osal_timer_stop_from_isr(osal_timer_handle_t handle) {
    return osal_timer_stop(handle);
}
osal_status_t osal_timer_reset_from_isr(osal_timer_handle_t handle) {
    return osal_timer_reset(handle);
}
uint32_t osal_timer_get_remaining(osal_timer_handle_t handle) {
    global_lock();
    osal_timer_internal_t* t = native_timer(handle);
    uint64_t now = native_now_ms();
    uint32_t remaining = t && t->active && t->expires_ms > now ?
        (uint32_t)(t->expires_ms - now) : 0;
    global_unlock();
    return remaining;
}
uint32_t osal_timer_get_period(osal_timer_handle_t handle) {
    global_lock();
    osal_timer_internal_t* t = native_timer(handle);
    uint32_t period = t ? t->period_ms : 0;
    global_unlock();
    return period;
}
osal_status_t osal_timer_set_callback(osal_timer_handle_t handle,
                                      osal_timer_callback_t callback, void* arg) {
    if (!handle || !callback) return OSAL_ERROR_NULL_POINTER;
    global_lock();
    osal_timer_internal_t* t = native_timer(handle);
    if (!t) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    if (t->callback_running) { global_unlock(); return OSAL_ERROR_BUSY; }
    t->callback = callback;
    t->arg = arg;
    global_unlock();
    return OSAL_OK;
}
