/* Event waits share the same monotonic deadline and generation model as
 * queues. An auto-clear wait consumes the matching bits under the monitor. */
typedef struct native_event_waiter {
    struct native_event_waiter* next;
    osal_event_bits_t wanted, matched;
    osal_event_wait_mode_t mode;
    bool auto_clear, ready, cancelled;
} native_event_waiter_t;
static osal_event_internal_t* native_event(osal_event_handle_t handle) {
    for (size_t i = 0; i < OSAL_MAX_EVENTS; ++i)
        if (s_events[i].used && s_events[i].token == handle)
            return &s_events[i];
    return NULL;
}
static bool native_event_match(osal_event_bits_t actual, osal_event_bits_t wanted,
                               osal_event_wait_mode_t mode) {
    return mode == OSAL_EVENT_WAIT_ALL ? (actual & wanted) == wanted :
                                       (actual & wanted) != 0;
}
/* Match all existing waiters against one snapshot before clearing any bits.
 * This preserves broadcast/barrier semantics for multiple auto-clear waits. */
static void native_event_dispatch(osal_event_internal_t* e) {
    osal_event_bits_t clear = 0;
    for (native_event_waiter_t* w = e->waiters; w; w = w->next) {
        if (!w->ready && native_event_match(e->bits, w->wanted, w->mode)) {
            w->matched = e->bits & w->wanted;
            w->ready = true;
            if (w->auto_clear) clear |= w->matched;
        }
    }
    e->bits &= ~clear;
    native_notify(&e->changed);
}
osal_status_t osal_event_create(osal_event_handle_t* handle) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    osal_status_t init = osal_init();
    if (init != OSAL_OK) return init;
    global_lock();
    for (size_t i = 0; i < OSAL_MAX_EVENTS; ++i) {
        osal_event_internal_t* e = &s_events[i];
        if (e->used) continue;
        void* token = native_token(OSAL_TYPE_EVENT);
        if (!token) break;
        e->token = token;
        e->used = true;
        e->bits = 0;
        e->waiters = NULL;
        OSAL_HANDLE_INIT(&e->header, OSAL_TYPE_EVENT);
#if OSAL_STATS_ENABLE
        osal_stats_inc(&s_osal_stats.events);
#endif
        *handle = token;
        global_unlock();
        return OSAL_OK;
    }
    global_unlock();
    return OSAL_ERROR_NO_MEMORY;
}
osal_status_t osal_event_delete(osal_event_handle_t handle) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    global_lock();
    osal_event_internal_t* e = native_event(handle);
    if (!e) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    for (native_event_waiter_t* w = e->waiters; w; w = w->next)
        w->cancelled = true;
    e->waiters = NULL;
    e->used = false;
    OSAL_HANDLE_DEINIT(&e->header);
    native_notify(&e->changed);
#if OSAL_STATS_ENABLE
    osal_stats_dec(&s_osal_stats.events);
#endif
    global_unlock();
    return OSAL_OK;
}
static osal_status_t native_event_update(osal_event_handle_t handle,
                                       osal_event_bits_t bits, bool set) {
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    if (!bits || (bits & 0xff000000u)) return OSAL_ERROR_INVALID_PARAM;
    global_lock();
    osal_event_internal_t* e = native_event(handle);
    if (!e) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    e->bits = set ? e->bits | bits : e->bits & ~bits;
    native_event_dispatch(e);
    global_unlock();
    return OSAL_OK;
}
osal_status_t osal_event_set(osal_event_handle_t handle, osal_event_bits_t bits) {
    return native_event_update(handle, bits, true);
}
osal_status_t osal_event_clear(osal_event_handle_t handle, osal_event_bits_t bits) {
    return native_event_update(handle, bits, false);
}
static osal_status_t native_event_wait(osal_event_handle_t handle,
                                      osal_event_bits_t set_bits,
                                      osal_event_bits_t wait_bits,
                                      const osal_event_wait_options_t* options,
                                      osal_event_bits_t* bits_out) {
    if (bits_out) *bits_out = 0;
    if (!handle || !options) return OSAL_ERROR_NULL_POINTER;
    if (!wait_bits || ((wait_bits | set_bits) & 0xff000000u) || (options->mode != OSAL_EVENT_WAIT_ANY &&
                      options->mode != OSAL_EVENT_WAIT_ALL))
        return OSAL_ERROR_INVALID_PARAM;
    native_deadline_t d = native_deadline(options->timeout_ms);
    global_lock();
    osal_event_internal_t* e = native_event(handle);
    if (!e) { global_unlock(); return OSAL_ERROR_INVALID_PARAM; }
    native_event_waiter_t w = {.next = e->waiters, .wanted = wait_bits,
        .mode = options->mode, .auto_clear = options->auto_clear};
    e->waiters = &w;
    e->bits |= set_bits;
    native_event_dispatch(e);
    osal_status_t result = OSAL_OK;
    while (!w.ready && !w.cancelled) {
        result = native_wait(&e->changed, &d);
        if (result != OSAL_OK) break;
    }
    if (w.cancelled) {
        global_unlock(); return OSAL_ERROR_CANCELLED;
    }
    /* Deletion unlinks old generation waiters before a slot can be reused. */
    native_event_waiter_t** link = (native_event_waiter_t**)&e->waiters;
    while (*link != &w) link = &(*link)->next;
    *link = w.next;
    if (bits_out) *bits_out = w.ready ? w.matched : e->bits & wait_bits;
    if (w.ready) result = OSAL_OK;
    global_unlock();
    return result;
}
osal_status_t osal_event_wait(osal_event_handle_t handle, osal_event_bits_t bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out) {
    return native_event_wait(handle, 0, bits, options, bits_out);
}
osal_event_bits_t osal_event_get(osal_event_handle_t handle) {
    global_lock();
    osal_event_internal_t* e = native_event(handle);
    osal_event_bits_t bits = e ? e->bits : 0;
    global_unlock();
    return bits;
}
osal_status_t osal_event_set_from_isr(osal_event_handle_t handle, osal_event_bits_t bits) {
    return osal_event_set(handle, bits);
}
osal_status_t osal_event_clear_from_isr(osal_event_handle_t handle, osal_event_bits_t bits) {
    return osal_event_clear(handle, bits);
}
osal_status_t osal_event_sync(osal_event_handle_t handle, osal_event_bits_t set_bits,
                              osal_event_bits_t wait_bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out) {
    if (!handle || !options) return OSAL_ERROR_NULL_POINTER;
    if (!set_bits || (options && (options->mode != OSAL_EVENT_WAIT_ALL || !options->auto_clear)))
        return OSAL_ERROR_INVALID_PARAM;
    return native_event_wait(handle, set_bits, wait_bits, options, bits_out);
}
