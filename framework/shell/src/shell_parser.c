/**
 * @file shell_parser.c
 * @brief Shell command line parser implementation
 * 
 * This module implements command line parsing for the Shell/CLI middleware.
 * It supports space-separated arguments and quoted strings.
 * 
 * Requirements: 3.1, 3.4, 3.5, 3.6
 */

#include "shell/shell_parser.h"
#include <string.h>
#include <stdbool.h>

/**
 * @brief Skip whitespace characters in a string
 * 
 * @param str Pointer to string pointer (updated to skip whitespace)
 */
static void skip_whitespace(char** str) {
    while (**str == ' ' || **str == '\t') {
        (*str)++;
    }
}

/**
 * @brief Check if character is a quote character
 * 
 * @param c Character to check
 * @return true if quote character, false otherwise
 */
static bool is_quote(char c) {
    return (c == '"' || c == '\'');
}

/**
 * @brief Parse a quoted string argument
 * 
 * Extracts a string enclosed in quotes, handling the quote character.
 * The opening quote is consumed before calling this function.
 * 
 * @param str Pointer to string pointer (starts after opening quote)
 * @param quote_char The quote character used ('"' or '\'')
 * @param arg_start Output: pointer to start of argument
 * @return SHELL_OK on success, error code on failure
 */
static shell_status_t parse_quoted_string(char** str, char quote_char, char** arg_start) {
    *arg_start = *str;
    
    /* Find closing quote */
    while (**str != '\0' && **str != quote_char) {
        (*str)++;
    }
    
    if (**str == '\0') {
        /* Unterminated quote - treat rest of string as argument */
        return SHELL_OK;
    }
    
    /* Null-terminate the argument (replace closing quote) */
    **str = '\0';
    (*str)++;
    
    return SHELL_OK;
}

/**
 * @brief Parse an unquoted argument
 * 
 * Extracts an argument delimited by whitespace.
 * 
 * @param str Pointer to string pointer
 * @param arg_start Output: pointer to start of argument
 * @return SHELL_OK on success
 */
static shell_status_t parse_unquoted_arg(char** str, char** arg_start) {
    *arg_start = *str;
    
    /* Find end of argument (whitespace or end of string) */
    while (**str != '\0' && **str != ' ' && **str != '\t') {
        (*str)++;
    }
    
    /* Null-terminate if not at end of string */
    if (**str != '\0') {
        **str = '\0';
        (*str)++;
    }
    
    return SHELL_OK;
}

/**
 * @brief Parse a command line into command name and arguments
 * 
 * Parses a command line string, extracting the command name and arguments.
 * Supports:
 * - Space-separated arguments (Requirement 3.4)
 * - Quoted strings as single arguments (Requirement 3.5)
 * - Maximum 8 arguments per command (Requirement 3.6)
 * 
 * @param line Input command line (will be modified)
 * @param result Output parsed command structure
 * @return SHELL_OK on success
 * @return SHELL_ERROR_INVALID_PARAM if line or result is NULL
 * @return SHELL_ERROR_BUFFER_FULL if too many arguments
 */
shell_status_t parse_command_line(char* line, parsed_command_t* result) {
    if (line == NULL || result == NULL) {
        return SHELL_ERROR_INVALID_PARAM;
    }
    
    /* Initialize result */
    result->cmd_name = NULL;
    result->argc = 0;
    for (int i = 0; i < SHELL_MAX_ARGS; i++) {
        result->argv[i] = NULL;
    }
    
    char* ptr = line;
    
    /* Skip leading whitespace */
    skip_whitespace(&ptr);
    
    /* Check for empty line */
    if (*ptr == '\0') {
        return SHELL_OK;
    }
    
    /* Parse command name (first token) */
    char* arg_start;
    if (is_quote(*ptr)) {
        char quote_char = *ptr;
        ptr++;  /* Skip opening quote */
        parse_quoted_string(&ptr, quote_char, &arg_start);
    } else {
        parse_unquoted_arg(&ptr, &arg_start);
    }
    
    /* Set command name */
    result->cmd_name = arg_start;
    result->argv[0] = arg_start;
    result->argc = 1;
    
    /* Parse remaining arguments */
    while (*ptr != '\0') {
        /* Skip whitespace between arguments */
        skip_whitespace(&ptr);
        
        /* Check for end of string */
        if (*ptr == '\0') {
            break;
        }
        
        /* Check argument limit */
        if (result->argc >= SHELL_MAX_ARGS) {
            return SHELL_ERROR_BUFFER_FULL;
        }
        
        /* Parse next argument */
        if (is_quote(*ptr)) {
            char quote_char = *ptr;
            ptr++;  /* Skip opening quote */
            parse_quoted_string(&ptr, quote_char, &arg_start);
        } else {
            parse_unquoted_arg(&ptr, &arg_start);
        }
        
        /* Add argument to result */
        result->argv[result->argc] = arg_start;
        result->argc++;
    }
    
    return SHELL_OK;
}
