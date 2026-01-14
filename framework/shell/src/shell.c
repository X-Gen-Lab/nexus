/**
 * \file            shell.c
 * \brief           Shell core implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Implements the main Shell module including initialization, deinitialization,
 * input processing, escape sequence handling, and command execution.
 *
 * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 3.2, 3.3, 3.7, 4.4, 4.5, 4.6,
 *               4.7, 4.8, 4.9, 4.10, 4.11, 4.12, 9.1, 9.2, 9.3, 9.4, 9.5
 */

#include "shell/shell.h"
#include "shell/shell_line_editor.h"
#include "shell/shell_history.h"
#include "shell/shell_autocomplete.h"
#include "shell/shell_parser.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * \defgroup        SHELL_CORE Shell Core Implementation
 * \brief           Core shell functionality
 * \{
 */

/**
 * \name            Constants
 * \{
 */

/** Shell version string */
#define SHELL_VERSION               "1.0.0"

/** Maximum escape sequence buffer size */
#define SHELL_ESCAPE_BUFFER_SIZE    8

/** ANSI escape sequences */
#define ANSI_CLEAR_SCREEN           "\033[2J\033[H"
#define ANSI_CURSOR_LEFT            "\033[D"
#define ANSI_CURSOR_RIGHT           "\033[C"
#define ANSI_ERASE_LINE             "\033[K"
#define ANSI_CURSOR_SAVE            "\033[s"
#define ANSI_CURSOR_RESTORE         "\033[u"

/**
 * \}
 */

/**
 * \name            Escape Sequence State Machine
 * \{
 */

/**
 * \brief           Escape sequence parser states
 */
typedef enum {
    ESC_STATE_NORMAL = 0,           /**< Normal input state */
    ESC_STATE_ESC,                  /**< Received ESC (0x1B) */
    ESC_STATE_CSI,                  /**< Received CSI (ESC [) */
    ESC_STATE_SS3                   /**< Received SS3 (ESC O) */
} escape_state_t;

/**
 * \brief           Parsed escape sequence result
 */
typedef enum {
    ESC_RESULT_NONE = 0,            /**< No complete sequence yet */
    ESC_RESULT_UP,                  /**< Up arrow */
    ESC_RESULT_DOWN,                /**< Down arrow */
    ESC_RESULT_LEFT,                /**< Left arrow */
    ESC_RESULT_RIGHT,               /**< Right arrow */
    ESC_RESULT_HOME,                /**< Home key */
    ESC_RESULT_END,                 /**< End key */
    ESC_RESULT_DELETE,              /**< Delete key */
    ESC_RESULT_INVALID              /**< Invalid/unknown sequence */
} escape_result_t;

/**
 * \}
 */

/**
 * \brief           Shell context structure
 *
 * Contains all state for the Shell module.
 */
typedef struct {
    bool                initialized;    /**< Initialization flag */
    shell_config_t      config;         /**< Configuration copy */
    const shell_backend_t* backend;     /**< I/O backend */
    line_editor_t       editor;         /**< Line editor state */
    history_manager_t   history;        /**< History manager state */
    char*               cmd_buffer;     /**< Command buffer */
    char*               saved_input;    /**< Saved input for history */
    shell_status_t      last_error;     /**< Last error code */

    /* Escape sequence parsing state */
    escape_state_t      escape_state;   /**< Current escape state */
    uint8_t             escape_buffer[SHELL_ESCAPE_BUFFER_SIZE];
    uint8_t             escape_index;   /**< Escape buffer index */

    /* History storage */
    char**              history_entries;/**< History entry pointers */
    char*               history_storage;/**< History storage buffer */

    /* Prompt storage */
    char                prompt[SHELL_MAX_PROMPT_LEN + 1];
} shell_context_t;

/**
 * \name            Private Data
 * \{
 */

/** Global shell context */
static shell_context_t g_shell_ctx;

/**
 * \}
 */

/**
 * \name            Private Function Declarations
 * \{
 */

static shell_status_t validate_config(const shell_config_t* config);
static void reset_escape_state(void);
static escape_result_t process_escape_char(uint8_t c);
static void handle_escape_result(escape_result_t result);
static void handle_printable_char(char c);
static void handle_control_char(uint8_t c);
static void handle_tab_completion(void);
static void execute_command_line(void);
static void redraw_line(void);
static void refresh_line_from_cursor(void);

/**
 * \}
 */


/**
 * \name            Configuration Validation
 * \{
 */

/**
 * \brief           Validate shell configuration
 * \param[in]       config: Configuration to validate
 * \return          SHELL_OK if valid, error code otherwise
 */
static shell_status_t
validate_config(const shell_config_t* config) {
    if (config == NULL) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    /* Validate command buffer size (Requirement 1.5) */
    if (config->cmd_buffer_size < SHELL_MIN_CMD_BUFFER_SIZE ||
        config->cmd_buffer_size > SHELL_MAX_CMD_BUFFER_SIZE) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    /* Validate history depth (Requirement 5.4) */
    if (config->history_depth < SHELL_MIN_HISTORY_DEPTH ||
        config->history_depth > SHELL_MAX_HISTORY_DEPTH) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    /* Validate prompt length (Requirement 1.4) */
    if (config->prompt != NULL &&
        strlen(config->prompt) > SHELL_MAX_PROMPT_LEN) {
        return SHELL_ERROR_INVALID_PARAM;
    }

    return SHELL_OK;
}

/**
 * \}
 */

/**
 * \name            Escape Sequence Processing
 * \{
 */

/**
 * \brief           Reset escape sequence state machine
 */
static void
reset_escape_state(void) {
    g_shell_ctx.escape_state = ESC_STATE_NORMAL;
    g_shell_ctx.escape_index = 0;
    memset(g_shell_ctx.escape_buffer, 0, SHELL_ESCAPE_BUFFER_SIZE);
}

/**
 * \brief           Process a character in escape sequence
 * \param[in]       c: Character to process
 * \return          Escape result (key identified or still processing)
 */
static escape_result_t
process_escape_char(uint8_t c) {
    switch (g_shell_ctx.escape_state) {
        case ESC_STATE_NORMAL:
            if (c == SHELL_KEY_ESCAPE) {
                g_shell_ctx.escape_state = ESC_STATE_ESC;
                return ESC_RESULT_NONE;
            }
            break;

        case ESC_STATE_ESC:
            if (c == '[') {
                g_shell_ctx.escape_state = ESC_STATE_CSI;
                return ESC_RESULT_NONE;
            } else if (c == 'O') {
                g_shell_ctx.escape_state = ESC_STATE_SS3;
                return ESC_RESULT_NONE;
            }
            reset_escape_state();
            return ESC_RESULT_INVALID;

        case ESC_STATE_CSI:
            /* Store character in buffer for multi-char sequences */
            if (g_shell_ctx.escape_index < SHELL_ESCAPE_BUFFER_SIZE - 1) {
                g_shell_ctx.escape_buffer[g_shell_ctx.escape_index++] = c;
            }

            /* Check for final character (letter or ~) */
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                c == '~') {
                escape_result_t result = ESC_RESULT_INVALID;

                /* Single character sequences */
                if (g_shell_ctx.escape_index == 1) {
                    switch (c) {
                        case 'A': result = ESC_RESULT_UP; break;
                        case 'B': result = ESC_RESULT_DOWN; break;
                        case 'C': result = ESC_RESULT_RIGHT; break;
                        case 'D': result = ESC_RESULT_LEFT; break;
                        case 'H': result = ESC_RESULT_HOME; break;
                        case 'F': result = ESC_RESULT_END; break;
                        default: break;
                    }
                }
                /* Multi-character sequences (e.g., ESC[1~, ESC[3~) */
                else if (g_shell_ctx.escape_index == 2 && c == '~') {
                    switch (g_shell_ctx.escape_buffer[0]) {
                        case '1': result = ESC_RESULT_HOME; break;
                        case '3': result = ESC_RESULT_DELETE; break;
                        case '4': result = ESC_RESULT_END; break;
                        default: break;
                    }
                }

                reset_escape_state();
                return result;
            }
            return ESC_RESULT_NONE;

        case ESC_STATE_SS3:
            /* SS3 sequences (ESC O x) */
            {
                escape_result_t result = ESC_RESULT_INVALID;
                switch (c) {
                    case 'A': result = ESC_RESULT_UP; break;
                    case 'B': result = ESC_RESULT_DOWN; break;
                    case 'C': result = ESC_RESULT_RIGHT; break;
                    case 'D': result = ESC_RESULT_LEFT; break;
                    case 'H': result = ESC_RESULT_HOME; break;
                    case 'F': result = ESC_RESULT_END; break;
                    default: break;
                }
                reset_escape_state();
                return result;
            }

        default:
            reset_escape_state();
            break;
    }

    return ESC_RESULT_INVALID;
}

/**
 * \}
 */


/**
 * \name            Line Display Functions
 * \{
 */

/**
 * \brief           Redraw the entire command line
 *
 * Clears the current line and redraws prompt and buffer content.
 */
static void
redraw_line(void) {
    if (g_shell_ctx.backend == NULL || g_shell_ctx.backend->write == NULL) {
        return;
    }

    /* Move to start of line and clear */
    shell_putchar('\r');
    shell_puts(ANSI_ERASE_LINE);

    /* Print prompt */
    shell_puts(g_shell_ctx.prompt);

    /* Print buffer content */
    const char* buf = line_editor_get_buffer(&g_shell_ctx.editor);
    shell_puts(buf);

    /* Move cursor to correct position */
    uint16_t len = line_editor_get_length(&g_shell_ctx.editor);
    uint16_t cursor = line_editor_get_cursor(&g_shell_ctx.editor);
    int back = (int)len - (int)cursor;
    while (back > 0) {
        shell_puts(ANSI_CURSOR_LEFT);
        back--;
    }
}

/**
 * \brief           Refresh line from cursor position to end
 *
 * Used after inserting or deleting characters.
 */
static void
refresh_line_from_cursor(void) {
    if (g_shell_ctx.backend == NULL || g_shell_ctx.backend->write == NULL) {
        return;
    }

    const char* buf = line_editor_get_buffer(&g_shell_ctx.editor);
    uint16_t cursor = line_editor_get_cursor(&g_shell_ctx.editor);
    uint16_t len = line_editor_get_length(&g_shell_ctx.editor);

    /* Print from cursor to end */
    shell_puts(&buf[cursor]);

    /* Clear any remaining characters */
    shell_puts(ANSI_ERASE_LINE);

    /* Move cursor back to correct position */
    int back = (int)len - (int)cursor;
    while (back > 0) {
        shell_puts(ANSI_CURSOR_LEFT);
        back--;
    }
}

/**
 * \}
 */

/**
 * \name            Input Handling Functions
 * \{
 */

/**
 * \brief           Handle escape sequence result
 * \param[in]       result: Parsed escape sequence result
 */
static void
handle_escape_result(escape_result_t result) {
    const char* hist_cmd;

    switch (result) {
        case ESC_RESULT_UP:
            /* Save current input if starting to browse */
            if (!history_is_browsing(&g_shell_ctx.history)) {
                const char* buf = line_editor_get_buffer(&g_shell_ctx.editor);
                size_t len = strlen(buf);
                if (len >= g_shell_ctx.config.cmd_buffer_size) {
                    len = g_shell_ctx.config.cmd_buffer_size - 1;
                }
                memcpy(g_shell_ctx.saved_input, buf, len);
                g_shell_ctx.saved_input[len] = '\0';
            }
            hist_cmd = history_get_prev(&g_shell_ctx.history);
            if (hist_cmd != NULL) {
                line_editor_set_content(&g_shell_ctx.editor, hist_cmd);
                redraw_line();
            }
            break;

        case ESC_RESULT_DOWN:
            hist_cmd = history_get_next(&g_shell_ctx.history);
            if (hist_cmd != NULL) {
                line_editor_set_content(&g_shell_ctx.editor, hist_cmd);
            } else {
                /* Restore saved input */
                line_editor_set_content(&g_shell_ctx.editor,
                                        g_shell_ctx.saved_input);
            }
            redraw_line();
            break;

        case ESC_RESULT_LEFT:
            if (line_editor_get_cursor(&g_shell_ctx.editor) > 0) {
                line_editor_move_cursor(&g_shell_ctx.editor, -1);
                shell_puts(ANSI_CURSOR_LEFT);
            }
            break;

        case ESC_RESULT_RIGHT:
            if (line_editor_get_cursor(&g_shell_ctx.editor) <
                line_editor_get_length(&g_shell_ctx.editor)) {
                line_editor_move_cursor(&g_shell_ctx.editor, 1);
                shell_puts(ANSI_CURSOR_RIGHT);
            }
            break;

        case ESC_RESULT_HOME:
            line_editor_move_to_start(&g_shell_ctx.editor);
            redraw_line();
            break;

        case ESC_RESULT_END:
            line_editor_move_to_end(&g_shell_ctx.editor);
            redraw_line();
            break;

        case ESC_RESULT_DELETE:
            if (line_editor_delete_char(&g_shell_ctx.editor)) {
                refresh_line_from_cursor();
            }
            break;

        default:
            break;
    }
}

/**
 * \brief           Handle printable character input
 * \param[in]       c: Character to insert
 */
static void
handle_printable_char(char c) {
    /* Check buffer full (Requirement 4.7) */
    if (line_editor_get_length(&g_shell_ctx.editor) >=
        g_shell_ctx.config.cmd_buffer_size - 1) {
        return;
    }

    if (line_editor_insert_char(&g_shell_ctx.editor, c)) {
        /* Echo character (Requirement 4.6) */
        shell_putchar(c);

        /* If not at end, refresh rest of line */
        if (line_editor_get_cursor(&g_shell_ctx.editor) <
            line_editor_get_length(&g_shell_ctx.editor)) {
            refresh_line_from_cursor();
        }
    }
}

/**
 * \brief           Handle Tab key for auto-completion
 */
static void
handle_tab_completion(void) {
    completion_result_t result;
    const char* input = line_editor_get_buffer(&g_shell_ctx.editor);
    int input_len = (int)line_editor_get_length(&g_shell_ctx.editor);
    int cursor_pos = (int)line_editor_get_cursor(&g_shell_ctx.editor);

    shell_status_t status = autocomplete_process(input, input_len,
                                                  cursor_pos, &result);
    if (status != SHELL_OK) {
        return;
    }

    if (result.match_count == 0) {
        /* No matches - do nothing (Requirement 6.3) */
        return;
    } else if (result.match_count == 1) {
        /* Single match - complete and add space (Requirement 6.4) */
        line_editor_clear(&g_shell_ctx.editor);
        for (int i = 0; result.matches[0][i] != '\0'; i++) {
            line_editor_insert_char(&g_shell_ctx.editor, result.matches[0][i]);
        }
        line_editor_insert_char(&g_shell_ctx.editor, ' ');
        redraw_line();
    } else {
        /* Multiple matches - show options (Requirement 6.2) */
        shell_puts("\r\n");
        autocomplete_show_matches(&result);

        /* Complete common prefix if any */
        if (result.common_prefix_len > input_len) {
            char prefix[SHELL_MAX_CMD_NAME + 1];
            int prefix_len = autocomplete_get_common_prefix(&result, prefix,
                                                            sizeof(prefix));
            if (prefix_len > 0) {
                line_editor_clear(&g_shell_ctx.editor);
                for (int i = 0; i < prefix_len; i++) {
                    line_editor_insert_char(&g_shell_ctx.editor, prefix[i]);
                }
            }
        }

        /* Redraw prompt and input */
        shell_print_prompt();
        shell_puts(line_editor_get_buffer(&g_shell_ctx.editor));
    }
}

/**
 * \brief           Handle control character input
 * \param[in]       c: Control character
 */
static void
handle_control_char(uint8_t c) {
    switch (c) {
        case SHELL_KEY_ENTER:
            /* Execute command (Requirement 4.1) */
            shell_puts("\r\n");
            execute_command_line();
            break;

        case SHELL_KEY_BACKSPACE:
        case SHELL_KEY_DELETE:
            /* Delete character before cursor (Requirement 4.2) */
            if (line_editor_backspace(&g_shell_ctx.editor)) {
                shell_puts("\b");
                refresh_line_from_cursor();
            }
            break;

        case SHELL_KEY_TAB:
            /* Auto-completion */
            handle_tab_completion();
            break;

        case SHELL_KEY_CTRL_C:
            /* Cancel input (Requirement 4.4) */
            shell_puts("^C\r\n");
            line_editor_clear(&g_shell_ctx.editor);
            history_reset_browse(&g_shell_ctx.history);
            shell_print_prompt();
            break;

        case SHELL_KEY_CTRL_L:
            /* Clear screen (Requirement 4.5) */
            shell_clear_screen();
            shell_print_prompt();
            shell_puts(line_editor_get_buffer(&g_shell_ctx.editor));
            redraw_line();
            break;

        case SHELL_KEY_CTRL_A:
            /* Move to start (Requirement 4.10) */
            line_editor_move_to_start(&g_shell_ctx.editor);
            redraw_line();
            break;

        case SHELL_KEY_CTRL_E:
            /* Move to end (Requirement 4.11) */
            line_editor_move_to_end(&g_shell_ctx.editor);
            redraw_line();
            break;

        case SHELL_KEY_CTRL_K:
            /* Delete to end (Requirement 4.13) */
            line_editor_delete_to_end(&g_shell_ctx.editor);
            shell_puts(ANSI_ERASE_LINE);
            break;

        case SHELL_KEY_CTRL_U:
            /* Delete to start (Requirement 4.14) */
            line_editor_delete_to_start(&g_shell_ctx.editor);
            redraw_line();
            break;

        case SHELL_KEY_CTRL_W:
            /* Delete word (Requirement 4.15) */
            line_editor_delete_word(&g_shell_ctx.editor);
            redraw_line();
            break;

        default:
            break;
    }
}

/**
 * \}
 */


/**
 * \name            Command Execution
 * \{
 */

/**
 * \brief           Execute the current command line
 *
 * Parses and executes the command in the line editor buffer.
 */
static void
execute_command_line(void) {
    const char* input = line_editor_get_buffer(&g_shell_ctx.editor);

    /* Skip empty input */
    if (line_editor_get_length(&g_shell_ctx.editor) == 0) {
        shell_print_prompt();
        return;
    }

    /* Add to history (Requirement 5.1) */
    history_add(&g_shell_ctx.history, input);
    history_reset_browse(&g_shell_ctx.history);

    /* Copy input to parse buffer (parsing modifies the string) */
    char parse_buffer[SHELL_MAX_CMD_BUFFER_SIZE];
    size_t input_len = strlen(input);
    if (input_len >= sizeof(parse_buffer)) {
        input_len = sizeof(parse_buffer) - 1;
    }
    memcpy(parse_buffer, input, input_len);
    parse_buffer[input_len] = '\0';

    /* Parse command line (Requirement 3.1) */
    parsed_command_t parsed;
    shell_status_t status = parse_command_line(parse_buffer, &parsed);

    if (status != SHELL_OK || parsed.argc == 0) {
        shell_print_prompt();
        line_editor_clear(&g_shell_ctx.editor);
        return;
    }

    /* Look up command (Requirement 3.2) */
    const shell_command_t* cmd = shell_get_command(parsed.cmd_name);

    if (cmd == NULL) {
        /* Unknown command (Requirement 3.3) */
        shell_printf("Unknown command: %s\r\n", parsed.cmd_name);
    } else {
        /* Execute command handler */
        int ret = cmd->handler(parsed.argc, parsed.argv);

        /* Print error code if non-zero (Requirement 3.7) */
        if (ret != 0) {
            shell_printf("Error: command returned %d\r\n", ret);
        }
    }

    /* Clear input and show new prompt */
    line_editor_clear(&g_shell_ctx.editor);
    shell_print_prompt();
}

/**
 * \}
 */

/**
 * \name            Public API Implementation
 * \{
 */

/**
 * \brief           Initialize the Shell module
 * \param[in]       config: Pointer to configuration structure
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t
shell_init(const shell_config_t* config) {
    shell_status_t status;

    /* Check if already initialized (Requirement 1.3) */
    if (g_shell_ctx.initialized) {
        return SHELL_ERROR_ALREADY_INIT;
    }

    /* Validate configuration (Requirement 1.2) */
    status = validate_config(config);
    if (status != SHELL_OK) {
        g_shell_ctx.last_error = status;
        return status;
    }

    /* Clear context */
    memset(&g_shell_ctx, 0, sizeof(g_shell_ctx));

    /* Copy configuration */
    g_shell_ctx.config = *config;

    /* Set prompt (Requirement 1.4) */
    if (config->prompt != NULL) {
        size_t prompt_len = strlen(config->prompt);
        if (prompt_len > SHELL_MAX_PROMPT_LEN) {
            prompt_len = SHELL_MAX_PROMPT_LEN;
        }
        memcpy(g_shell_ctx.prompt, config->prompt, prompt_len);
        g_shell_ctx.prompt[prompt_len] = '\0';
    } else {
        size_t default_len = strlen(SHELL_DEFAULT_PROMPT);
        if (default_len > SHELL_MAX_PROMPT_LEN) {
            default_len = SHELL_MAX_PROMPT_LEN;
        }
        memcpy(g_shell_ctx.prompt, SHELL_DEFAULT_PROMPT, default_len);
        g_shell_ctx.prompt[default_len] = '\0';
    }

    /* Allocate command buffer */
    g_shell_ctx.cmd_buffer = (char*)malloc(config->cmd_buffer_size);
    if (g_shell_ctx.cmd_buffer == NULL) {
        g_shell_ctx.last_error = SHELL_ERROR_NO_MEMORY;
        return SHELL_ERROR_NO_MEMORY;
    }
    memset(g_shell_ctx.cmd_buffer, 0, config->cmd_buffer_size);

    /* Allocate saved input buffer */
    g_shell_ctx.saved_input = (char*)malloc(config->cmd_buffer_size);
    if (g_shell_ctx.saved_input == NULL) {
        free(g_shell_ctx.cmd_buffer);
        g_shell_ctx.cmd_buffer = NULL;
        g_shell_ctx.last_error = SHELL_ERROR_NO_MEMORY;
        return SHELL_ERROR_NO_MEMORY;
    }
    memset(g_shell_ctx.saved_input, 0, config->cmd_buffer_size);

    /* Allocate history storage */
    size_t entry_size = config->cmd_buffer_size;
    g_shell_ctx.history_storage = (char*)malloc(config->history_depth *
                                                 entry_size);
    if (g_shell_ctx.history_storage == NULL) {
        free(g_shell_ctx.cmd_buffer);
        free(g_shell_ctx.saved_input);
        g_shell_ctx.cmd_buffer = NULL;
        g_shell_ctx.saved_input = NULL;
        g_shell_ctx.last_error = SHELL_ERROR_NO_MEMORY;
        return SHELL_ERROR_NO_MEMORY;
    }
    memset(g_shell_ctx.history_storage, 0, config->history_depth * entry_size);

    /* Allocate history entry pointers */
    g_shell_ctx.history_entries = (char**)malloc(config->history_depth *
                                                  sizeof(char*));
    if (g_shell_ctx.history_entries == NULL) {
        free(g_shell_ctx.cmd_buffer);
        free(g_shell_ctx.saved_input);
        free(g_shell_ctx.history_storage);
        g_shell_ctx.cmd_buffer = NULL;
        g_shell_ctx.saved_input = NULL;
        g_shell_ctx.history_storage = NULL;
        g_shell_ctx.last_error = SHELL_ERROR_NO_MEMORY;
        return SHELL_ERROR_NO_MEMORY;
    }

    /* Set up history entry pointers */
    for (uint8_t i = 0; i < config->history_depth; i++) {
        g_shell_ctx.history_entries[i] = &g_shell_ctx.history_storage[i *
                                                                      entry_size];
    }

    /* Initialize line editor */
    line_editor_init(&g_shell_ctx.editor, g_shell_ctx.cmd_buffer,
                     config->cmd_buffer_size);

    /* Initialize history manager */
    history_init(&g_shell_ctx.history, g_shell_ctx.history_entries,
                 config->history_depth, (uint16_t)entry_size);

    /* Initialize escape state */
    reset_escape_state();

    /* Mark as initialized */
    g_shell_ctx.initialized = true;
    g_shell_ctx.last_error = SHELL_OK;

    return SHELL_OK;
}

/**
 * \brief           Deinitialize the Shell module
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t
shell_deinit(void) {
    /* Check if initialized */
    if (!g_shell_ctx.initialized) {
        return SHELL_ERROR_NOT_INIT;
    }

    /* Deinitialize history */
    history_deinit(&g_shell_ctx.history);

    /* Free allocated memory */
    if (g_shell_ctx.cmd_buffer != NULL) {
        free(g_shell_ctx.cmd_buffer);
        g_shell_ctx.cmd_buffer = NULL;
    }

    if (g_shell_ctx.saved_input != NULL) {
        free(g_shell_ctx.saved_input);
        g_shell_ctx.saved_input = NULL;
    }

    if (g_shell_ctx.history_storage != NULL) {
        free(g_shell_ctx.history_storage);
        g_shell_ctx.history_storage = NULL;
    }

    if (g_shell_ctx.history_entries != NULL) {
        free(g_shell_ctx.history_entries);
        g_shell_ctx.history_entries = NULL;
    }

    /* Clear context */
    memset(&g_shell_ctx, 0, sizeof(g_shell_ctx));

    return SHELL_OK;
}

/**
 * \brief           Check if Shell is initialized
 * \return          true if initialized, false otherwise
 */
bool
shell_is_initialized(void) {
    return g_shell_ctx.initialized;
}

/**
 * \brief           Process Shell input
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t
shell_process(void) {
    uint8_t c;
    int bytes_read;
    const shell_backend_t* backend;

    /* Check initialization */
    if (!g_shell_ctx.initialized) {
        g_shell_ctx.last_error = SHELL_ERROR_NOT_INIT;
        return SHELL_ERROR_NOT_INIT;
    }

    /* Get backend from backend module */
    backend = shell_get_backend();

    /* Check backend (Requirement 8.6) */
    if (backend == NULL || backend->read == NULL) {
        g_shell_ctx.last_error = SHELL_ERROR_NO_BACKEND;
        return SHELL_ERROR_NO_BACKEND;
    }

    /* Non-blocking read (Requirement 9.2, 9.3) */
    bytes_read = backend->read(&c, 1);
    if (bytes_read <= 0) {
        return SHELL_OK;
    }

    /* Process escape sequences */
    if (g_shell_ctx.escape_state != ESC_STATE_NORMAL || c == SHELL_KEY_ESCAPE) {
        escape_result_t result = process_escape_char(c);
        if (result != ESC_RESULT_NONE && result != ESC_RESULT_INVALID) {
            handle_escape_result(result);
        }
        return SHELL_OK;
    }

    /* Handle control characters */
    if (c < 0x20 || c == 0x7F) {
        handle_control_char(c);
        return SHELL_OK;
    }

    /* Handle printable characters (Requirement 4.3) */
    handle_printable_char((char)c);

    return SHELL_OK;
}

/**
 * \brief           Get the last error code
 * \return          Last error code
 */
shell_status_t
shell_get_last_error(void) {
    return g_shell_ctx.last_error;
}

/**
 * \brief           Get Shell version string
 * \return          Version string
 */
const char*
shell_get_version(void) {
    return SHELL_VERSION;
}

/**
 * \brief           Print the Shell prompt
 */
void
shell_print_prompt(void) {
    const shell_backend_t* backend = shell_get_backend();
    if (backend != NULL && backend->write != NULL) {
        shell_puts(g_shell_ctx.prompt);
    }
}

/**
 * \brief           Clear the terminal screen
 */
void
shell_clear_screen(void) {
    shell_puts(ANSI_CLEAR_SCREEN);
}

/**
 * \brief           Get the history manager
 * \return          Pointer to history manager, or NULL if not initialized
 */
history_manager_t*
shell_get_history_manager(void) {
    if (!g_shell_ctx.initialized) {
        return NULL;
    }
    return &g_shell_ctx.history;
}

/**
 * \}
 */

/**
 * \name            Error Handling Functions
 * \brief           Error handling and recovery (Requirements 10.1-10.5)
 * \{
 */

/**
 * \brief           Error message strings for each status code
 */
static const char* const g_error_messages[] = {
    "Success",                              /* SHELL_OK */
    "Generic error",                        /* SHELL_ERROR */
    "Invalid parameter",                    /* SHELL_ERROR_INVALID_PARAM */
    "Shell not initialized",                /* SHELL_ERROR_NOT_INIT */
    "Shell already initialized",            /* SHELL_ERROR_ALREADY_INIT */
    "Memory allocation failed",             /* SHELL_ERROR_NO_MEMORY */
    "Item not found",                       /* SHELL_ERROR_NOT_FOUND */
    "Item already exists",                  /* SHELL_ERROR_ALREADY_EXISTS */
    "No backend configured",                /* SHELL_ERROR_NO_BACKEND */
    "Buffer is full",                       /* SHELL_ERROR_BUFFER_FULL */
};

/** Number of error messages */
#define SHELL_ERROR_MESSAGE_COUNT \
    (sizeof(g_error_messages) / sizeof(g_error_messages[0]))

/**
 * \brief           Get error message string for a status code
 * \param[in]       status: Status code to get message for
 * \return          Pointer to error message string (never NULL)
 */
const char*
shell_get_error_message(shell_status_t status) {
    if ((size_t)status < SHELL_ERROR_MESSAGE_COUNT) {
        return g_error_messages[status];
    }
    return "Unknown error";
}

/**
 * \brief           Print error message to shell output
 * \param[in]       status: Status code to print message for
 */
void
shell_print_error(shell_status_t status) {
    const char* msg = shell_get_error_message(status);
    shell_printf("Error: %s (code %d)\r\n", msg, (int)status);
}

/**
 * \brief           Print error message with context
 * \param[in]       status: Status code to print message for
 * \param[in]       context: Additional context string (can be NULL)
 */
void
shell_print_error_context(shell_status_t status, const char* context) {
    const char* msg = shell_get_error_message(status);
    if (context != NULL && context[0] != '\0') {
        shell_printf("Error: %s - %s (code %d)\r\n", msg, context, (int)status);
    } else {
        shell_print_error(status);
    }
}

/**
 * \brief           Reset shell to a known good state after error
 * \return          SHELL_OK on successful recovery, error code otherwise
 */
shell_status_t
shell_recover(void) {
    /* Check if shell is initialized */
    if (!g_shell_ctx.initialized) {
        return SHELL_ERROR_NOT_INIT;
    }

    /* Clear the line editor buffer */
    line_editor_clear(&g_shell_ctx.editor);

    /* Reset escape sequence state */
    reset_escape_state();

    /* Reset history browsing state */
    history_reset_browse(&g_shell_ctx.history);

    /* Clear saved input */
    if (g_shell_ctx.saved_input != NULL) {
        g_shell_ctx.saved_input[0] = '\0';
    }

    /* Clear last error */
    g_shell_ctx.last_error = SHELL_OK;

    /* Print new prompt to indicate recovery */
    shell_puts("\r\n");
    shell_print_prompt();

    return SHELL_OK;
}

/**
 * \}
 */

/**
 * \}
 */

