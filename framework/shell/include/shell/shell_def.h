/**
 * \file            shell_def.h
 * \brief           Shell module definitions and constants
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This file contains status codes, constants, and common definitions
 * used throughout the Shell/CLI middleware.
 */

#ifndef SHELL_DEF_H
#define SHELL_DEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        SHELL_STATUS Shell Status Codes
 * \brief           Status codes for shell operations
 * \{
 */

/**
 * \brief           Shell operation status codes
 */
typedef enum {
    SHELL_OK = 0,                   /**< Operation successful */
    SHELL_ERROR = 1,                /**< Generic error */
    SHELL_ERROR_INVALID_PARAM = 2,  /**< Invalid parameter */
    SHELL_ERROR_NOT_INIT = 3,       /**< Module not initialized */
    SHELL_ERROR_ALREADY_INIT = 4,   /**< Module already initialized */
    SHELL_ERROR_NO_MEMORY = 5,      /**< Memory allocation failed */
    SHELL_ERROR_NOT_FOUND = 6,      /**< Item not found */
    SHELL_ERROR_ALREADY_EXISTS = 7, /**< Item already exists */
    SHELL_ERROR_NO_BACKEND = 8,     /**< No backend configured */
    SHELL_ERROR_BUFFER_FULL = 9,    /**< Buffer is full */
} shell_status_t;

/**
 * \}
 */

/**
 * \defgroup        SHELL_CONSTANTS Shell Constants
 * \brief           Constants for shell configuration
 * \{
 */

/** Maximum prompt string length */
#define SHELL_MAX_PROMPT_LEN 16

/** Minimum command buffer size */
#define SHELL_MIN_CMD_BUFFER_SIZE 64

/** Maximum command buffer size */
#define SHELL_MAX_CMD_BUFFER_SIZE 256

/** Default command buffer size */
#define SHELL_DEFAULT_CMD_BUFFER_SIZE 128

/** Minimum history depth */
#define SHELL_MIN_HISTORY_DEPTH 4

/** Maximum history depth */
#define SHELL_MAX_HISTORY_DEPTH 32

/** Default history depth */
#define SHELL_DEFAULT_HISTORY_DEPTH 16

/** Maximum number of registered commands */
#define SHELL_MAX_COMMANDS 32

/** Maximum number of arguments per command */
#define SHELL_MAX_ARGS 8

/** Maximum command name length */
#define SHELL_MAX_CMD_NAME 16

/** Maximum number of auto-completion matches */
#define SHELL_MAX_COMPLETIONS 16

/** Default prompt string */
#define SHELL_DEFAULT_PROMPT "nexus> "

/**
 * \}
 */

/**
 * \defgroup        SHELL_KEYS Special Key Codes
 * \brief           ASCII control character definitions
 * \{
 */

#define SHELL_KEY_CTRL_A    0x01 /**< Ctrl+A - Move to start */
#define SHELL_KEY_CTRL_C    0x03 /**< Ctrl+C - Cancel input */
#define SHELL_KEY_CTRL_E    0x05 /**< Ctrl+E - Move to end */
#define SHELL_KEY_CTRL_K    0x0B /**< Ctrl+K - Delete to end */
#define SHELL_KEY_CTRL_L    0x0C /**< Ctrl+L - Clear screen */
#define SHELL_KEY_CTRL_U    0x15 /**< Ctrl+U - Delete to start */
#define SHELL_KEY_CTRL_W    0x17 /**< Ctrl+W - Delete word */
#define SHELL_KEY_BACKSPACE 0x08 /**< Backspace */
#define SHELL_KEY_TAB       0x09 /**< Tab - Auto-complete */
#define SHELL_KEY_ENTER     0x0D /**< Enter/Return */
#define SHELL_KEY_ESCAPE    0x1B /**< Escape */
#define SHELL_KEY_DELETE    0x7F /**< Delete (alternate backspace) */

/**
 * \}
 */

/**
 * \defgroup        SHELL_MACROS Shell Utility Macros
 * \brief           Utility macros for shell operations
 * \{
 */

/**
 * \brief           Return if expression evaluates to error
 * \param[in]       expr: Expression to evaluate
 */
#define SHELL_RETURN_IF_ERROR(expr)                                            \
    do {                                                                       \
        shell_status_t _status = (expr);                                       \
        if (_status != SHELL_OK)                                               \
            return _status;                                                    \
    } while (0)

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* SHELL_DEF_H */
