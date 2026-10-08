/* Own allocation headers retain the original size for safe realloc and
 * checked alignment overhead. No private FreeRTOS heap layout is inspected. */
typedef union {
    max_align_t alignment;
    struct { void* raw; size_t size; } allocation;
} rtos_mem_header_t;
static void* rtos_alloc(size_t alignment, size_t size) {
    if (osal_is_isr() || !size || !alignment || (alignment & (alignment - 1u))) return NULL;
    if (alignment < _Alignof(rtos_mem_header_t)) alignment = _Alignof(rtos_mem_header_t);
    if (alignment - 1u > SIZE_MAX - sizeof(rtos_mem_header_t) ||
        size > SIZE_MAX - (sizeof(rtos_mem_header_t) + alignment - 1u)) return NULL;
    void* raw = pvPortMalloc(size + sizeof(rtos_mem_header_t) + alignment - 1u);
    if (!raw) return NULL;
    uintptr_t data = ((uintptr_t)raw + sizeof(rtos_mem_header_t) + alignment - 1u) & ~(uintptr_t)(alignment - 1u);
    rtos_mem_header_t* h = (rtos_mem_header_t*)data - 1;
    h->allocation.raw = raw;
    h->allocation.size = size;
    UBaseType_t m = rtos_lock();
    s_mem_allocated += size;
    ++s_mem_count;
    if (s_mem_allocated > s_mem_peak) s_mem_peak = s_mem_allocated;
    rtos_unlock(m);
    return (void*)data;
}
void* osal_mem_alloc(size_t size) { return rtos_alloc(_Alignof(max_align_t), size); }
void* osal_mem_alloc_aligned(size_t alignment, size_t size) { return rtos_alloc(alignment, size); }
void osal_mem_free(void* ptr) {
    if (!ptr) return;
    /* Allocation/free are task-only; invalid callers receive an error report. */
    if (osal_is_isr()) { osal_report_error(OSAL_ERROR_ISR, __FILE__, __LINE__); return; }
    rtos_mem_header_t* h = (rtos_mem_header_t*)ptr - 1;
    void* raw = h->allocation.raw;
    UBaseType_t m = rtos_lock(); s_mem_allocated -= h->allocation.size; --s_mem_count; rtos_unlock(m);
    vPortFree(raw);
}
void osal_mem_free_aligned(void* ptr) { osal_mem_free(ptr); }
void* osal_mem_calloc(size_t count, size_t size) {
    if (!count || !size || size > SIZE_MAX / count) return NULL;
    void* p = osal_mem_alloc(count * size);
    if (p) memset(p, 0, count * size);
    return p;
}
void* osal_mem_realloc(void* ptr, size_t size) {
    if (!ptr) return osal_mem_alloc(size);
    if (!size) { osal_mem_free(ptr); return NULL; }
    if (osal_is_isr()) return NULL;
    rtos_mem_header_t* h = (rtos_mem_header_t*)ptr - 1;
    size_t old = h->allocation.size;
    void* fresh = osal_mem_alloc(size);
    if (!fresh) return NULL;
    memcpy(fresh, ptr, old < size ? old : size);
    osal_mem_free(ptr);
    return fresh;
}
osal_status_t osal_mem_get_stats(osal_mem_stats_t* stats) {
    if (!stats) return OSAL_ERROR_NULL_POINTER;
    RTOS_TASK_ONLY();
    stats->total_size = configTOTAL_HEAP_SIZE;
    stats->free_size = xPortGetFreeHeapSize();
    stats->min_free_size = xPortGetMinimumEverFreeHeapSize();
    return OSAL_OK;
}
size_t osal_mem_get_free_size(void) { return xPortGetFreeHeapSize(); }
size_t osal_mem_get_min_free_size(void) { return xPortGetMinimumEverFreeHeapSize(); }
size_t osal_mem_get_allocation_count(void) {
    UBaseType_t m = rtos_lock(); size_t count = s_mem_count; rtos_unlock(m); return count;
}
osal_status_t osal_mem_check_integrity(void) {
    RTOS_TASK_ONLY();
    HeapStats_t stats;
    vPortGetHeapStats(&stats);
    return stats.xAvailableHeapSpaceInBytes <= configTOTAL_HEAP_SIZE ? OSAL_OK : OSAL_ERROR;
}
