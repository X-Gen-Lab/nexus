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

/* Common definitions */
#include "osal_def.h"

/* OSAL modules */
#include "osal_mutex.h"
#include "osal_queue.h"
#include "osal_sem.h"
#include "osal_task.h"

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
 */
osal_status_t osal_init(void);

/**
 * \brief           Start OSAL scheduler
 * \note            This function does not return
 */
void osal_start(void);

/**
 * \brief           Check if scheduler is running
 * \return          true if running, false otherwise
 */
bool osal_is_running(void);

/**
 * \brief           Enter critical section
 */
void osal_enter_critical(void);

/**
 * \brief           Exit critical section
 */
void osal_exit_critical(void);

/**
 * \brief           Check if in ISR context
 * \return          true if in ISR, false otherwise
 */
bool osal_is_isr(void);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_H */
