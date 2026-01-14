/**
 * \file            shell_autocomplete.h
 * \brief           Shell auto-completion interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This file defines the auto-completion structures and API
 * for the Shell/CLI middleware.
 */

#ifndef SHELL_AUTOCOMPLETE_H
#define SHELL_AUTOCOMPLETE_H

#include "shell_def.h"
#include "shell_command.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        SHELL_AUTOCOMPLETE Shell Auto-Completion Interface
 * \brief           Command auto-completion functionality
 * \{
 */

/**
 * \brief           Auto-completion result structure
 */
typedef struct {
    char        matches[SHELL_MAX_COMPLETIONS][SHELL_MAX_CMD_NAME + 1]; /**< Matching command names */
    int         match_count;                                             /**< Number of matches found */
    int         common_prefix_len;                                       /**< Length of common prefix */
} completion_result_t;

/**
 * \brief           Initialize the auto-completion module
 * \return          SHELL_OK on success
 */
shell_status_t autocomplete_init(void);

/**
 * \brief           Find command completions for partial input
 * \param[in]       partial: Partial command name to complete
 * \param[out]      result: Pointer to store completion results
 * \return          SHELL_OK on success
 * \return          SHELL_ERROR_INVALID_PARAM if partial or result is NULL
 */
shell_status_t autocomplete_command(const char* partial, completion_result_t* result);

/**
 * \brief           Show all matching completions to the user
 * \param[in]       result: Pointer to completion results to display
 */
void autocomplete_show_matches(const completion_result_t* result);

/**
 * \brief           Get the common prefix from completion results
 * \param[in]       result: Pointer to completion results
 * \param[out]      prefix: Buffer to store the common prefix
 * \param[in]       prefix_size: Size of the prefix buffer
 * \return          Length of the common prefix, or 0 if no common prefix
 */
int autocomplete_get_common_prefix(const completion_result_t* result, 
                                    char* prefix, 
                                    int prefix_size);

/**
 * \brief           Process Tab key press for auto-completion
 * \param[in]       input: Current input buffer
 * \param[in]       input_len: Length of current input
 * \param[in]       cursor_pos: Current cursor position
 * \param[out]      result: Pointer to store completion results
 * \return          SHELL_OK on success
 * \return          SHELL_ERROR_INVALID_PARAM if parameters are invalid
 */
shell_status_t autocomplete_process(const char* input, 
                                     int input_len,
                                     int cursor_pos,
                                     completion_result_t* result);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* SHELL_AUTOCOMPLETE_H */
