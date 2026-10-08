/** HAL allocation and static pools with checked capacities and port locking.
 * Dynamic/custom allocation is a task-only management service. Control paths
 * use explicit static pools; no allocator is called inside a critical region. */
#include "hal/system/nx_mem.h"
#include "hal/system/nx_mutex.h"
#include "osal/osal.h"
#include <stdlib.h>
#include <string.h>

static nx_mem_mode_t g_mem_mode = NX_MEM_MODE_DYNAMIC;
static nx_mem_allocator_t g_custom;
static nx_mem_stats_t g_mem_stats;
static size_t g_inflight_allocations;
typedef union {
    max_align_t alignment;
    struct { size_t size; void (*free)(void*, void*); void* context; } allocation;
} nx_mem_header_t;
static void default_free(void* ptr, void* context) { (void)context; free(ptr); }
static void allocation_failed(void) {
    uint32_t saved = nx_critical_enter();
    ++g_mem_stats.fail_count;
    nx_critical_exit(saved);
}
static bool pool_valid(const nx_mem_pool_t* p) {
    if (!p || !p->buffer || !p->bitmap || !p->block_size || !p->block_count)
        return false;
    size_t words = p->block_count / 32u + (p->block_count % 32u != 0);
    return p->bitmap_words >= words && p->block_size <= p->buffer_bytes / p->block_count;
}

nx_status_t nx_mem_init(nx_mem_mode_t mode, nx_mem_allocator_t* custom) {
    if (osal_is_isr()) return NX_ERR_INVALID_STATE;
    if ((unsigned)mode > (unsigned)NX_MEM_MODE_CUSTOM ||
        (mode == NX_MEM_MODE_CUSTOM && (!custom || !custom->alloc || !custom->free)))
        return NX_ERR_INVALID_PARAM;
    uint32_t saved = nx_critical_enter();
    if (g_mem_stats.allocated_bytes || g_inflight_allocations) { nx_critical_exit(saved); return NX_ERR_BUSY; }
    g_mem_mode = mode;
    g_custom = custom ? *custom : (nx_mem_allocator_t){0};
    g_mem_stats = (nx_mem_stats_t){0};
    nx_critical_exit(saved);
    return NX_OK;
}
void* nx_mem_alloc(size_t size) {
    if (osal_is_isr() || !size || size > SIZE_MAX - sizeof(nx_mem_header_t)) {
        allocation_failed(); return NULL;
    }
    uint32_t saved = nx_critical_enter();
    nx_mem_mode_t mode = g_mem_mode;
    nx_mem_allocator_t custom = g_custom;
    if (mode != NX_MEM_MODE_STATIC) ++g_inflight_allocations;
    nx_critical_exit(saved);
    if (mode == NX_MEM_MODE_STATIC) { allocation_failed(); return NULL; }
    nx_mem_header_t* h = mode == NX_MEM_MODE_CUSTOM ?
        custom.alloc(sizeof(*h) + size, custom.user_data) : malloc(sizeof(*h) + size);
    if (!h) {
        saved = nx_critical_enter(); --g_inflight_allocations; nx_critical_exit(saved);
        allocation_failed(); return NULL;
    }
    h->allocation.size = size;
    h->allocation.free = mode == NX_MEM_MODE_CUSTOM ? custom.free : default_free;
    h->allocation.context = custom.user_data;
    saved = nx_critical_enter();
    if (size > SIZE_MAX - g_mem_stats.allocated_bytes) {
        --g_inflight_allocations;
        nx_critical_exit(saved);
        h->allocation.free(h, h->allocation.context);
        allocation_failed(); return NULL;
    }
    --g_inflight_allocations;
    ++g_mem_stats.alloc_count;
    g_mem_stats.allocated_bytes += size;
    if (g_mem_stats.allocated_bytes > g_mem_stats.peak_bytes)
        g_mem_stats.peak_bytes = g_mem_stats.allocated_bytes;
    nx_critical_exit(saved);
    return h + 1;
}
void nx_mem_free(void* ptr) {
    if (!ptr) return;
    if (osal_is_isr()) return;
    nx_mem_header_t* h = (nx_mem_header_t*)ptr - 1;
    size_t size = h->allocation.size;
    void (*release)(void*, void*) = h->allocation.free;
    void* context = h->allocation.context;
    /* Keep live-byte accounting until reclamation finishes. Allocator mode
     * reinitialization therefore cannot reset counters during a free. */
    release(h, context);
    uint32_t saved = nx_critical_enter();
    ++g_mem_stats.free_count;
    g_mem_stats.allocated_bytes -= size;
    nx_critical_exit(saved);
}
void* nx_mem_alloc_from_pool(nx_mem_pool_t* p) {
    if (!pool_valid(p)) { allocation_failed(); return NULL; }
    uint32_t saved = nx_critical_enter();
    for (size_t i = 0; i < p->block_count; ++i) {
        uint32_t mask = UINT32_C(1) << (i % 32u);
        if (p->bitmap[i / 32u] & mask) continue;
        if (p->block_size > SIZE_MAX - g_mem_stats.allocated_bytes) break;
        p->bitmap[i / 32u] |= mask;
        ++p->allocated;
        if (p->allocated > p->peak) p->peak = p->allocated;
        ++g_mem_stats.alloc_count;
        g_mem_stats.allocated_bytes += p->block_size;
        if (g_mem_stats.allocated_bytes > g_mem_stats.peak_bytes)
            g_mem_stats.peak_bytes = g_mem_stats.allocated_bytes;
        nx_critical_exit(saved);
        return (uint8_t*)p->buffer + i * p->block_size;
    }
    ++g_mem_stats.fail_count;
    nx_critical_exit(saved);
    return NULL;
}
nx_status_t nx_mem_free_to_pool(nx_mem_pool_t* p, void* ptr) {
    if (!pool_valid(p) || !ptr) return NX_ERR_INVALID_PARAM;
    uintptr_t base = (uintptr_t)p->buffer, address = (uintptr_t)ptr;
    if (address < base) return NX_ERR_INVALID_PARAM;
    uintptr_t offset = address - base;
    if (offset % p->block_size || offset / p->block_size >= p->block_count)
        return NX_ERR_INVALID_PARAM;
    size_t index = (size_t)(offset / p->block_size);
    uint32_t mask = UINT32_C(1) << (index % 32u);
    uint32_t saved = nx_critical_enter();
    if (!(p->bitmap[index / 32u] & mask)) {
        nx_critical_exit(saved); return NX_ERR_INVALID_STATE;
    }
    p->bitmap[index / 32u] &= ~mask;
    --p->allocated;
    ++g_mem_stats.free_count;
    g_mem_stats.allocated_bytes -= p->block_size;
    nx_critical_exit(saved);
    return NX_OK;
}
nx_status_t nx_mem_get_stats(nx_mem_stats_t* stats) {
    if (!stats) return NX_ERR_NULL_PTR;
    uint32_t saved = nx_critical_enter();
    *stats = g_mem_stats;
    nx_critical_exit(saved);
    return NX_OK;
}
