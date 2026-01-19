/**
 * \file            nx_system_mem.h
 * \brief           System-wide memory statistics interface
 * \author          Nexus Team
 */

#ifndef NX_SYSTEM_MEM_H
#define NX_SYSTEM_MEM_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           System-wide memory statistics
 */
typedef struct nx_system_mem_stats_s {
    /* HAL memory statistics */
    size_t hal_total_bytes;
    size_t hal_allocated_bytes;
    size_t hal_peak_bytes;
    uint32_t hal_alloc_count;
    uint32_t hal_free_count;
    uint32_t hal_fail_count;

    /* OSAL memory statistics */
    size_t osal_total_bytes;
    size_t osal_allocated_bytes;
    size_t osal_peak_bytes;
    size_t osal_free_bytes;
    size_t osal_min_free_bytes;

    /* Combined statistics */
    size_t total_system_memory;
    size_t total_allocated;
    size_t total_free;
} nx_system_mem_stats_t;

/**
 * \brief           Get system-wide memory statistics
 * \param[out]      stats: Pointer to store statistics
 * \return          NX_OK on success, error code otherwise
 * \retval          NX_OK Statistics retrieved successfully
 * \retval          NX_ERR_INVALID_PARAM stats is NULL
 */
nx_status_t nx_system_get_memory_stats(nx_system_mem_stats_t* stats);

/**
 * \brief           Print memory statistics to console
 * \details         Prints formatted memory usage information for debugging
 */
void nx_system_print_memory_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_SYSTEM_MEM_H */
