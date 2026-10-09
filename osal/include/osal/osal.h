/**
 * \file            osal.h
 * \brief           OSAL Main Header
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This is the main header file for the Operating System
 *                  Abstraction Layer (OSAL). Include this file to access
 *                  all OSAL modules.
 */

#ifndef OSAL_H
#define OSAL_H

/* Configuration */
#include "osal_config.h"

/* Common definitions */
#include "osal_def.h"
#include "osal_backend.h"

/* OSAL modules */
#include "osal_diag.h"
#include "osal_event.h"
#include "osal_mem.h"
#include "osal_mutex.h"
#include "osal_queue.h"
#include "osal_sem.h"
#include "osal_task.h"
#include "osal_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        OSAL Operating System Abstraction Layer
 * \brief           OS abstraction layer for Nexus platform
 * \{
 */

/**
 * \brief           Initialize OSAL layer
 * \return          OSAL_OK on success, error code otherwise
 * \retval          OSAL_OK Initialization successful
 * \retval          OSAL_ERROR Initialization failed
 * \note            FreeRTOS constructors may run before the scheduler starts
 *                  and preserve incoming masks. Operational mutex/semaphore/
 *                  queue/event/task calls require a started scheduler and
 *                  return NOT_INIT before start, including ISR calls. Execute
 *                  application services in a task; initialization alone does
 *                  not start a RTOS scheduler.
 *                  Boot task context may query objects, delete unused mutex/
 *                  semaphore/queue/event objects and use management heap APIs.
 *                  Task/timer reclaim requires scheduling/daemon settlement;
 *                  pending boot lifetimes make deinit BUSY. Product boot must
 *                  precede the bootstrap task; initialize services only in
 *                  the scheduled worker.
 */
osal_status_t osal_init(void);
/** Current backend initialization state; does not start or allocate resources. */
bool osal_is_initialized(void);

/** Task-only rollback after all producers and objects have been stopped.
 * Returns BUSY with live objects; never force-deletes them or resets lifetime
 * tokens. FreeRTOS additionally requires its scheduler not to be running.
 * Native's process-lifetime synchronization storage remains available. */
osal_status_t osal_deinit(void);

/**
 * \brief           Start OSAL scheduler
 * \note            This function does not return under normal operation
 */
void osal_start(void);

/**
 * \brief           Check if scheduler is running
 * \return          true if running, false otherwise
 * \retval          true Scheduler is running
 * \retval          false Scheduler is not running
 */
bool osal_is_running(void);

/**
 * \brief           Enter critical section
 * \note            Task context; nested calls must be balanced on the same task.
 *                  Native uses a recursive thread lock. Baremetal saves the
 *                  original interrupt mask. FreeRTOS uses saved Arch state
 *                  before scheduling and its port mask after scheduler start.
 *                  No allocation, sleep, or blocking API is allowed inside.
 *                  Use a saved-mask HAL primitive for mixed task/ISR regions.
 */
void osal_enter_critical(void);

/**
 * \brief           Exit critical section
 * \note            Restores the corresponding nested task critical region.
 */
void osal_exit_critical(void);

/**
 * \brief           Check if in ISR context
 * \return          true if in ISR, false otherwise
 * \retval          true Currently executing in ISR context
 * \retval          false Currently executing in task context
 */
bool osal_is_isr(void);

/** Read the backend's monotonic millisecond clock (modulo UINT32_MAX + 1).
 * Finite deadlines use unsigned subtraction with budgets below 2^31 ms.
 * Baremetal returns NOT_SUPPORTED until the board installs its clock.
 * FreeRTOS task-context boot reads remain valid; an ISR read before scheduler
 * start returns NOT_INIT and clears the output without entering the kernel. */
osal_status_t osal_get_time_ms(uint32_t* milliseconds);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_H */
