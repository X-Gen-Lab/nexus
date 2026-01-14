/**
 * \file            shell_history.h
 * \brief           Shell history manager interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This file defines the history manager data structures and functions
 * for storing and navigating command history.
 */

#ifndef SHELL_HISTORY_H
#define SHELL_HISTORY_H

#include "shell_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        SHELL_HISTORY History Manager
 * \brief           Command history storage and navigation
 * \{
 */

/**
 * \brief           History manager state structure
 */
typedef struct {
    char**      entries;        /**< Array of history entry pointers */
    uint16_t    entry_size;     /**< Maximum size of each entry */
    uint8_t     capacity;       /**< Maximum number of entries */
    uint8_t     count;          /**< Current number of entries */
    uint8_t     head;           /**< Index of newest entry (circular buffer) */
    int8_t      browse_index;   /**< Current browse position (-1 = current input) */
} history_manager_t;

/**
 * \brief           Initialize history manager
 * \param[in,out]   hist: Pointer to history manager structure
 * \param[in]       entries: Array of entry buffer pointers
 * \param[in]       capacity: Maximum number of history entries
 * \param[in]       entry_size: Maximum size of each entry (including null terminator)
 */
void history_init(history_manager_t* hist, char** entries, uint8_t capacity, uint16_t entry_size);

/**
 * \brief           Deinitialize history manager
 * \param[in,out]   hist: Pointer to history manager structure
 */
void history_deinit(history_manager_t* hist);

/**
 * \brief           Add command to history
 * \param[in,out]   hist: Pointer to history manager
 * \param[in]       cmd: Command string to add
 * \return          true if command was added, false if skipped (empty or duplicate)
 */
bool history_add(history_manager_t* hist, const char* cmd);

/**
 * \brief           Get previous command (Up arrow)
 * \param[in,out]   hist: Pointer to history manager
 * \return          Pointer to previous command, or NULL if at oldest
 */
const char* history_get_prev(history_manager_t* hist);

/**
 * \brief           Get next command (Down arrow)
 * \param[in,out]   hist: Pointer to history manager
 * \return          Pointer to next command, or NULL if at newest (current input)
 */
const char* history_get_next(history_manager_t* hist);

/**
 * \brief           Reset browse position
 * \param[in,out]   hist: Pointer to history manager
 */
void history_reset_browse(history_manager_t* hist);

/**
 * \brief           Get current entry count
 * \param[in]       hist: Pointer to history manager
 * \return          Number of entries in history
 */
uint8_t history_get_count(const history_manager_t* hist);

/**
 * \brief           Get entry at index
 * \param[in]       hist: Pointer to history manager
 * \param[in]       index: Index of entry (0 = most recent)
 * \return          Pointer to entry, or NULL if index out of range
 */
const char* history_get_entry(const history_manager_t* hist, uint8_t index);

/**
 * \brief           Clear all history
 * \param[in,out]   hist: Pointer to history manager
 */
void history_clear(history_manager_t* hist);

/**
 * \brief           Check if currently browsing history
 * \param[in]       hist: Pointer to history manager
 * \return          true if browsing history, false if at current input
 */
bool history_is_browsing(const history_manager_t* hist);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* SHELL_HISTORY_H */
