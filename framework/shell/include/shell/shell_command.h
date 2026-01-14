/**
 * \file            shell_command.h
 * \brief           Shell command registration and management
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This file defines the command structure and registration API
 * for the Shell/CLI middleware.
 */

#ifndef SHELL_COMMAND_H
#define SHELL_COMMAND_H

#include "shell_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        SHELL_COMMAND Shell Command Interface
 * \brief           Command registration and management
 * \{
 */

/**
 * \brief           Command handler function type
 * \param[in]       argc: Number of arguments (including command name)
 * \param[in]       argv: Array of argument strings
 * \return          0 on success, non-zero error code on failure
 */
typedef int (*shell_cmd_handler_t)(int argc, char* argv[]);

/**
 * \brief           Command completion callback type
 * \param[in]       partial: Partial argument string to complete
 * \param[out]      completions: Array to store completion suggestions
 * \param[out]      count: Pointer to store number of completions found
 */
typedef void (*shell_completion_cb_t)(const char* partial, 
                                       char* completions[], 
                                       int* count);

/**
 * \brief           Shell command structure
 */
typedef struct {
    const char*             name;       /**< Command name (max 16 chars) */
    shell_cmd_handler_t     handler;    /**< Command handler function */
    const char*             help;       /**< Short help description */
    const char*             usage;      /**< Usage string (e.g., "cmd [options]") */
    shell_completion_cb_t   completion; /**< Argument completion callback (optional) */
} shell_command_t;

/**
 * \brief           Register a command with the Shell
 * \param[in]       cmd: Pointer to command structure (must remain valid)
 * \return          SHELL_OK on success
 * \return          SHELL_ERROR_INVALID_PARAM if cmd, name, or handler is NULL
 * \return          SHELL_ERROR_ALREADY_EXISTS if command name already registered
 * \return          SHELL_ERROR_NO_MEMORY if command registry is full
 */
shell_status_t shell_register_command(const shell_command_t* cmd);

/**
 * \brief           Unregister a command from the Shell
 * \param[in]       name: Command name to unregister
 * \return          SHELL_OK on success
 * \return          SHELL_ERROR_INVALID_PARAM if name is NULL
 * \return          SHELL_ERROR_NOT_FOUND if command not found
 */
shell_status_t shell_unregister_command(const char* name);

/**
 * \brief           Get a command by name
 * \param[in]       name: Command name to look up
 * \return          Pointer to command structure, or NULL if not found
 */
const shell_command_t* shell_get_command(const char* name);

/**
 * \brief           Get all registered commands
 * \param[out]      cmds: Pointer to store command array pointer
 * \param[out]      count: Pointer to store number of commands
 * \return          SHELL_OK on success
 */
shell_status_t shell_get_commands(const shell_command_t** cmds[], int* count);

/**
 * \brief           Get the number of registered commands
 * \return          Number of currently registered commands
 */
int shell_get_command_count(void);

/**
 * \brief           Set global completion callback
 * \param[in]       callback: Completion callback function
 * \return          SHELL_OK on success
 */
shell_status_t shell_set_completion_callback(shell_completion_cb_t callback);

/**
 * \brief           Get the global completion callback
 * \return          Current completion callback, or NULL if not set
 */
shell_completion_cb_t shell_get_completion_callback(void);

/**
 * \brief           Clear all registered commands (for testing)
 */
void shell_clear_commands(void);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* SHELL_COMMAND_H */
