osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
    RTOS_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    rtos_resource_t* r = rtos_reserve(OSAL_TYPE_MUTEX);
    if (!r) return OSAL_ERROR_NO_MEMORY;
    r->kernel = xSemaphoreCreateRecursiveMutex();
    if (!r->kernel) { rtos_abandon(r); return OSAL_ERROR_NO_MEMORY; }
    rtos_publish(r, handle);
    return OSAL_OK;
}
osal_status_t osal_mutex_delete(osal_mutex_handle_t handle) {
    rtos_resource_t* r;
    osal_status_t result = rtos_close(handle, OSAL_TYPE_MUTEX, &r);
    if (result != OSAL_OK) return result;
    vSemaphoreDelete(r->kernel);
    rtos_reclaim(r);
    return OSAL_OK;
}
osal_status_t osal_mutex_lock(osal_mutex_handle_t handle, uint32_t timeout_ms) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_MUTEX, r);
    BaseType_t result = xSemaphoreTakeRecursive(r->kernel, rtos_ticks(timeout_ms));
    rtos_unpin(r);
    return result == pdTRUE ? OSAL_OK : OSAL_ERROR_TIMEOUT;
}
osal_status_t osal_mutex_unlock(osal_mutex_handle_t handle) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_MUTEX, r);
    BaseType_t result = xSemaphoreGiveRecursive(r->kernel);
    rtos_unpin(r);
    return result == pdTRUE ? OSAL_OK : OSAL_ERROR_INVALID_PARAM;
}
static void* rtos_task_token(TaskHandle_t task) {
    if (!task) return NULL;
    UBaseType_t m = rtos_lock();
    void* token = NULL;
    for (unsigned i = 0; i < RTOS_RESOURCE_MAX; ++i)
        if (s_resources[i].used && !s_resources[i].closing &&
            s_resources[i].type == OSAL_TYPE_TASK && s_resources[i].kernel == task) {
            token = s_resources[i].token; break;
        }
    rtos_unlock(m);
    return token;
}
osal_task_handle_t osal_mutex_get_owner(osal_mutex_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_MUTEX);
    if (!r) return NULL;
    void* owner = rtos_task_token(xSemaphoreGetMutexHolder(r->kernel));
    rtos_unpin(r);
    return owner;
}
bool osal_mutex_is_locked(osal_mutex_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_MUTEX);
    if (!r) return false;
    bool locked = xSemaphoreGetMutexHolder(r->kernel) != NULL;
    rtos_unpin(r);
    return locked;
}

osal_status_t osal_sem_create(uint32_t initial_count, uint32_t max_count,
                              osal_sem_handle_t* handle) {
    RTOS_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    if (!max_count || initial_count > max_count || (uint64_t)(UBaseType_t)max_count != (uint64_t)max_count)
        return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r = rtos_reserve(OSAL_TYPE_SEM);
    if (!r) return OSAL_ERROR_NO_MEMORY;
    r->capacity = max_count;
    r->kernel = xSemaphoreCreateCounting((UBaseType_t)max_count, (UBaseType_t)initial_count);
    if (!r->kernel) { rtos_abandon(r); return OSAL_ERROR_NO_MEMORY; }
    rtos_publish(r, handle);
    return OSAL_OK;
}
osal_status_t osal_sem_create_binary(uint32_t initial, osal_sem_handle_t* handle) {
    return osal_sem_create(initial, 1, handle);
}
osal_status_t osal_sem_create_counting(uint32_t max_count, uint32_t initial,
                                      osal_sem_handle_t* handle) {
    return osal_sem_create(initial, max_count, handle);
}
osal_status_t osal_sem_delete(osal_sem_handle_t handle) {
    rtos_resource_t* r;
    osal_status_t result = rtos_close(handle, OSAL_TYPE_SEM, &r);
    if (result != OSAL_OK) return result;
    vSemaphoreDelete(r->kernel);
    rtos_reclaim(r);
    return OSAL_OK;
}
osal_status_t osal_sem_take(osal_sem_handle_t handle, uint32_t timeout_ms) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_SEM, r);
    BaseType_t result = xSemaphoreTake(r->kernel, rtos_ticks(timeout_ms));
    rtos_unpin(r);
    return result == pdTRUE ? OSAL_OK : OSAL_ERROR_TIMEOUT;
}
osal_status_t osal_sem_give(osal_sem_handle_t handle) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_SEM, r);
    /* Release the give pin before a newly woken owner can reclaim the object.
     * This is nonblocking and defers the task yield until the port unlock. */
    UBaseType_t mask = rtos_lock();
    BaseType_t result = xSemaphoreGive(r->kernel);
    --r->references;
    rtos_unlock(mask);
    return result == pdTRUE ? OSAL_OK : OSAL_ERROR_FULL;
}
osal_status_t osal_sem_give_from_isr(osal_sem_handle_t handle) {
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_SEM, r);
    BaseType_t wake = pdFALSE;
    BaseType_t result = xSemaphoreGiveFromISR(r->kernel, &wake);
    rtos_unpin(r);
    portYIELD_FROM_ISR(wake);
    return result == pdTRUE ? OSAL_OK : OSAL_ERROR_FULL;
}
uint32_t osal_sem_get_count(osal_sem_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_SEM);
    if (!r) return 0;
    UBaseType_t count = osal_is_isr() ? uxQueueMessagesWaitingFromISR(r->kernel) :
                                     uxSemaphoreGetCount(r->kernel);
    rtos_unpin(r);
    return (uint32_t)count;
}
osal_status_t osal_sem_reset(osal_sem_handle_t handle, uint32_t count) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_SEM, r);
    if (count > r->capacity || r->capacity > 256u) {
        rtos_unpin(r); return OSAL_ERROR_INVALID_PARAM;
    }
    UBaseType_t m = rtos_lock();
    if (r->references != 1) { rtos_unlock(m); rtos_unpin(r); return OSAL_ERROR_BUSY; }
    while (xSemaphoreTake(r->kernel, 0) == pdTRUE) {}
    for (uint32_t i = 0; i < count; ++i) xSemaphoreGive(r->kernel);
    rtos_unlock(m);
    rtos_unpin(r);
    return OSAL_OK;
}

osal_status_t osal_queue_create(size_t item_size, size_t item_count,
                                osal_queue_handle_t* handle) {
    RTOS_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    if (!item_size || !item_count || item_size > SIZE_MAX / item_count ||
        item_size > OSAL_MAX_QUEUE_ITEM_SIZE || item_size > OSAL_MAX_QUEUE_BYTES / item_count ||
        item_size > (size_t)((UBaseType_t)-1) || item_count > (size_t)((UBaseType_t)-1))
        return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r = rtos_reserve(OSAL_TYPE_QUEUE);
    if (!r) return OSAL_ERROR_NO_MEMORY;
    r->kernel = xQueueCreate((UBaseType_t)item_count, (UBaseType_t)item_size);
    r->capacity = item_count;
    r->queue_mode = OSAL_QUEUE_MODE_NORMAL;
    if (!r->kernel) { rtos_abandon(r); return OSAL_ERROR_NO_MEMORY; }
    r->scratch = pvPortMalloc(item_size);
    if (!r->scratch) {
        vQueueDelete(r->kernel); rtos_abandon(r); return OSAL_ERROR_NO_MEMORY;
    }
    rtos_publish(r, handle);
    return OSAL_OK;
}
osal_status_t osal_queue_delete(osal_queue_handle_t handle) {
    rtos_resource_t* r;
    osal_status_t result = rtos_close(handle, OSAL_TYPE_QUEUE, &r);
    if (result != OSAL_OK) return result;
    vQueueDelete(r->kernel);
    vPortFree(r->scratch);
    rtos_reclaim(r);
    return OSAL_OK;
}
/* A per-queue scratch item makes dropping the oldest item deterministic.
 * The receive+send pair is atomic under the single-core kernel port mask. */
osal_status_t osal_queue_set_mode(osal_queue_handle_t handle, osal_queue_mode_t mode) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_QUEUE, r);
    osal_status_t result = OSAL_OK;
    UBaseType_t m = rtos_lock();
    if (mode != OSAL_QUEUE_MODE_NORMAL && mode != OSAL_QUEUE_MODE_OVERWRITE)
        result = OSAL_ERROR_INVALID_PARAM;
    else if (r->references != 1) result = OSAL_ERROR_BUSY;
    else r->queue_mode = mode;
    rtos_unlock(m);
    rtos_unpin(r);
    return result;
}
static osal_status_t rtos_queue_put(osal_queue_handle_t handle, const void* item,
                                   uint32_t ms, bool front, bool isr, bool force_overwrite) {
    if (!isr) RTOS_TASK_ONLY();
    if (!handle || !item) return OSAL_ERROR_NULL_POINTER;
    /* A nonblocking task send publishes data and drops its lifetime pin before
     * a woken receiver can run. This permits immediate reply-queue cleanup. */
    bool atomic_send = !isr && ms == OSAL_NO_WAIT;
    UBaseType_t send_mask = atomic_send ? rtos_lock() : 0;
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_QUEUE);
    if (!r) {
        if (atomic_send) rtos_unlock(send_mask);
        return OSAL_ERROR_INVALID_PARAM;
    }
    UBaseType_t m = rtos_lock();
    bool overwrite = force_overwrite || r->queue_mode == OSAL_QUEUE_MODE_OVERWRITE;
    rtos_unlock(m);
    BaseType_t wake = pdFALSE, result;
    if (overwrite && r->capacity > 1) {
        m = rtos_lock();
        UBaseType_t count = isr ? uxQueueMessagesWaitingFromISR(r->kernel) : uxQueueMessagesWaiting(r->kernel);
        if (count == r->capacity) {
            if (isr) xQueueReceiveFromISR(r->kernel, r->scratch, &wake);
            else xQueueReceive(r->kernel, r->scratch, 0);
        }
        if (isr) result = front ? xQueueSendToFrontFromISR(r->kernel, item, &wake) :
                                  xQueueSendToBackFromISR(r->kernel, item, &wake);
        else result = front ? xQueueSendToFront(r->kernel, item, 0) : xQueueSendToBack(r->kernel, item, 0);
        rtos_unlock(m);
    } else if (isr) result = overwrite ? xQueueOverwriteFromISR(r->kernel, item, &wake) :
        front ? xQueueSendToFrontFromISR(r->kernel, item, &wake) :
                xQueueSendToBackFromISR(r->kernel, item, &wake);
    else result = overwrite ? xQueueOverwrite(r->kernel, item) :
        front ? xQueueSendToFront(r->kernel, item, rtos_ticks(ms)) :
                xQueueSendToBack(r->kernel, item, rtos_ticks(ms));
    rtos_unpin(r);
    if (atomic_send) rtos_unlock(send_mask);
    if (isr) portYIELD_FROM_ISR(wake);
    return result == pdTRUE ? OSAL_OK : ms == OSAL_NO_WAIT ? OSAL_ERROR_FULL : OSAL_ERROR_TIMEOUT;
}
osal_status_t osal_queue_send(osal_queue_handle_t handle, const void* item, uint32_t ms) {
    return rtos_queue_put(handle, item, ms, false, false, false);
}
osal_status_t osal_queue_send_front(osal_queue_handle_t handle, const void* item, uint32_t ms) {
    return rtos_queue_put(handle, item, ms, true, false, false);
}
osal_status_t osal_queue_send_from_isr(osal_queue_handle_t handle, const void* item) {
    return rtos_queue_put(handle, item, 0, false, true, false);
}
static osal_status_t rtos_queue_get(osal_queue_handle_t handle, void* item,
                                   uint32_t ms, bool peek, bool isr) {
    if (!isr) RTOS_TASK_ONLY();
    if (!item) return OSAL_ERROR_NULL_POINTER;
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_QUEUE, r);
    BaseType_t wake = pdFALSE, result;
    if (isr) result = peek ? xQueuePeekFromISR(r->kernel, item) :
                            xQueueReceiveFromISR(r->kernel, item, &wake);
    else result = peek ? xQueuePeek(r->kernel, item, rtos_ticks(ms)) :
                         xQueueReceive(r->kernel, item, rtos_ticks(ms));
    rtos_unpin(r);
    if (isr) portYIELD_FROM_ISR(wake);
    return result == pdTRUE ? OSAL_OK : ms == OSAL_NO_WAIT ? OSAL_ERROR_EMPTY : OSAL_ERROR_TIMEOUT;
}
osal_status_t osal_queue_receive(osal_queue_handle_t handle, void* item, uint32_t ms) {
    return rtos_queue_get(handle, item, ms, false, false);
}
osal_status_t osal_queue_peek(osal_queue_handle_t handle, void* item) {
    return rtos_queue_get(handle, item, 0, true, false);
}
osal_status_t osal_queue_receive_from_isr(osal_queue_handle_t handle, void* item) {
    return rtos_queue_get(handle, item, 0, false, true);
}
osal_status_t osal_queue_peek_from_isr(osal_queue_handle_t handle, void* item) {
    return rtos_queue_get(handle, item, 0, true, true);
}
size_t osal_queue_get_count(osal_queue_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_QUEUE);
    if (!r) return 0;
    UBaseType_t count = osal_is_isr() ? uxQueueMessagesWaitingFromISR(r->kernel) :
                                     uxQueueMessagesWaiting(r->kernel);
    rtos_unpin(r);
    return count;
}
size_t osal_queue_get_available_space(osal_queue_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_QUEUE);
    if (!r) return 0;
    UBaseType_t count = osal_is_isr() ? uxQueueMessagesWaitingFromISR(r->kernel) :
                                     uxQueueMessagesWaiting(r->kernel);
    size_t space = r->capacity - count;
    rtos_unpin(r);
    return space;
}
bool osal_queue_is_empty(osal_queue_handle_t handle) { return osal_queue_get_count(handle) == 0; }
bool osal_queue_is_full(osal_queue_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_QUEUE);
    if (!r) return false;
    UBaseType_t count = osal_is_isr() ? uxQueueMessagesWaitingFromISR(r->kernel) :
                                     uxQueueMessagesWaiting(r->kernel);
    bool full = count == r->capacity;
    rtos_unpin(r);
    return full;
}
osal_status_t osal_queue_reset(osal_queue_handle_t handle) {
    RTOS_TASK_ONLY();
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_QUEUE, r);
    BaseType_t result = xQueueReset(r->kernel);
    rtos_unpin(r);
    return result == pdPASS ? OSAL_OK : OSAL_ERROR;
}

osal_status_t osal_event_create(osal_event_handle_t* handle) {
    RTOS_TASK_ONLY();
    if (!handle) return OSAL_ERROR_NULL_POINTER;
    *handle = NULL;
    rtos_resource_t* r = rtos_reserve(OSAL_TYPE_EVENT);
    if (!r) return OSAL_ERROR_NO_MEMORY;
    r->kernel = xEventGroupCreate();
    if (!r->kernel) { rtos_abandon(r); return OSAL_ERROR_NO_MEMORY; }
    rtos_publish(r, handle);
    return OSAL_OK;
}
osal_status_t osal_event_delete(osal_event_handle_t handle) {
    rtos_resource_t* r;
    osal_status_t result = rtos_close(handle, OSAL_TYPE_EVENT, &r);
    if (result != OSAL_OK) return result;
    vEventGroupDelete(r->kernel);
    rtos_reclaim(r);
    return OSAL_OK;
}
static osal_status_t rtos_event_update(osal_event_handle_t handle, osal_event_bits_t bits, bool set) {
    RTOS_TASK_ONLY();
    if (!bits || (bits & ~RTOS_EVENT_MASK)) return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_EVENT, r);
    if (set) xEventGroupSetBits(r->kernel, bits); else xEventGroupClearBits(r->kernel, bits);
    rtos_unpin(r);
    return OSAL_OK;
}
osal_status_t osal_event_set(osal_event_handle_t handle, osal_event_bits_t bits) {
    return rtos_event_update(handle, bits, true);
}
osal_status_t osal_event_clear(osal_event_handle_t handle, osal_event_bits_t bits) {
    return rtos_event_update(handle, bits, false);
}
static void rtos_event_deferred(void* arg, uint32_t operation) {
    rtos_resource_t* r = arg;
    if (operation & 0x80000000u) xEventGroupSetBits(r->kernel, operation & RTOS_EVENT_MASK);
    else xEventGroupClearBits(r->kernel, operation & RTOS_EVENT_MASK);
    rtos_unpin(r); /* Held from ISR enqueue until actual daemon execution. */
}
static osal_status_t rtos_event_from_isr(osal_event_handle_t handle, osal_event_bits_t bits, bool set) {
    if (!bits || (bits & ~RTOS_EVENT_MASK)) return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_EVENT, r);
    BaseType_t wake = pdFALSE;
    BaseType_t result = xTimerPendFunctionCallFromISR(rtos_event_deferred, r,
        bits | (set ? 0x80000000u : 0), &wake);
    if (result != pdPASS) rtos_unpin(r);
    portYIELD_FROM_ISR(wake);
    return result == pdPASS ? OSAL_OK : OSAL_ERROR_FULL;
}
osal_status_t osal_event_set_from_isr(osal_event_handle_t handle, osal_event_bits_t bits) {
    return rtos_event_from_isr(handle, bits, true);
}
osal_status_t osal_event_clear_from_isr(osal_event_handle_t handle, osal_event_bits_t bits) {
    return rtos_event_from_isr(handle, bits, false);
}
static osal_status_t rtos_event_wait(osal_event_handle_t handle, osal_event_bits_t set_bits,
                                    osal_event_bits_t bits,
                                    const osal_event_wait_options_t* options,
                                    osal_event_bits_t* bits_out) {
    RTOS_TASK_ONLY();
    if (bits_out) *bits_out = 0;
    if (!options) return OSAL_ERROR_NULL_POINTER;
    if (!bits || ((bits | set_bits) & ~RTOS_EVENT_MASK) ||
        (options->mode != OSAL_EVENT_WAIT_ALL && options->mode != OSAL_EVENT_WAIT_ANY))
        return OSAL_ERROR_INVALID_PARAM;
    rtos_resource_t* r;
    RTOS_PIN(handle, OSAL_TYPE_EVENT, r);
    EventBits_t result;
    if (set_bits) {
        if (options->mode != OSAL_EVENT_WAIT_ALL || !options->auto_clear) {
            rtos_unpin(r); return OSAL_ERROR_INVALID_PARAM;
        }
        result = xEventGroupSync(r->kernel, set_bits, bits, rtos_ticks(options->timeout_ms));
    } else result = xEventGroupWaitBits(r->kernel, bits,
        options->auto_clear ? pdTRUE : pdFALSE,
        options->mode == OSAL_EVENT_WAIT_ALL ? pdTRUE : pdFALSE,
        rtos_ticks(options->timeout_ms));
    rtos_unpin(r);
    if (bits_out) *bits_out = (uint32_t)result & bits;
    bool matched = options->mode == OSAL_EVENT_WAIT_ALL ? (result & bits) == bits : (result & bits) != 0;
    return matched ? OSAL_OK : OSAL_ERROR_TIMEOUT;
}
osal_status_t osal_event_wait(osal_event_handle_t handle, osal_event_bits_t bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out) {
    return rtos_event_wait(handle, 0, bits, options, bits_out);
}
osal_status_t osal_event_sync(osal_event_handle_t handle, osal_event_bits_t set_bits,
                              osal_event_bits_t wait_bits,
                              const osal_event_wait_options_t* options,
                              osal_event_bits_t* bits_out) {
    if (!handle || !options) return OSAL_ERROR_NULL_POINTER;
    if (!set_bits) return OSAL_ERROR_INVALID_PARAM;
    return rtos_event_wait(handle, set_bits, wait_bits, options, bits_out);
}
osal_event_bits_t osal_event_get(osal_event_handle_t handle) {
    rtos_resource_t* r = rtos_pin(handle, OSAL_TYPE_EVENT);
    if (!r) return 0;
    EventBits_t bits = osal_is_isr() ? xEventGroupGetBitsFromISR(r->kernel) : xEventGroupGetBits(r->kernel);
    rtos_unpin(r);
    return (uint32_t)bits;
}

osal_status_t osal_queue_send_overwrite(osal_queue_handle_t handle, const void* item) {
    return rtos_queue_put(handle, item, 0, false, false, true);
}
