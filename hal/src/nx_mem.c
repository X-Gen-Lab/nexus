/**
 * \file            nx_mem.c
 * \brief           Memory management implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-17
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/system/nx_mem.h"
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Private Variables                                                         */
/*---------------------------------------------------------------------------*/

static nx_mem_mode_t g_mem_mode = NX_MEM_MODE_DYNAMIC;
static nx_mem_allocator_t* g_custom_allocator = NULL;
static nx_mem_stats_t g_mem_stats = {0};

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find first free block in bitmap
 */
static int find_free_block(uint32_t* bitmap, size_t block_count) {
    for (size_t i = 0; i < block_count; i++) {
        size_t word_idx = i / 32;
        size_t bit_idx = i % 32;

        if ((bitmap[word_idx] & (1U << bit_idx)) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * \brief           Set block as allocated in bitmap
 */
static void set_block_allocated(uint32_t* bitmap, size_t block_idx) {
    size_t word_idx = block_idx / 32;
    size_t bit_idx = block_idx % 32;
    bitmap[word_idx] |= (1U << bit_idx);
}

/**
 * \brief           Set block as free in bitmap
 */
static void set_block_free(uint32_t* bitmap, size_t block_idx) {
    size_t word_idx = block_idx / 32;
    size_t bit_idx = block_idx % 32;
    bitmap[word_idx] &= ~(1U << bit_idx);
}

/**
 * \brief           Get block index from pointer
 */
static int get_block_index(nx_mem_pool_t* pool, void* ptr) {
    if (ptr < pool->buffer) {
        return -1;
    }

    size_t offset = (uint8_t*)ptr - (uint8_t*)pool->buffer;
    if (offset % pool->block_size != 0) {
        return -1;
    }

    size_t block_idx = offset / pool->block_size;
    if (block_idx >= pool->block_count) {
        return -1;
    }

    return (int)block_idx;
}

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize memory management system
 */
void nx_mem_init(nx_mem_mode_t mode, nx_mem_allocator_t* custom) {
    g_mem_mode = mode;
    g_custom_allocator = custom;

    /* Reset statistics */
    memset(&g_mem_stats, 0, sizeof(g_mem_stats));
}

/**
 * \brief           Allocate memory
 */
void* nx_mem_alloc(size_t size) {
    void* ptr = NULL;

    switch (g_mem_mode) {
        case NX_MEM_MODE_STATIC:
            /* Static mode requires explicit pool allocation */
            g_mem_stats.fail_count++;
            return NULL;

        case NX_MEM_MODE_DYNAMIC:
            ptr = malloc(size);
            if (ptr != NULL) {
                g_mem_stats.alloc_count++;
                g_mem_stats.allocated_bytes += size;
                if (g_mem_stats.allocated_bytes > g_mem_stats.peak_bytes) {
                    g_mem_stats.peak_bytes = g_mem_stats.allocated_bytes;
                }
            } else {
                g_mem_stats.fail_count++;
            }
            break;

        case NX_MEM_MODE_CUSTOM:
            if (g_custom_allocator != NULL &&
                g_custom_allocator->alloc != NULL) {
                ptr = g_custom_allocator->alloc(size,
                                                g_custom_allocator->user_data);
                if (ptr != NULL) {
                    g_mem_stats.alloc_count++;
                    g_mem_stats.allocated_bytes += size;
                    if (g_mem_stats.allocated_bytes > g_mem_stats.peak_bytes) {
                        g_mem_stats.peak_bytes = g_mem_stats.allocated_bytes;
                    }
                } else {
                    g_mem_stats.fail_count++;
                }
            } else {
                g_mem_stats.fail_count++;
            }
            break;
    }

    return ptr;
}

/**
 * \brief           Allocate memory from specific pool
 */
void* nx_mem_alloc_from_pool(nx_mem_pool_t* pool) {
    if (pool == NULL || pool->buffer == NULL || pool->bitmap == NULL) {
        g_mem_stats.fail_count++;
        return NULL;
    }

    /* Find free block */
    int block_idx = find_free_block(pool->bitmap, pool->block_count);
    if (block_idx < 0) {
        g_mem_stats.fail_count++;
        return NULL;
    }

    /* Mark block as allocated */
    set_block_allocated(pool->bitmap, (size_t)block_idx);
    pool->allocated++;

    /* Update peak */
    if (pool->allocated > pool->peak) {
        pool->peak = pool->allocated;
    }

    /* Update global statistics */
    g_mem_stats.alloc_count++;
    g_mem_stats.allocated_bytes += pool->block_size;
    if (g_mem_stats.allocated_bytes > g_mem_stats.peak_bytes) {
        g_mem_stats.peak_bytes = g_mem_stats.allocated_bytes;
    }

    /* Return pointer to block */
    return (uint8_t*)pool->buffer + (block_idx * pool->block_size);
}

/**
 * \brief           Free allocated memory
 */
void nx_mem_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    switch (g_mem_mode) {
        case NX_MEM_MODE_STATIC:
            /* Static mode requires explicit pool free */
            break;

        case NX_MEM_MODE_DYNAMIC:
            free(ptr);
            g_mem_stats.free_count++;
            /* Note: We can't track exact size freed in dynamic mode */
            break;

        case NX_MEM_MODE_CUSTOM:
            if (g_custom_allocator != NULL &&
                g_custom_allocator->free != NULL) {
                g_custom_allocator->free(ptr, g_custom_allocator->user_data);
                g_mem_stats.free_count++;
            }
            break;
    }
}

/**
 * \brief           Free memory to specific pool
 */
void nx_mem_free_to_pool(nx_mem_pool_t* pool, void* ptr) {
    if (pool == NULL || ptr == NULL) {
        return;
    }

    /* Get block index */
    int block_idx = get_block_index(pool, ptr);
    if (block_idx < 0) {
        return;
    }

    /* Mark block as free */
    set_block_free(pool->bitmap, (size_t)block_idx);

    /* Update pool statistics */
    if (pool->allocated > 0) {
        pool->allocated--;
    }

    /* Update global statistics */
    g_mem_stats.free_count++;
    if (g_mem_stats.allocated_bytes >= pool->block_size) {
        g_mem_stats.allocated_bytes -= pool->block_size;
    }
}

/**
 * \brief           Get memory statistics
 */
nx_status_t nx_mem_get_stats(nx_mem_stats_t* stats) {
    if (stats == NULL) {
        return NX_ERR_INVALID_PARAM;
    }

    memcpy(stats, &g_mem_stats, sizeof(nx_mem_stats_t));
    return NX_OK;
}
