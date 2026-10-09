static UBaseType_t rtos_priority(uint8_t priority) {
    return (UBaseType_t)((uint32_t)priority * (configMAX_PRIORITIES - 1u) / 31u);
}
static void rtos_task_entry(void* arg) {
    rtos_resource_t* r = arg;
    r->task_func(r->arg);
    UBaseType_t m = rtos_lock();
    r->finished = true;
    --r->references; /* The task owns a pin from creation through return. */
    rtos_unlock(m);
    /* Self-delete defers TCB cleanup to idle. That storage cannot be safely
     * reused merely because join observed the function return. Park instead;
     * manager delete removes this non-running task and finishes port cleanup
     * synchronously before releasing its static slot. */
    for (;;) vTaskSuspend(NULL);
}
osal_status_t osal_task_create(const osal_task_config_t* config, osal_task_handle_t* handle) {
    RTOS_BOOT_CONTEXT();
    if (!config || !handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    if (!config->func || config->priority > 31) return OSAL_ERROR_INVALID_PARAM;
    uint64_t words = ((uint64_t)config->stack_size + sizeof(StackType_t) - 1u) / sizeof(StackType_t);
    if (words < configMINIMAL_STACK_SIZE) words = configMINIMAL_STACK_SIZE;
    if (words > (uint64_t)((configSTACK_DEPTH_TYPE)-1)) return OSAL_ERROR_INVALID_PARAM;
    if (words > RTOS_STACK_WORDS) return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r = rtos_reserve(OSAL_TYPE_TASK);
    if (!r) return OSAL_ERROR_NO_MEMORY;
    r->task_func = config->func;
    r->arg = config->arg;
    r->references = 1;
    const char* name = config->name ? config->name : "task";
    strncpy(r->name, name, sizeof(r->name) - 1u);
    /* Publishing and installing the handle are atomic with respect to the new
     * task. The scheduler cannot execute an early-returning function first. */
    rtos_boot_kernel_guard_t guard = rtos_boot_kernel_enter();
    vTaskSuspendAll();
    rtos_task_storage_t* storage = &s_task_storage[r->storage_index];
    TaskHandle_t task = xTaskCreateStatic(rtos_task_entry, name, (configSTACK_DEPTH_TYPE)words,
        r, rtos_priority(config->priority), storage->stack, &storage->control);
    if (task) {
        r->kernel = task;
        rtos_publish(r, handle);
    } else rtos_abandon(r);
    xTaskResumeAll();
    rtos_boot_kernel_exit(guard);
    return task ? OSAL_OK : OSAL_ERROR_NO_MEMORY;
}
osal_task_handle_t osal_task_get_current(void) {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) return NULL;
    return rtos_task_token(xTaskGetCurrentTaskHandle());
}
osal_status_t osal_task_request_stop(osal_task_handle_t handle) {
    RTOS_TASK_ONLY();
    if (!handle) handle = osal_task_get_current();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_TASK, r);
    UBaseType_t m = rtos_lock(); r->stop_requested = true; rtos_unlock(m);
    rtos_unpin(r);
    return OSAL_OK;
}
bool osal_task_should_stop(void) {
    rtos_resource_t* r = rtos_pin(osal_task_get_current(), OSAL_TYPE_TASK);
    if (!r) return false;
    UBaseType_t m = rtos_lock(); bool stop = r->stop_requested; rtos_unlock(m);
    rtos_unpin(r);
    return stop;
}
osal_status_t osal_task_join(osal_task_handle_t handle, uint32_t ms) {
    RTOS_TASK_ONLY();
    if (handle == osal_task_get_current()) return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_TASK, r);
    TimeOut_t timeout;
    TickType_t remaining = rtos_ticks(ms);
    vTaskSetTimeOutState(&timeout);
    osal_status_t result = OSAL_ERROR_TIMEOUT;
    for (;;) {
        UBaseType_t m = rtos_lock(); bool done = r->finished; rtos_unlock(m);
        if (done) { result = OSAL_OK; break; }
        if (ms == OSAL_NO_WAIT || xTaskCheckForTimeOut(&timeout, &remaining)) break;
        vTaskDelay(1);
    }
    rtos_unpin(r);
    return result;
}
osal_status_t osal_task_delete(osal_task_handle_t handle) {
    RTOS_TASK_ONLY();
    if (!handle) handle = osal_task_get_current();
    if (!handle) return OSAL_ERROR_INVALID_PARAM;
    UBaseType_t m = rtos_lock();
    rtos_resource_t* r = rtos_find(handle, OSAL_TYPE_TASK);
    if (!r || r->closing) { rtos_unlock(m); return OSAL_ERROR_INVALID_PARAM; }
    if (!r->finished || r->references) {
        r->stop_requested = true;
        rtos_unlock(m); return OSAL_ERROR_BUSY;
    }
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i) {
        rtos_resource_t* mutex = &s_resources[i];
        if (mutex->used && !mutex->closing && mutex->type == OSAL_TYPE_MUTEX && mutex->kernel &&
            xSemaphoreGetMutexHolder(mutex->kernel) == r->kernel) {
            /* A returning worker must release its locks. Quarantine its TCB
             * instead of letting a reused static address inherit ownership. */
            rtos_unlock(m); return OSAL_ERROR_BUSY;
        }
    }
    r->closing = true;
    rtos_unlock(m);
    /* Single-core port: caller is the running task, so the completed target
     * cannot be running. vTaskDelete(other) performs port cleanup immediately. */
    vTaskDelete(r->kernel);
    r->kernel = NULL;
    rtos_reclaim(r);
    return OSAL_OK;
}
osal_status_t osal_task_suspend(osal_task_handle_t handle) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_TASK, r);
    UBaseType_t m = rtos_lock();
    TaskHandle_t task = r->finished ? NULL : r->kernel;
    if (task) vTaskSuspend(task);
    rtos_unlock(m);
    rtos_unpin(r);
    return task ? OSAL_OK : OSAL_ERROR_INVALID_PARAM;
}
osal_status_t osal_task_resume(osal_task_handle_t handle) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_TASK, r);
    UBaseType_t m = rtos_lock();
    TaskHandle_t task = r->finished ? NULL : r->kernel;
    if (task) vTaskResume(task);
    rtos_unlock(m);
    rtos_unpin(r);
    return task ? OSAL_OK : OSAL_ERROR_INVALID_PARAM;
}
osal_status_t osal_task_delay(uint32_t ms) {
    RTOS_TASK_ONLY();
    if (osal_task_should_stop()) return OSAL_ERROR_CANCELLED;
    if (!ms) {
        taskYIELD();
    } else {
        vTaskDelay(rtos_ticks(ms));
    }
    return osal_task_should_stop() ? OSAL_ERROR_CANCELLED : OSAL_OK;
}
osal_status_t osal_task_yield(void) {
    RTOS_TASK_ONLY(); taskYIELD();
    return osal_task_should_stop() ? OSAL_ERROR_CANCELLED : OSAL_OK;
}
const char* osal_task_get_name(osal_task_handle_t handle) {
    /* The returned registry string is borrowed until deletion; products copy
     * it while holding their task lifetime, rather than caching a pointer. */
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_TASK);
    if (!r) return NULL;
    const char* name = r->name;
    rtos_unpin(r);
    return name;
}
uint8_t osal_task_get_priority(osal_task_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_TASK);
    if (!r) return 0;
    UBaseType_t m = rtos_lock();
    rtos_boot_kernel_guard_t guard = rtos_boot_kernel_enter();
    UBaseType_t priority = r->kernel ? uxTaskPriorityGet(r->kernel) : 0;
    rtos_boot_kernel_exit(guard);
    rtos_unlock(m);
    rtos_unpin(r);
    return (uint8_t)(priority * 31u / (configMAX_PRIORITIES - 1u));
}
osal_status_t osal_task_set_priority(osal_task_handle_t handle, uint8_t priority) {
    RTOS_TASK_ONLY();
    if (priority > 31) return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_TASK, r);
    UBaseType_t m = rtos_lock();
    TaskHandle_t task = r->kernel;
    if (task) vTaskPrioritySet(task, rtos_priority(priority));
    rtos_unlock(m);
    rtos_unpin(r);
    return task ? OSAL_OK : OSAL_ERROR_INVALID_PARAM;
}
size_t osal_task_get_stack_watermark(osal_task_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_TASK);
    if (!r) return 0;
    UBaseType_t m = rtos_lock();
    rtos_boot_kernel_guard_t guard = rtos_boot_kernel_enter();
    size_t bytes = r->kernel ? (size_t)uxTaskGetStackHighWaterMark(r->kernel) * sizeof(StackType_t) : 0;
    rtos_boot_kernel_exit(guard);
    rtos_unlock(m);
    rtos_unpin(r);
    return bytes;
}
osal_task_state_t osal_task_get_state(osal_task_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_TASK);
    if (!r) return OSAL_TASK_STATE_DELETED;
    UBaseType_t m = rtos_lock();
    bool boot = xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED;
    eTaskState state = r->kernel && !r->finished ?
        (boot ? eReady : eTaskGetState(r->kernel)) : eDeleted;
    rtos_unlock(m);
    rtos_unpin(r);
    switch (state) {
        case eRunning: return OSAL_TASK_STATE_RUNNING;
        case eReady: return OSAL_TASK_STATE_READY;
        case eBlocked: return OSAL_TASK_STATE_BLOCKED;
        case eSuspended: return OSAL_TASK_STATE_SUSPENDED;
        default: return OSAL_TASK_STATE_DELETED;
    }
}
