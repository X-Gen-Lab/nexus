/**
 * \file            shell_history.c
 * \brief           Shell history manager implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Implements command history storage and navigation with FIFO ordering,
 * duplicate filtering, and circular buffer management.
 *
 * Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
 */

#include "shell/shell_history.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Private Helper Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Check if a command string is empty or whitespace-only
 * \param[in]       cmd: Command string to check
 * \return          true if empty or whitespace-only, false otherwise
 */
static bool
is_empty_command(const char* cmd) {
    if (cmd == NULL) {
        return true;
    }

    while (*cmd != '\0') {
        if (*cmd != ' ' && *cmd != '\t' && *cmd != '\n' && *cmd != '\r') {
            return false;
        }
        cmd++;
    }
    return true;
}

/**
 * \brief           Get the actual index in the circular buffer
 * \details         Logical index 0 = most recent entry (at head)
 *                  Logical index count-1 = oldest entry
 * \param[in]       hist: History manager instance
 * \param[in]       logical_index: Logical index (0 = most recent)
 * \return          Buffer index
 */
static uint8_t
get_buffer_index(const history_manager_t* hist, uint8_t logical_index) {
    if (hist->count == 0 || logical_index >= hist->count) {
        return 0;
    }

    /* head points to the most recent entry */
    /* For logical_index 0, we want head */
    /* For logical_index 1, we want head - 1 (with wrap) */
    int idx = (int)hist->head - (int)logical_index;
    if (idx < 0) {
        idx += hist->capacity;
    }
    return (uint8_t)idx;
}

/**
 * \brief           Get the most recent entry (for duplicate checking)
 * \param[in]       hist: History manager instance
 * \return          Most recent entry or NULL if empty
 */
static const char*
get_most_recent(const history_manager_t* hist) {
    if (hist->count == 0) {
        return NULL;
    }
    return hist->entries[hist->head];
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

void
history_init(history_manager_t* hist, char** entries,
             uint8_t capacity, uint16_t entry_size) {
    if (hist == NULL) {
        return;
    }

    hist->entries = entries;
    hist->entry_size = entry_size;
    hist->capacity = capacity;
    hist->count = 0;
    hist->head = 0;
    hist->browse_index = -1;

    /* Clear all entry buffers if provided */
    if (entries != NULL && capacity > 0 && entry_size > 0) {
        for (uint8_t i = 0; i < capacity; i++) {
            if (entries[i] != NULL) {
                entries[i][0] = '\0';
            }
        }
    }
}

void
history_deinit(history_manager_t* hist) {
    if (hist == NULL) {
        return;
    }

    hist->entries = NULL;
    hist->entry_size = 0;
    hist->capacity = 0;
    hist->count = 0;
    hist->head = 0;
    hist->browse_index = -1;
}

bool
history_add(history_manager_t* hist, const char* cmd) {
    /* Validate parameters */
    if (hist == NULL || hist->entries == NULL || hist->capacity == 0) {
        return false;
    }

    /* Requirement 5.7: Don't add empty commands */
    if (is_empty_command(cmd)) {
        return false;
    }

    /* Requirement 5.6: Don't add duplicate consecutive commands */
    const char* recent = get_most_recent(hist);
    if (recent != NULL && strcmp(recent, cmd) == 0) {
        return false;
    }

    /* Calculate next head position */
    uint8_t new_head;
    if (hist->count == 0) {
        new_head = 0;
    } else {
        new_head = (hist->head + 1) % hist->capacity;
    }

    /* Copy command to the new position */
    if (hist->entries[new_head] == NULL) {
        return false;
    }

    size_t cmd_len = strlen(cmd);
    if (cmd_len >= hist->entry_size) {
        cmd_len = hist->entry_size - 1;
    }
    memcpy(hist->entries[new_head], cmd, cmd_len);
    hist->entries[new_head][cmd_len] = '\0';

    /* Update head */
    hist->head = new_head;

    /* Requirement 5.5: FIFO - when full, oldest is overwritten */
    if (hist->count < hist->capacity) {
        hist->count++;
    }

    /* Reset browse position after adding */
    hist->browse_index = -1;

    return true;
}

const char*
history_get_prev(history_manager_t* hist) {
    if (hist == NULL || hist->entries == NULL || hist->count == 0) {
        return NULL;
    }

    /* Calculate next browse index (going backward in time = older) */
    int8_t next_index;
    if (hist->browse_index < 0) {
        /* Currently at current input, go to most recent history */
        next_index = 0;
    } else {
        /* Go to older entry */
        next_index = hist->browse_index + 1;
    }

    /* Check if we've reached the oldest entry */
    if (next_index >= (int8_t)hist->count) {
        /* Already at oldest, return current oldest */
        if (hist->browse_index >= 0 && hist->browse_index < (int8_t)hist->count) {
            uint8_t buf_idx = get_buffer_index(hist, (uint8_t)hist->browse_index);
            return hist->entries[buf_idx];
        }
        return NULL;
    }

    /* Update browse index and return entry */
    hist->browse_index = next_index;
    uint8_t buf_idx = get_buffer_index(hist, (uint8_t)next_index);
    return hist->entries[buf_idx];
}

const char*
history_get_next(history_manager_t* hist) {
    if (hist == NULL || hist->entries == NULL) {
        return NULL;
    }

    /* If not browsing, nothing to do */
    if (hist->browse_index < 0) {
        return NULL;
    }

    /* Calculate next browse index (going forward in time = newer) */
    int8_t next_index = hist->browse_index - 1;

    /* Check if we've returned to current input */
    if (next_index < 0) {
        hist->browse_index = -1;
        return NULL;  /* NULL indicates return to current input */
    }

    /* Update browse index and return entry */
    hist->browse_index = next_index;
    uint8_t buf_idx = get_buffer_index(hist, (uint8_t)next_index);
    return hist->entries[buf_idx];
}

void
history_reset_browse(history_manager_t* hist) {
    if (hist == NULL) {
        return;
    }
    hist->browse_index = -1;
}

uint8_t
history_get_count(const history_manager_t* hist) {
    if (hist == NULL) {
        return 0;
    }
    return hist->count;
}

const char*
history_get_entry(const history_manager_t* hist, uint8_t index) {
    if (hist == NULL || hist->entries == NULL || index >= hist->count) {
        return NULL;
    }

    uint8_t buf_idx = get_buffer_index(hist, index);
    return hist->entries[buf_idx];
}

void
history_clear(history_manager_t* hist) {
    if (hist == NULL) {
        return;
    }

    hist->count = 0;
    hist->head = 0;
    hist->browse_index = -1;

    /* Clear all entry buffers */
    if (hist->entries != NULL) {
        for (uint8_t i = 0; i < hist->capacity; i++) {
            if (hist->entries[i] != NULL) {
                hist->entries[i][0] = '\0';
            }
        }
    }
}

bool
history_is_browsing(const history_manager_t* hist) {
    if (hist == NULL) {
        return false;
    }
    return hist->browse_index >= 0;
}
