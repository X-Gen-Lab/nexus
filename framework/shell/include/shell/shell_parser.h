/**
 * \file            shell_parser.h
 * \brief           Shell command line parser interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This file defines the command line parsing API for the Shell/CLI middleware.
 * The parser supports space-separated arguments and quoted strings.
 */

#ifndef SHELL_PARSER_H
#define SHELL_PARSER_H

#include "shell_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        SHELL_PARSER Shell Parser Interface
 * \brief           Command line parsing functionality
 * \{
 */

/**
 * \brief           Parsed command structure
 */
typedef struct {
    char* cmd_name;             /**< Command name (first token) */
    int argc;                   /**< Argument count (including command name) */
    char* argv[SHELL_MAX_ARGS]; /**< Argument array */
} parsed_command_t;

/**
 * \brief           Parse a command line into command name and arguments
 * \param[in,out]   line: Input command line buffer (will be modified in place)
 * \param[out]      result: Output parsed command structure
 * \return          SHELL_OK on success
 * \return          SHELL_ERROR_INVALID_PARAM if line or result is NULL
 * \return          SHELL_ERROR_BUFFER_FULL if too many arguments (>
 * SHELL_MAX_ARGS)
 * \note            The line buffer is modified in place. The argv pointers
 * point into the original line buffer.
 * \note            Empty lines result in argc=0 and cmd_name=NULL
 * \note            Unterminated quotes are handled gracefully (rest of string
 * is argument)
 */
shell_status_t parse_command_line(char* line, parsed_command_t* result);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* SHELL_PARSER_H */
