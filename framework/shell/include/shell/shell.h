/**
 * \file            shell.h
 * \brief           Shell/CLI middleware main header
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This is the main header file for the Shell/CLI middleware.
 * It provides the core API for initializing, configuring, and
 * running the interactive command-line interface.
 */

#ifndef SHELL_H
#define SHELL_H

#include "shell_backend.h"
#include "shell_command.h"
#include "shell_def.h"
#include "shell_history.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        SHELL Shell Core API
 * \brief           Core shell initialization and processing
 * \{
 */

/**
 * \brief           Shell configuration structure
 */
typedef struct {
    const char* prompt;       /**< Prompt string (max 16 chars) */
    uint16_t cmd_buffer_size; /**< Command buffer size (64-256) */
    uint8_t history_depth;    /**< History depth (4-32) */
    uint8_t max_commands;     /**< Max commands (default 32) */
} shell_config_t;

/**
 * \brief           Default Shell configuration
 */
#define SHELL_CONFIG_DEFAULT                                                   \
    {                                                                          \
        .prompt = SHELL_DEFAULT_PROMPT,                                        \
        .cmd_buffer_size = SHELL_DEFAULT_CMD_BUFFER_SIZE,                      \
        .history_depth = SHELL_DEFAULT_HISTORY_DEPTH,                          \
        .max_commands = SHELL_MAX_COMMANDS                                     \
    }

/**
 * \brief           Initialize the Shell module
 * \param[in]       config: Pointer to configuration structure
 * \return          SHELL_OK on success
 * \return          SHELL_ERROR_INVALID_PARAM if config is NULL or invalid
 * \return          SHELL_ERROR_ALREADY_INIT if already initialized
 */
shell_status_t shell_init(const shell_config_t* config);

/**
 * \brief           Deinitialize the Shell module
 * \return          SHELL_OK on success
 * \return          SHELL_ERROR_NOT_INIT if not initialized
 */
shell_status_t shell_deinit(void);

/**
 * \brief           Check if Shell is initialized
 * \return          true if initialized, false otherwise
 */
bool shell_is_initialized(void);

/**
 * \brief           Process Shell input
 * \note            This function is non-blocking
 * \return          SHELL_OK on success
 * \return          SHELL_ERROR_NOT_INIT if not initialized
 * \return          SHELL_ERROR_NO_BACKEND if no backend configured
 */
shell_status_t shell_process(void);

/**
 * \brief           Get the last error code
 * \return          Last error code
 */
shell_status_t shell_get_last_error(void);

/**
 * \brief           Get error message string for a status code
 * \param[in]       status: Status code to get message for
 * \return          Pointer to error message string (never NULL)
 */
const char* shell_get_error_message(shell_status_t status);

/**
 * \brief           Print error message to shell output
 * \param[in]       status: Status code to print message for
 */
void shell_print_error(shell_status_t status);

/**
 * \brief           Print error message with context
 * \param[in]       status: Status code to print message for
 * \param[in]       context: Additional context string (can be NULL)
 */
void shell_print_error_context(shell_status_t status, const char* context);

/**
 * \brief           Reset shell to a known good state after error
 * \return          SHELL_OK on successful recovery, error code otherwise
 */
shell_status_t shell_recover(void);

/**
 * \brief           Get Shell version string
 * \return          Version string (e.g., "1.0.0")
 */
const char* shell_get_version(void);

/**
 * \brief           Print the Shell prompt
 */
void shell_print_prompt(void);

/**
 * \brief           Clear the terminal screen
 */
void shell_clear_screen(void);

/**
 * \}
 */

/**
 * \defgroup        SHELL_BUILTIN Built-in Commands
 * \brief           Built-in shell commands
 * \{
 */

/**
 * \brief           Register all built-in commands
 * \return          SHELL_OK on success
 */
shell_status_t shell_register_builtin_commands(void);

/**
 * \brief           Get the history manager
 * \return          Pointer to history manager, or NULL if not initialized
 */
history_manager_t* shell_get_history_manager(void);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* SHELL_H */
