/**
 * \file            shell_command.c
 * \brief           Shell command registration implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Provides command registration, lookup, and management functionality.
 *
 * Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7
 */

#include "shell/shell_command.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Private Data                                                              */
/*---------------------------------------------------------------------------*/

/** Registered commands array */
static const shell_command_t* g_commands[SHELL_MAX_COMMANDS];

/** Number of registered commands */
static int g_command_count = 0;

/** Global completion callback */
static shell_completion_cb_t g_completion_callback = NULL;

/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

shell_status_t
shell_register_command(const shell_command_t* cmd) {
    /* Validate parameters */
    if (cmd == NULL || cmd->name == NULL || cmd->handler == NULL) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    /* Check for duplicate */
    if (shell_get_command(cmd->name) != NULL) {
        return SHELL_ERROR_ALREADY_EXISTS;
    }

    /* Check capacity */
    if (g_command_count >= SHELL_MAX_COMMANDS) {
        return SHELL_ERROR_NO_MEMORY;
    }

    /* Register command */
    g_commands[g_command_count++] = cmd;

    return SHELL_OK;
}

shell_status_t
shell_unregister_command(const char* name) {
    int i;

    /* Validate parameters */
    if (name == NULL) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    /* Find and remove command */
    for (i = 0; i < g_command_count; i++) {
        if (g_commands[i] != NULL &&
            strcmp(g_commands[i]->name, name) == 0) {
            /* Shift remaining commands */
            for (int j = i; j < g_command_count - 1; j++) {
                g_commands[j] = g_commands[j + 1];
            }
            g_command_count--;
            return SHELL_OK;
        }
    }

    return SHELL_ERROR_NOT_FOUND;
}

const shell_command_t*
shell_get_command(const char* name) {
    int i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < g_command_count; i++) {
        if (g_commands[i] != NULL &&
            strcmp(g_commands[i]->name, name) == 0) {
            return g_commands[i];
        }
    }

    return NULL;
}

shell_status_t
shell_get_commands(const shell_command_t** cmds[], int* count) {
    if (cmds == NULL || count == NULL) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    *cmds = g_commands;
    *count = g_command_count;

    return SHELL_OK;
}

int
shell_get_command_count(void) {
    return g_command_count;
}

shell_status_t
shell_set_completion_callback(shell_completion_cb_t callback) {
    g_completion_callback = callback;
    return SHELL_OK;
}

shell_completion_cb_t
shell_get_completion_callback(void) {
    return g_completion_callback;
}

void
shell_clear_commands(void) {
    g_command_count = 0;
    g_completion_callback = NULL;
    for (int i = 0; i < SHELL_MAX_COMMANDS; i++) {
        g_commands[i] = NULL;
    }
}
