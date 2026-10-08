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
 */
osal_status_t osal_init(void);

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
 *                  original interrupt mask. FreeRTOS uses its port mask.
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
 * Baremetal returns NOT_SUPPORTED until the board installs its clock. */
osal_status_t osal_get_time_ms(uint32_t* milliseconds);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_H */
