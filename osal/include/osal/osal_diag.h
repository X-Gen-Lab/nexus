/**
 * \file            osal_diag.h
 * \brief           OSAL Diagnostics Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file provides diagnostics and statistics interfaces
 *                  for monitoring OSAL resource usage and system health.
 */

#ifndef OSAL_DIAG_H
#define OSAL_DIAG_H

#include "osal_config.h"
#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Statistics Structure                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           OSAL resource statistics structure
 * \details         Contains current counts and watermarks for all OSAL
 *                  resource types. Watermarks represent peak usage since
 *                  last reset.
 */
typedef struct {
    /*-----------------------------------------------------------------------*/
    /* Current counts                                                        */
    /*-----------------------------------------------------------------------*/
    uint16_t task_count;        /**< Active task count */
    uint16_t mutex_count;       /**< Active mutex count */
    uint16_t sem_count;         /**< Active semaphore count */
    uint16_t queue_count;       /**< Active queue count */
    uint16_t event_count;       /**< Active event flags count */
    uint16_t timer_count;       /**< Active timer count */

    /*-----------------------------------------------------------------------*/
    /* Watermarks (peak usage)                                               */
    /*-----------------------------------------------------------------------*/
    uint16_t task_watermark;    /**< Peak task count */
    uint16_t mutex_watermark;   /**< Peak mutex count */
    uint16_t sem_watermark;     /**< Peak semaphore count */
    uint16_t queue_watermark;   /**< Peak queue count */
    uint16_t event_watermark;   /**< Peak event flags count */
    uint16_t timer_watermark;   /**< Peak timer count */

    /*-----------------------------------------------------------------------*/
    /* Memory statistics                                                     */
    /*-----------------------------------------------------------------------*/
    size_t mem_allocated;       /**< Total bytes currently allocated */
    size_t mem_peak;            /**< Peak memory allocation */
    size_t mem_alloc_count;     /**< Number of active allocations */
} osal_stats_t;

/*---------------------------------------------------------------------------*/
/* Error Callback                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Error callback function type
 * \param[in]       error: Error code that occurred
 * \param[in]       file: Source file name where error occurred
 * \param[in]       line: Source line number where error occurred
 *
 * \details         This callback is invoked when certain errors occur,
 *                  such as memory corruption or stack overflow detection.
 *                  The callback should be kept short and should not block.
 */
typedef void (*osal_error_callback_t)(osal_status_t error,
                                      const char* file,
                                      uint32_t line);

/*---------------------------------------------------------------------------*/
/* Diagnostics API                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get OSAL resource statistics
 * \param[out]      stats: Pointer to store statistics
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Statistics retrieved successfully
 * \retval          OSAL_ERROR_NULL_POINTER stats is NULL
 * \note            This function is safe to call from any context,
 *                  including ISR
 */
osal_status_t osal_get_stats(osal_stats_t* stats);

/**
 * \brief           Reset OSAL statistics watermarks
 * \return          OSAL_OK on success
 * \retval          OSAL_OK Watermarks reset successfully
 * \details         Resets all watermark values to current counts.
 *                  This is useful for monitoring peak usage over
 *                  specific time periods.
 * \note            This function is safe to call from any context,
 *                  including ISR
 */
osal_status_t osal_reset_stats(void);

/**
 * \brief           Register error callback
 * \param[in]       callback: Error callback function, or NULL to disable
 * \return          OSAL_OK on success
 * \retval          OSAL_OK Callback registered successfully
 * \details         Registers a callback function that will be invoked
 *                  when certain errors occur. Only one callback can be
 *                  registered at a time; registering a new callback
 *                  replaces the previous one.
 * \note            The callback may be invoked from ISR context, so it
 *                  should be kept short and should not block
 */
osal_status_t osal_set_error_callback(osal_error_callback_t callback);

/**
 * \brief           Get error callback
 * \return          Currently registered error callback, or NULL if none
 */
osal_error_callback_t osal_get_error_callback(void);

/*---------------------------------------------------------------------------*/
/* Diagnostic Utilities                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Report an error through the error callback
 * \param[in]       error: Error code to report
 * \param[in]       file: Source file name
 * \param[in]       line: Source line number
 * \details         This function invokes the registered error callback
 *                  if one is set. It is intended for internal use by
 *                  OSAL implementations.
 */
void osal_report_error(osal_status_t error, const char* file, uint32_t line);

/**
 * \brief           Macro to report error with current file and line
 * \param[in]       error: Error code to report
 */
#define OSAL_REPORT_ERROR(error)                                            \
    osal_report_error((error), __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif /* OSAL_DIAG_H */
