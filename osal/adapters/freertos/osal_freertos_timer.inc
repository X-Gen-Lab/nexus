static void rtos_timer_callback(TimerHandle_t timer) {
    rtos_resource_t* slot = pvTimerGetTimerID(timer);
    rtos_resource_t* r = rtos_pin(slot->token, OSAL_TYPE_TIMER);
    if (!r) return;
    UBaseType_t m = rtos_lock();
    osal_timer_callback_t callback = r->callback;
    void* arg = r->arg;
    rtos_unlock(m);
    callback(arg);
    rtos_unpin(r);
}
static void rtos_timer_ack(void* arg, uint32_t unused) {
    (void)unused;
    rtos_resource_t* r = arg;
    UBaseType_t m = rtos_lock();
    r->command_done = true;
    rtos_unlock(m);
}
static bool rtos_timer_daemon(void) {
    return osal_is_running() && xTaskGetCurrentTaskHandle() == xTimerGetTimerDaemonTaskHandle();
}
static osal_status_t rtos_timer_wait_ack(rtos_resource_t* r) {
    TimeOut_t timeout;
    TickType_t remaining = rtos_ticks(OSAL_COMMAND_TIMEOUT_MS);
    vTaskSetTimeOutState(&timeout);
    for (;;) {
        UBaseType_t m = rtos_lock(); bool done = r->command_done; rtos_unlock(m);
        if (done) return OSAL_OK;
        if (xTaskCheckForTimeOut(&timeout, &remaining)) return OSAL_ERROR_TIMEOUT;
        vTaskDelay(1);
    }
}
/* A rejected barrier enqueue retains the resource. A later management call
 * retries just that barrier before issuing another operation. Exactly one
 * acknowledgement may exist for a lifetime at a time. */
static osal_status_t rtos_timer_settle(rtos_resource_t* r) {
    UBaseType_t m = rtos_lock();
    if (r->references != 1) { rtos_unlock(m); return OSAL_ERROR_BUSY; }
    bool pending = r->command_pending;
    bool done = r->command_done;
    bool enqueued = r->ack_enqueued;
    if (pending && !done && !enqueued) r->ack_enqueued = true;
    if (pending && done) r->command_pending = false;
    rtos_unlock(m);
    if (!pending || done) return OSAL_OK;
    if (enqueued) return OSAL_ERROR_BUSY;
    if (xTimerPendFunctionCall(rtos_timer_ack, r, 0,
                               rtos_ticks(OSAL_COMMAND_TIMEOUT_MS)) != pdPASS) {
        m = rtos_lock(); r->ack_enqueued = false; rtos_unlock(m);
        return OSAL_ERROR_TIMEOUT;
    }
    osal_status_t result = rtos_timer_wait_ack(r);
    if (result == OSAL_OK) {
        m = rtos_lock(); r->command_pending = false; rtos_unlock(m);
    }
    return result;
}
/* A command barrier is executed by the timer daemon after the corresponding
 * command. Queue acceptance alone never returns buffer/callback ownership. */
static osal_status_t rtos_timer_command(rtos_resource_t* r, unsigned operation,
                                       uint32_t period_ms) {
    if (!osal_is_running() || rtos_timer_daemon()) return OSAL_ERROR_BUSY;
    osal_status_t settled = rtos_timer_settle(r);
    if (settled != OSAL_OK) return settled;
    UBaseType_t m = rtos_lock();
    if (r->command_pending) { rtos_unlock(m); return OSAL_ERROR_BUSY; }
    r->command_pending = true;
    r->command_done = false;
    r->ack_enqueued = false;
    rtos_unlock(m);
    BaseType_t result = operation == 0 ? xTimerStart(r->kernel, 0) :
                        operation == 1 ? xTimerStop(r->kernel, 0) :
                        operation == 2 ? xTimerReset(r->kernel, 0) :
                        xTimerChangePeriod(r->kernel, rtos_ticks(period_ms), 0);
    if (result != pdPASS) {
        m = rtos_lock(); r->command_pending = false; rtos_unlock(m);
        return OSAL_ERROR_FULL;
    }
    m = rtos_lock(); r->ack_enqueued = true; rtos_unlock(m);
    if (xTimerPendFunctionCall(rtos_timer_ack, r, 0,
                               rtos_ticks(OSAL_COMMAND_TIMEOUT_MS)) != pdPASS) {
        m = rtos_lock(); r->ack_enqueued = false; rtos_unlock(m);
        /* Keep the lifetime quarantined until a subsequent operation can
         * enqueue a barrier. No successful reclamation is reported. */
        return OSAL_ERROR_TIMEOUT;
    }
    osal_status_t result_status = rtos_timer_wait_ack(r);
    if (result_status == OSAL_OK) {
        m = rtos_lock(); r->command_pending = false; rtos_unlock(m);
    }
    return result_status;
}
osal_status_t osal_timer_create(const osal_timer_config_t* config, osal_timer_handle_t* handle) {
    RTOS_BOOT_CONTEXT();
    if (!config || !handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    if (!config->callback || !config->period_ms || config->period_ms == OSAL_WAIT_FOREVER ||
        (config->mode != OSAL_TIMER_ONE_SHOT && config->mode != OSAL_TIMER_PERIODIC))
        return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r = rtos_reserve(OSAL_TYPE_TIMER);
    if (!r) return OSAL_ERROR_NO_MEMORY;
    r->callback = config->callback;
    r->arg = config->arg;
    r->period_ms = config->period_ms;
    strncpy(r->name, config->name ? config->name : "timer", sizeof(r->name) - 1u);
    rtos_boot_kernel_guard_t guard = rtos_boot_kernel_enter();
    r->kernel = xTimerCreateStatic(r->name, rtos_ticks(config->period_ms),
                             config->mode == OSAL_TIMER_PERIODIC ? pdTRUE : pdFALSE,
                             r, rtos_timer_callback, &s_timer_storage[r->storage_index]);
    rtos_boot_kernel_exit(guard);
    if (!r->kernel) { rtos_abandon(r); return OSAL_ERROR_NO_MEMORY; }
    rtos_publish(r, handle);
    return OSAL_OK;
}
osal_status_t osal_timer_delete(osal_timer_handle_t handle) {
    RTOS_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    if (!osal_is_running() || rtos_timer_daemon()) return OSAL_ERROR_BUSY;
    rtos_resource_t* retry = rtos_pin(handle, OSAL_TYPE_TIMER);
    if (retry) {
        osal_status_t settled = rtos_timer_settle(retry);
        rtos_unpin(retry);
        if (settled != OSAL_OK) return settled;
    }
    UBaseType_t m = rtos_lock();
    rtos_resource_t* r = rtos_find(handle, OSAL_TYPE_TIMER);
    if (!r) { rtos_unlock(m); return OSAL_ERROR_INVALID_PARAM; }
    if (!r->closing) {
        if (r->references || r->command_pending) { rtos_unlock(m); return OSAL_ERROR_BUSY; }
        r->closing = true;
        r->command_done = false;
        r->delete_enqueued = false;
    } else if (r->references) { rtos_unlock(m); return OSAL_ERROR_BUSY; }
    /* A deleting caller owns a pin, so concurrent delete cannot also reclaim. */
    ++r->references;
    bool enqueued = r->delete_enqueued;
    bool pending = r->command_pending;
    bool done = r->command_done;
    rtos_unlock(m);
    if (!enqueued) {
        if (xTimerDelete(r->kernel, 0) != pdPASS) {
            m = rtos_lock(); r->closing = false; rtos_unlock(m);
            rtos_unpin(r); return OSAL_ERROR_FULL;
        }
        m = rtos_lock(); r->delete_enqueued = true; rtos_unlock(m);
    }
    if (!pending && !done) {
        m = rtos_lock(); r->command_pending = true; rtos_unlock(m);
        if (xTimerPendFunctionCall(rtos_timer_ack, r, 0,
                                   rtos_ticks(OSAL_COMMAND_TIMEOUT_MS)) != pdPASS) {
            m = rtos_lock(); r->command_pending = false; rtos_unlock(m);
            rtos_unpin(r); return OSAL_ERROR_TIMEOUT;
        }
    }
    osal_status_t result = rtos_timer_wait_ack(r);
    if (result != OSAL_OK) { rtos_unpin(r); return result; }
    /* The daemon has executed delete. No callback or queued command retains
     * the old kernel object, and the old token can now be invalidated. */
    m = rtos_lock();
    --r->references;
    --s_counts[r->type];
    r->used = false;
    rtos_unlock(m);
    return OSAL_OK;
}
static osal_status_t rtos_timer_set(osal_timer_handle_t handle, unsigned operation, uint32_t period_ms) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_TIMER, r);
    osal_status_t result = rtos_timer_command(r, operation, period_ms);
    if (result == OSAL_OK && operation == 3) {
        UBaseType_t m = rtos_lock(); r->period_ms = period_ms; rtos_unlock(m);
    }
    rtos_unpin(r);
    return result;
}
osal_status_t osal_timer_start(osal_timer_handle_t handle) { return rtos_timer_set(handle, 0, 0); }
osal_status_t osal_timer_stop(osal_timer_handle_t handle) { return rtos_timer_set(handle, 1, 0); }
osal_status_t osal_timer_reset(osal_timer_handle_t handle) { return rtos_timer_set(handle, 2, 0); }
osal_status_t osal_timer_set_period(osal_timer_handle_t handle, uint32_t period_ms) {
    if (!period_ms || period_ms == OSAL_WAIT_FOREVER) return OSAL_ERROR_INVALID_PARAM;
    return rtos_timer_set(handle, 3, period_ms);
}
bool osal_timer_is_active(osal_timer_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_TIMER);
    if (!r) return false;
    rtos_boot_kernel_guard_t guard = rtos_boot_kernel_enter();
    bool active = xTimerIsTimerActive(r->kernel) == pdTRUE;
    rtos_boot_kernel_exit(guard);
    rtos_unpin(r);
    return active;
}
static osal_status_t rtos_timer_isr(osal_timer_handle_t handle, unsigned operation) {
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_TIMER, r);
    UBaseType_t m = rtos_lock(); bool busy = r->command_pending; rtos_unlock(m);
    if (busy) { rtos_unpin(r); return OSAL_ERROR_BUSY; }
    BaseType_t wake = pdFALSE;
    BaseType_t result = operation == 0 ? xTimerStartFromISR(r->kernel, &wake) :
                        operation == 1 ? xTimerStopFromISR(r->kernel, &wake) :
                                         xTimerResetFromISR(r->kernel, &wake);
    rtos_unpin(r);
    portYIELD_FROM_ISR(wake);
    return result == pdPASS ? OSAL_OK : OSAL_ERROR_FULL;
}
osal_status_t osal_timer_start_from_isr(osal_timer_handle_t handle) { return rtos_timer_isr(handle, 0); }
osal_status_t osal_timer_stop_from_isr(osal_timer_handle_t handle) { return rtos_timer_isr(handle, 1); }
osal_status_t osal_timer_reset_from_isr(osal_timer_handle_t handle) { return rtos_timer_isr(handle, 2); }
uint32_t osal_timer_get_remaining(osal_timer_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_TIMER);
    if (!r) return 0;
    rtos_boot_kernel_guard_t guard = rtos_boot_kernel_enter();
    TickType_t remaining = xTimerIsTimerActive(r->kernel) ?
        xTimerGetExpiryTime(r->kernel) - xTaskGetTickCount() : 0;
    rtos_boot_kernel_exit(guard);
    uint32_t ms = rtos_milliseconds(remaining);
    rtos_unpin(r);
    return ms;
}
uint32_t osal_timer_get_period(osal_timer_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_TIMER);
    if (!r) return 0;
    UBaseType_t m = rtos_lock(); uint32_t ms = r->period_ms; rtos_unlock(m);
    rtos_unpin(r);
    return ms;
}
osal_status_t osal_timer_set_callback(osal_timer_handle_t handle, osal_timer_callback_t callback, void* arg) {
    RTOS_TASK_ONLY();
    if (!callback) return OSAL_ERROR_NULL_POINTER;
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_TIMER, r);
    UBaseType_t m = rtos_lock();
    bool busy = r->references != 1 || r->command_pending;
    if (!busy) { r->callback = callback; r->arg = arg; }
    rtos_unlock(m);
    rtos_unpin(r);
    return busy ? OSAL_ERROR_BUSY : OSAL_OK;
}
