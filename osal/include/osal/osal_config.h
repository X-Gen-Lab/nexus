/**
 * \file            osal_config.h
 * \brief           OSAL Configuration Options
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file contains compile-time configuration options
 *                  for the OSAL layer. Users can override these defaults
 *                  by defining the macros before including this file.
 */

#ifndef OSAL_CONFIG_H
#define OSAL_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Module Enable/Disable                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable/disable task module
 */
#ifndef OSAL_USE_TASK
#define OSAL_USE_TASK 1
#endif

/**
 * \brief           Enable/disable mutex module
 */
#ifndef OSAL_USE_MUTEX
#define OSAL_USE_MUTEX 1
#endif

/**
 * \brief           Enable/disable semaphore module
 */
#ifndef OSAL_USE_SEM
#define OSAL_USE_SEM 1
#endif

/**
 * \brief           Enable/disable queue module
 */
#ifndef OSAL_USE_QUEUE
#define OSAL_USE_QUEUE 1
#endif

/**
 * \brief           Enable/disable event flags module
 */
#ifndef OSAL_USE_EVENT
#define OSAL_USE_EVENT 1
#endif

/**
 * \brief           Enable/disable timer module
 */
#ifndef OSAL_USE_TIMER
#define OSAL_USE_TIMER 1
#endif

/**
 * \brief           Enable/disable memory module
 */
#ifndef OSAL_USE_MEM
#define OSAL_USE_MEM 1
#endif

/**
 * \brief           Enable/disable diagnostics module
 */
#ifndef OSAL_USE_DIAG
#define OSAL_USE_DIAG 1
#endif

/*---------------------------------------------------------------------------*/
/* Resource Limits                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Maximum number of tasks
 */
#ifndef OSAL_MAX_TASKS
#define OSAL_MAX_TASKS 16
#endif

/**
 * \brief           Maximum number of mutexes
 */
#ifndef OSAL_MAX_MUTEXES
#define OSAL_MAX_MUTEXES 16
#endif

/**
 * \brief           Maximum number of semaphores
 */
#ifndef OSAL_MAX_SEMS
#define OSAL_MAX_SEMS 16
#endif

/**
 * \brief           Maximum number of queues
 */
#ifndef OSAL_MAX_QUEUES
#define OSAL_MAX_QUEUES 8
#endif

/**
 * \brief           Maximum number of event flags
 */
#ifndef OSAL_MAX_EVENTS
#define OSAL_MAX_EVENTS 8
#endif

/**
 * \brief           Maximum number of timers
 */
#ifndef OSAL_MAX_TIMERS
#define OSAL_MAX_TIMERS 8
#endif

/**
 * \brief           Default task stack size in bytes
 */
#ifndef OSAL_DEFAULT_STACK_SIZE
#define OSAL_DEFAULT_STACK_SIZE 1024
#endif

/**
 * \brief           Minimum task stack size in bytes
 */
#ifndef OSAL_MIN_STACK_SIZE
#define OSAL_MIN_STACK_SIZE 256
#endif

/**
 * \brief           Maximum task priority (0 = lowest)
 */
#ifndef OSAL_MAX_PRIORITY
#define OSAL_MAX_PRIORITY 31
#endif

/*---------------------------------------------------------------------------*/
/* Debug Options                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Enable/disable debug mode
 * \details         When enabled, additional validation and checks are
 *                  performed at runtime.
 */
#ifndef OSAL_DEBUG
#define OSAL_DEBUG 0
#endif

/**
 * \brief           Enable/disable memory debug mode
 * \details         When enabled, memory allocations are tracked with
 *                  source file and line information.
 */
#ifndef OSAL_MEM_DEBUG
#define OSAL_MEM_DEBUG 0
#endif

/**
 * \brief           Enable/disable statistics collection
 */
#ifndef OSAL_STATS_ENABLE
#define OSAL_STATS_ENABLE 1
#endif

/**
 * \brief           Enable/disable handle validation
 * \details         When enabled, handles are validated using magic numbers.
 */
#ifndef OSAL_HANDLE_VALIDATION
#define OSAL_HANDLE_VALIDATION OSAL_DEBUG
#endif

/**
 * \brief           Enable/disable stack overflow detection
 */
#ifndef OSAL_STACK_CHECK
#define OSAL_STACK_CHECK OSAL_DEBUG
#endif

/*---------------------------------------------------------------------------*/
/* Handle Validation                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Magic number for handle validation
 * \details         ASCII representation of "OSAL"
 */
#ifndef OSAL_HANDLE_MAGIC
#define OSAL_HANDLE_MAGIC 0x4F53414CUL
#endif

/**
 * \brief           Invalid magic number (used after deletion)
 */
#ifndef OSAL_HANDLE_INVALID
#define OSAL_HANDLE_INVALID 0xDEADBEEFUL
#endif

/*---------------------------------------------------------------------------*/
/* Memory Configuration                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Memory allocation strategy
 * \details         0 = Use platform allocator
 *                  1 = Use static pool allocator
 */
#ifndef OSAL_MEM_STRATEGY
#define OSAL_MEM_STRATEGY 0
#endif

/**
 * \brief           Default memory alignment in bytes
 */
#ifndef OSAL_MEM_ALIGNMENT
#define OSAL_MEM_ALIGNMENT 8
#endif

/**
 * \brief           Static memory pool size (when OSAL_MEM_STRATEGY = 1)
 */
#ifndef OSAL_MEM_POOL_SIZE
#define OSAL_MEM_POOL_SIZE 4096
#endif

#ifdef __cplusplus
}
#endif

#endif /* OSAL_CONFIG_H */
