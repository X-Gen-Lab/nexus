/**
 * \file            osal_mem.h
 * \brief           OSAL Memory Management Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef OSAL_MEM_H
#define OSAL_MEM_H

#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL_MEM Memory
 * \brief           Thread-safe dynamic memory allocation interface
 * \{
 */

/**
 * \brief           Memory statistics structure
 */
typedef struct {
    size_t total_size;    /**< Total heap size in bytes */
    size_t free_size;     /**< Current free heap size in bytes */
    size_t min_free_size; /**< Minimum ever free heap size (watermark) */
} osal_mem_stats_t;

/**
 * \brief           Allocate memory
 * \param[in]       size: Size in bytes to allocate
 * \return          Pointer to allocated memory, or NULL on failure
 * \retval          non-NULL Pointer to allocated memory
 * \retval          NULL Allocation failed or size is zero
 * \note            This function is thread-safe
 */
void* osal_mem_alloc(size_t size);

/**
 * \brief           Free memory
 * \param[in]       ptr: Pointer to memory to free (can be NULL)
 * \note            Safe to call with NULL pointer (no-op)
 * \note            This function is thread-safe
 */
void osal_mem_free(void* ptr);

/**
 * \brief           Allocate and zero-initialize memory
 * \param[in]       count: Number of elements
 * \param[in]       size: Size of each element in bytes
 * \return          Pointer to zero-initialized memory, or NULL on failure
 * \retval          non-NULL Pointer to zero-initialized memory
 * \retval          NULL Allocation failed or count/size is zero
 * \note            This function is thread-safe
 */
void* osal_mem_calloc(size_t count, size_t size);

/**
 * \brief           Reallocate memory
 * \param[in]       ptr: Pointer to existing memory (can be NULL)
 * \param[in]       size: New size in bytes
 * \return          Pointer to reallocated memory, or NULL on failure
 * \retval          non-NULL Pointer to reallocated memory
 * \retval          NULL Reallocation failed or size is zero (memory freed)
 * \note            If ptr is NULL, behaves like osal_mem_alloc
 * \note            If size is zero, frees the memory and returns NULL
 * \note            Original data is preserved up to the minimum of old and new
 *                  sizes
 * \note            This function is thread-safe
 */
void* osal_mem_realloc(void* ptr, size_t size);

/**
 * \brief           Allocate aligned memory
 * \param[in]       alignment: Alignment requirement (must be power of 2)
 * \param[in]       size: Size in bytes to allocate
 * \return          Pointer to aligned memory, or NULL on failure
 * \retval          non-NULL Pointer to aligned memory
 * \retval          NULL Allocation failed, alignment not power of 2, or
 *                       size is zero
 * \note            This function is thread-safe
 */
void* osal_mem_alloc_aligned(size_t alignment, size_t size);

/**
 * \brief           Get memory statistics
 * \param[out]      stats: Pointer to store statistics
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Statistics retrieved successfully
 * \retval          OSAL_ERROR_NULL_POINTER stats is NULL
 */
osal_status_t osal_mem_get_stats(osal_mem_stats_t* stats);

/**
 * \brief           Get free heap size
 * \return          Free heap size in bytes
 */
size_t osal_mem_get_free_size(void);

/**
 * \brief           Get minimum ever free heap size
 * \return          Minimum free heap size in bytes (watermark)
 * \note            This value represents the lowest free heap size since
 *                  system start
 */
size_t osal_mem_get_min_free_size(void);

/**
 * \brief           Get active allocation count
 * \return          Number of active memory allocations
 * \note            This function is thread-safe
 * \note            Requirements: 6.1
 */
size_t osal_mem_get_allocation_count(void);

/**
 * \brief           Check heap integrity
 * \return          OSAL_OK if heap is valid, OSAL_ERROR if corrupted
 * \retval          OSAL_OK Heap integrity check passed
 * \retval          OSAL_ERROR Heap corruption detected
 * \note            This function performs a basic integrity check on the heap
 * \note            The check may be limited depending on the underlying
 *                  memory allocator capabilities
 * \note            Requirements: 6.3
 */
osal_status_t osal_mem_check_integrity(void);

/**
 * \brief           Free aligned memory
 * \param[in]       ptr: Pointer to aligned memory allocated by
 *                       osal_mem_alloc_aligned() (can be NULL)
 * \note            Safe to call with NULL pointer (no-op)
 * \note            This function is thread-safe
 * \note            Must only be used with pointers from
 * osal_mem_alloc_aligned()
 * \note            Requirements: 6.4
 */
void osal_mem_free_aligned(void* ptr);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_MEM_H */
