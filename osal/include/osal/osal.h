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
 * \note            Disables interrupts to protect critical code sections
 */
void osal_enter_critical(void);

/**
 * \brief           Exit critical section
 * \note            Re-enables interrupts after critical code section
 */
void osal_exit_critical(void);

/**
 * \brief           Check if in ISR context
 * \return          true if in ISR, false otherwise
 * \retval          true Currently executing in ISR context
 * \retval          false Currently executing in task context
 */
bool osal_is_isr(void);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_H */
