/**
 * \file            nx_mem.h
 * \brief           Memory management interface
 * \author          Nexus Team
 */

#ifndef NX_MEM_H
#define NX_MEM_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Memory allocation mode
 */
typedef enum nx_mem_mode_e {
    NX_MEM_MODE_STATIC = 0, /**< Static memory pool allocation */
    NX_MEM_MODE_DYNAMIC,    /**< Dynamic heap allocation */
    NX_MEM_MODE_CUSTOM,     /**< Custom allocator */
} nx_mem_mode_t;

/**
 * \brief           Memory pool definition structure
 */
typedef struct nx_mem_pool_s {
    void* buffer;       /**< Pool buffer pointer */
    size_t block_size;  /**< Block size in bytes */
    size_t block_count; /**< Number of blocks */
    uint32_t* bitmap;   /**< Allocation bitmap */
    size_t allocated;   /**< Currently allocated blocks */
    size_t peak;        /**< Peak allocation count */
} nx_mem_pool_t;

/**
 * \brief           Memory statistics
 */
typedef struct nx_mem_stats_s {
    size_t total_bytes;     /**< Total available bytes */
    size_t allocated_bytes; /**< Currently allocated bytes */
    size_t peak_bytes;      /**< Peak allocation */
    uint32_t alloc_count;   /**< Total allocation count */
    uint32_t free_count;    /**< Total free count */
    uint32_t fail_count;    /**< Allocation failure count */
} nx_mem_stats_t;

/**
 * \brief           Custom allocator interface
 */
typedef struct nx_mem_allocator_s {
    void* (*alloc)(size_t size, void* user_data);
    void (*free)(void* ptr, void* user_data);
    void* user_data;
} nx_mem_allocator_t;

/**
 * \brief           Define static memory pool
 */
#define NX_MEM_POOL_DEFINE(_name, _block_size, _block_count)                   \
    static uint8_t _name##_buffer[(_block_size) * (_block_count)];             \
    static uint32_t _name##_bitmap[((_block_count) + 31) / 32];                \
    static nx_mem_pool_t _name = {                                             \
        .buffer = _name##_buffer,                                              \
        .block_size = (_block_size),                                           \
        .block_count = (_block_count),                                         \
        .bitmap = _name##_bitmap,                                              \
        .allocated = 0,                                                        \
        .peak = 0,                                                             \
    }

/**
 * \brief           Initialize memory management system
 * \param[in]       mode: Memory allocation mode
 * \param[in]       custom: Custom allocator (only for NX_MEM_MODE_CUSTOM)
 */
void nx_mem_init(nx_mem_mode_t mode, nx_mem_allocator_t* custom);

/**
 * \brief           Allocate memory
 * \param[in]       size: Size in bytes to allocate
 * \return          Pointer to allocated memory, NULL on failure
 */
void* nx_mem_alloc(size_t size);

/**
 * \brief           Allocate memory from specific pool
 * \param[in]       pool: Memory pool pointer
 * \return          Pointer to allocated memory, NULL on failure
 */
void* nx_mem_alloc_from_pool(nx_mem_pool_t* pool);

/**
 * \brief           Free allocated memory
 * \param[in]       ptr: Pointer to memory to free
 */
void nx_mem_free(void* ptr);

/**
 * \brief           Free memory to specific pool
 * \param[in]       pool: Memory pool pointer
 * \param[in]       ptr: Pointer to memory to free
 */
void nx_mem_free_to_pool(nx_mem_pool_t* pool, void* ptr);

/**
 * \brief           Get memory statistics
 * \param[out]      stats: Statistics structure to fill
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_mem_get_stats(nx_mem_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NX_MEM_H */
