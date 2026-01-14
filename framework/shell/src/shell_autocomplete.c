/**
 * @file shell_autocomplete.c
 * @brief Shell auto-completion implementation
 * 
 * Implements command name auto-completion with prefix matching,
 * common prefix calculation, and multi-match display.
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7
 */

#include "shell/shell_autocomplete.h"
#include "shell/shell_command.h"
#include <string.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

static shell_status_t autocomplete_argument(const char* input, 
                                             int input_len,
                                             int cursor_pos,
                                             completion_result_t* result);

/*---------------------------------------------------------------------------*/
/* Private Helper Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Safe string copy with null termination
 * 
 * @param dest Destination buffer
 * @param src  Source string
 * @param size Size of destination buffer
 */
static void safe_strcpy(char* dest, const char* src, size_t size)
{
    if (dest == NULL || src == NULL || size == 0) {
        return;
    }
    
    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/*---------------------------------------------------------------------------*/
/* Private Functions                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check if a string starts with a given prefix
 * 
 * @param str    String to check
 * @param prefix Prefix to match
 * @return true if str starts with prefix, false otherwise
 */
static bool starts_with(const char* str, const char* prefix)
{
    if (str == NULL || prefix == NULL) {
        return false;
    }
    
    while (*prefix != '\0') {
        if (*str != *prefix) {
            return false;
        }
        str++;
        prefix++;
    }
    
    return true;
}

/**
 * @brief Calculate the common prefix length of two strings
 * 
 * @param str1 First string
 * @param str2 Second string
 * @return Length of common prefix
 */
static int common_prefix_length(const char* str1, const char* str2)
{
    int len = 0;
    
    if (str1 == NULL || str2 == NULL) {
        return 0;
    }
    
    while (str1[len] != '\0' && str2[len] != '\0' && str1[len] == str2[len]) {
        len++;
    }
    
    return len;
}


/*---------------------------------------------------------------------------*/
/* Public Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize the auto-completion module
 */
shell_status_t autocomplete_init(void)
{
    /* No initialization needed for now */
    return SHELL_OK;
}

/**
 * @brief Find command completions for partial input
 * 
 * Searches registered commands for names that start with the partial input.
 * Implements Requirements 6.1, 6.2, 6.3, 6.4.
 */
shell_status_t autocomplete_command(const char* partial, completion_result_t* result)
{
    const shell_command_t** commands = NULL;
    int command_count = 0;
    int partial_len;
    int i;
    
    /* Validate parameters */
    if (result == NULL) {
        return SHELL_ERROR_INVALID_PARAM;
    }
    
    /* Initialize result */
    result->match_count = 0;
    result->common_prefix_len = 0;
    
    /* Handle NULL or empty partial - match all commands */
    if (partial == NULL) {
        partial = "";
    }
    partial_len = (int)strlen(partial);
    
    /* Get registered commands */
    if (shell_get_commands(&commands, &command_count) != SHELL_OK) {
        return SHELL_OK; /* No commands registered, return empty result */
    }
    
    if (commands == NULL || command_count == 0) {
        return SHELL_OK; /* No commands registered */
    }
    
    /* Find matching commands */
    for (i = 0; i < command_count && result->match_count < SHELL_MAX_COMPLETIONS; i++) {
        if (commands[i] != NULL && commands[i]->name != NULL) {
            if (starts_with(commands[i]->name, partial)) {
                /* Copy matching command name */
                safe_strcpy(result->matches[result->match_count], 
                            commands[i]->name, 
                            SHELL_MAX_CMD_NAME + 1);
                result->match_count++;
            }
        }
    }
    
    /* Calculate common prefix if there are matches */
    if (result->match_count > 0) {
        /* Start with the length of the first match */
        result->common_prefix_len = (int)strlen(result->matches[0]);
        
        /* Find common prefix among all matches */
        for (i = 1; i < result->match_count; i++) {
            int prefix_len = common_prefix_length(result->matches[0], result->matches[i]);
            if (prefix_len < result->common_prefix_len) {
                result->common_prefix_len = prefix_len;
            }
        }
    }
    
    return SHELL_OK;
}

/**
 * @brief Show all matching completions to the user
 * 
 * Displays the list of matching commands when multiple matches exist.
 * Implements Requirements 6.2, 6.5.
 */
void autocomplete_show_matches(const completion_result_t* result)
{
    int i;
    
    if (result == NULL || result->match_count == 0) {
        return;
    }
    
    /* Print newline before showing matches */
    printf("\n");
    
    /* Print all matches */
    for (i = 0; i < result->match_count; i++) {
        printf("%s  ", result->matches[i]);
    }
    
    /* Print newline after matches */
    printf("\n");
}

/**
 * @brief Get the common prefix from completion results
 * 
 * Returns the common prefix string that can be used to extend the input.
 */
int autocomplete_get_common_prefix(const completion_result_t* result, 
                                    char* prefix, 
                                    int prefix_size)
{
    if (result == NULL || prefix == NULL || prefix_size <= 0) {
        return 0;
    }
    
    if (result->match_count == 0 || result->common_prefix_len == 0) {
        prefix[0] = '\0';
        return 0;
    }
    
    /* Copy common prefix from first match */
    int copy_len = result->common_prefix_len;
    if (copy_len >= prefix_size) {
        copy_len = prefix_size - 1;
    }
    
    /* Manual copy to avoid strncpy warnings */
    for (int i = 0; i < copy_len; i++) {
        prefix[i] = result->matches[0][i];
    }
    prefix[copy_len] = '\0';
    
    return copy_len;
}


/**
 * @brief Process Tab key press for auto-completion
 * 
 * Main entry point for handling Tab key auto-completion.
 * Implements Requirements 6.1, 6.2, 6.3, 6.4, 6.6, 6.7.
 */
shell_status_t autocomplete_process(const char* input, 
                                     int input_len,
                                     int cursor_pos,
                                     completion_result_t* result)
{
    char partial[SHELL_MAX_CMD_NAME + 1];
    int partial_len;
    int word_start;
    int i;
    
    /* Validate parameters */
    if (result == NULL) {
        return SHELL_ERROR_INVALID_PARAM;
    }
    
    /* Initialize result */
    result->match_count = 0;
    result->common_prefix_len = 0;
    
    /* Handle NULL or empty input */
    if (input == NULL || input_len == 0) {
        return autocomplete_command("", result);
    }
    
    /* Find the start of the current word (for command completion) */
    word_start = 0;
    
    /* Skip leading whitespace */
    while (word_start < input_len && 
           (input[word_start] == ' ' || input[word_start] == '\t')) {
        word_start++;
    }
    
    /* Find end of first word (command name) */
    int word_end = word_start;
    while (word_end < input_len && 
           input[word_end] != ' ' && input[word_end] != '\t') {
        word_end++;
    }
    
    /* Only complete if cursor is at or after the first word */
    if (cursor_pos < word_start) {
        return SHELL_OK; /* Cursor before command, no completion */
    }
    
    /* If cursor is past the first word, we're in argument territory */
    if (cursor_pos > word_end) {
        /* Try argument completion */
        return autocomplete_argument(input, input_len, cursor_pos, result);
    }
    
    /* Extract partial command name */
    partial_len = cursor_pos - word_start;
    if (partial_len > SHELL_MAX_CMD_NAME) {
        partial_len = SHELL_MAX_CMD_NAME;
    }
    
    for (i = 0; i < partial_len; i++) {
        partial[i] = input[word_start + i];
    }
    partial[partial_len] = '\0';
    
    /* Find completions */
    return autocomplete_command(partial, result);
}

/*---------------------------------------------------------------------------*/
/* Private Function Implementations                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process argument completion
 * 
 * Handles Tab completion for command arguments using command-specific
 * or global completion callbacks.
 * Implements Requirements 6.6, 6.7.
 */
static shell_status_t autocomplete_argument(const char* input, 
                                             int input_len,
                                             int cursor_pos,
                                             completion_result_t* result)
{
    char cmd_name[SHELL_MAX_CMD_NAME + 1];
    char partial_arg[SHELL_MAX_CMD_BUFFER_SIZE];
    int cmd_start, cmd_end;
    int arg_start;
    shell_completion_cb_t callback = NULL;
    const shell_command_t* cmd = NULL;
    char* completions[SHELL_MAX_COMPLETIONS];
    int completion_count = 0;
    int i;
    
    /* Initialize result */
    result->match_count = 0;
    result->common_prefix_len = 0;
    
    /* Find command name */
    cmd_start = 0;
    while (cmd_start < input_len && 
           (input[cmd_start] == ' ' || input[cmd_start] == '\t')) {
        cmd_start++;
    }
    
    cmd_end = cmd_start;
    while (cmd_end < input_len && 
           input[cmd_end] != ' ' && input[cmd_end] != '\t') {
        cmd_end++;
    }
    
    /* Extract command name */
    int cmd_len = cmd_end - cmd_start;
    if (cmd_len > SHELL_MAX_CMD_NAME) {
        cmd_len = SHELL_MAX_CMD_NAME;
    }
    
    for (i = 0; i < cmd_len; i++) {
        cmd_name[i] = input[cmd_start + i];
    }
    cmd_name[cmd_len] = '\0';
    
    /* Find the command */
    cmd = shell_get_command(cmd_name);
    
    /* Get completion callback - prefer command-specific, fall back to global */
    if (cmd != NULL && cmd->completion != NULL) {
        callback = cmd->completion;
    } else {
        callback = shell_get_completion_callback();
    }
    
    /* If no callback available, no argument completion */
    if (callback == NULL) {
        return SHELL_OK;
    }
    
    /* Find the start of the current argument */
    arg_start = cursor_pos;
    while (arg_start > cmd_end && 
           input[arg_start - 1] != ' ' && input[arg_start - 1] != '\t') {
        arg_start--;
    }
    
    /* Extract partial argument */
    int partial_len = cursor_pos - arg_start;
    if (partial_len >= SHELL_MAX_CMD_BUFFER_SIZE) {
        partial_len = SHELL_MAX_CMD_BUFFER_SIZE - 1;
    }
    
    for (i = 0; i < partial_len; i++) {
        partial_arg[i] = input[arg_start + i];
    }
    partial_arg[partial_len] = '\0';
    
    /* Allocate temporary buffers for completions */
    static char completion_buffers[SHELL_MAX_COMPLETIONS][SHELL_MAX_CMD_NAME + 1];
    for (i = 0; i < SHELL_MAX_COMPLETIONS; i++) {
        completions[i] = completion_buffers[i];
    }
    
    /* Call the completion callback */
    callback(partial_arg, completions, &completion_count);
    
    /* Copy completions to result */
    if (completion_count > SHELL_MAX_COMPLETIONS) {
        completion_count = SHELL_MAX_COMPLETIONS;
    }
    
    for (i = 0; i < completion_count; i++) {
        if (completions[i] != NULL) {
            safe_strcpy(result->matches[i], completions[i], SHELL_MAX_CMD_NAME + 1);
        }
    }
    result->match_count = completion_count;
    
    /* Calculate common prefix if there are matches */
    if (result->match_count > 0) {
        result->common_prefix_len = (int)strlen(result->matches[0]);
        
        for (i = 1; i < result->match_count; i++) {
            int prefix_len = common_prefix_length(result->matches[0], result->matches[i]);
            if (prefix_len < result->common_prefix_len) {
                result->common_prefix_len = prefix_len;
            }
        }
    }
    
    return SHELL_OK;
}
