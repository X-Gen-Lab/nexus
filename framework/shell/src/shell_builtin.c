/**
 * \file            shell_builtin.c
 * \brief           Shell built-in commands implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Implements the built-in commands for the Shell/CLI middleware:
 * - help: List all commands or show help for a specific command
 * - version: Show Shell version
 * - clear: Clear the terminal screen
 * - history: Show command history
 * - echo: Print arguments
 *
 * Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6
 */

#include "shell/shell.h"
#include "shell/shell_command.h"
#include "shell/shell_history.h"
#include "shell/shell_backend.h"
#include <string.h>

/**
 * \addtogroup      SHELL_BUILTIN
 * \{
 */

/**
 * \name            Command Handlers
 * \{
 */

/**
 * \brief           Help command handler
 *
 * Lists all registered commands or shows detailed help for a specific command.
 *
 * Requirements: 7.1, 7.2
 *
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 */
static int
cmd_help(int argc, char* argv[]) {
    const shell_command_t** cmds = NULL;
    int count = 0;

    if (argc > 1) {
        /* Requirement 7.2: Show help for specific command */
        const shell_command_t* cmd = shell_get_command(argv[1]);
        if (cmd == NULL) {
            shell_printf("Unknown command: %s\r\n", argv[1]);
            return 1;
        }

        shell_printf("Command: %s\r\n", cmd->name);
        if (cmd->help != NULL) {
            shell_printf("  Description: %s\r\n", cmd->help);
        }
        if (cmd->usage != NULL) {
            shell_printf("  Usage: %s\r\n", cmd->usage);
        }
        return 0;
    }

    /* Requirement 7.1: List all registered commands */
    shell_status_t status = shell_get_commands(&cmds, &count);
    if (status != SHELL_OK || cmds == NULL) {
        shell_printf("Error: Failed to get command list\r\n");
        return 1;
    }

    shell_printf("Available commands:\r\n");
    for (int i = 0; i < count; i++) {
        if (cmds[i] != NULL && cmds[i]->name != NULL) {
            if (cmds[i]->help != NULL) {
                shell_printf("  %-12s - %s\r\n", cmds[i]->name, cmds[i]->help);
            } else {
                shell_printf("  %s\r\n", cmds[i]->name);
            }
        }
    }

    shell_printf("\r\nType 'help <command>' for more information.\r\n");
    return 0;
}

/**
 * \brief           Version command handler
 *
 * Displays the Shell version string.
 *
 * Requirement: 7.3
 *
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 */
static int
cmd_version(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_printf("Shell version: %s\r\n", shell_get_version());
    return 0;
}

/**
 * \brief           Clear command handler
 *
 * Clears the terminal screen using ANSI escape sequences.
 *
 * Requirement: 7.4
 *
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 */
static int
cmd_clear(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    shell_clear_screen();
    return 0;
}

/**
 * \brief           History command handler
 *
 * Displays the command history with entry numbers.
 *
 * Requirement: 7.5
 *
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 */
static int
cmd_history(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    history_manager_t* hist = shell_get_history_manager();
    if (hist == NULL) {
        shell_printf("Error: Shell not initialized\r\n");
        return 1;
    }

    uint8_t count = history_get_count(hist);
    if (count == 0) {
        shell_printf("No commands in history\r\n");
        return 0;
    }

    /* Display history from oldest to newest */
    for (int i = (int)count - 1; i >= 0; i--) {
        const char* entry = history_get_entry(hist, (uint8_t)i);
        if (entry != NULL) {
            shell_printf("  %3d  %s\r\n", (int)count - i, entry);
        }
    }

    return 0;
}

/**
 * \brief           Echo command handler
 *
 * Prints all arguments separated by spaces.
 *
 * Requirement: 7.6
 *
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 */
static int
cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            shell_printf(" ");
        }
        shell_printf("%s", argv[i]);
    }
    shell_printf("\r\n");
    return 0;
}

/**
 * \}
 */

/**
 * \name            Built-in Command Definitions
 * \{
 */

/** Help command definition */
static const shell_command_t builtin_help = {
    .name = "help",
    .handler = cmd_help,
    .help = "Show available commands",
    .usage = "help [command]",
    .completion = NULL
};

/** Version command definition */
static const shell_command_t builtin_version = {
    .name = "version",
    .handler = cmd_version,
    .help = "Show Shell version",
    .usage = "version",
    .completion = NULL
};

/** Clear command definition */
static const shell_command_t builtin_clear = {
    .name = "clear",
    .handler = cmd_clear,
    .help = "Clear the terminal screen",
    .usage = "clear",
    .completion = NULL
};

/** History command definition */
static const shell_command_t builtin_history = {
    .name = "history",
    .handler = cmd_history,
    .help = "Show command history",
    .usage = "history",
    .completion = NULL
};

/** Echo command definition */
static const shell_command_t builtin_echo = {
    .name = "echo",
    .handler = cmd_echo,
    .help = "Print arguments",
    .usage = "echo [text...]",
    .completion = NULL
};

/**
 * \}
 */

/**
 * \name            Public API
 * \{
 */

/**
 * \brief           Register all built-in commands
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t
shell_register_builtin_commands(void) {
    shell_status_t status;

    /* Register help command (Requirement 7.1, 7.2) */
    status = shell_register_command(&builtin_help);
    if (status != SHELL_OK) {
        return status;
    }

    /* Register version command (Requirement 7.3) */
    status = shell_register_command(&builtin_version);
    if (status != SHELL_OK) {
        return status;
    }

    /* Register clear command (Requirement 7.4) */
    status = shell_register_command(&builtin_clear);
    if (status != SHELL_OK) {
        return status;
    }

    /* Register history command (Requirement 7.5) */
    status = shell_register_command(&builtin_history);
    if (status != SHELL_OK) {
        return status;
    }

    /* Register echo command (Requirement 7.6) */
    status = shell_register_command(&builtin_echo);
    if (status != SHELL_OK) {
        return status;
    }

    return SHELL_OK;
}

/**
 * \}
 */

/**
 * \}
 */
