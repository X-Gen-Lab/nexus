/** Stateless formatting core. SPDX-License-Identifier: MIT */
#include "log/log_format.h"
#include <stdio.h>
/*---------------------------------------------------------------------------*/
/* Level Name Tables                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Full level names
 */
static const char* const s_level_names[] = {"TRACE", "DEBUG", "INFO", "WARN",
                                            "ERROR", "FATAL", "NONE"};

/**
 * \brief           Short level names (single character)
 */
static const char s_level_short[] = {'T', 'D', 'I', 'W', 'E', 'F', 'N'};

/**
 * \brief           ANSI color codes for each level
 */
static const char* const s_level_colors[] = {
    "\033[37m", /* TRACE - white */
    "\033[36m", /* DEBUG - cyan */
    "\033[32m", /* INFO - green */
    "\033[33m", /* WARN - yellow */
    "\033[31m", /* ERROR - red */
    "\033[35m", /* FATAL - magenta */
    "\033[0m"   /* NONE - reset */
};

/**
 * \brief           ANSI color reset code
 */
static const char* const s_color_reset = "\033[0m";

/*---------------------------------------------------------------------------*/
/* Formatting Helper Functions                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get level name string
 * \param[in]       level: Log level
 * \return          Level name string
 */
static const char* log_level_name(log_level_t level) {
    if (level > LOG_LEVEL_NONE) {
        return "UNKNOWN";
    }
    return s_level_names[level];
}

/**
 * \brief           Get short level name (single char)
 * \param[in]       level: Log level
 * \return          Short level character
 */
static char log_level_short(log_level_t level) {
    if (level > LOG_LEVEL_NONE) {
        return '?';
    }
    return s_level_short[level];
}

/**
 * \brief           Get color code for level
 * \param[in]       level: Log level
 * \return          ANSI color code string
 */
static const char* log_level_color(log_level_t level) {
    if (level > LOG_LEVEL_NONE) {
        return s_color_reset;
    }
    return s_level_colors[level];
}

/**
 * \brief           Extract filename from full path
 * \param[in]       path: Full file path
 * \return          Pointer to filename portion
 */
static const char* log_extract_filename(const char* path) {
    if (path == NULL) {
        return "unknown";
    }

    const char* filename = path;
    const char* p = path;

    while (*p != '\0') {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }

    return filename;
}

/**
 * \brief           Format message using pattern
 * \param[out]      buf: Output buffer
 * \param[in]       buf_size: Buffer size
 * \param[in]       level: Log level
 * \param[in]       module: Module name
 * \param[in]       file: Source file
 * \param[in]       line: Source line
 * \param[in]       func: Function name
 * \param[in]       user_msg: User message (already formatted)
 * \return          Number of characters written (excluding null terminator)
 */
size_t log_format_record(char* buf, size_t buf_size,
                          const log_format_options_t* options,
                          const log_record_t* record) {
    if (!record || (unsigned int)record->level > LOG_LEVEL_NONE) {
        if (buf && buf_size) buf[0] = '\0';
        return 0;
    }
    log_level_t level = record->level;
    const char* module = record->module;
    const char* file = record->file;
    int line = record->line;
    const char* func = record->function;
    const char* user_msg = record->message;
    if (buf == NULL || buf_size == 0) {
        return 0;
    }

    const char* pattern = options ? options->pattern : NULL;
    if (pattern == NULL) {
        pattern = LOG_DEFAULT_FORMAT;
    }

    size_t pos = 0;
    const char* p = pattern;

    while (*p != '\0' && pos < buf_size - 1) {
        if (*p == '%' && *(p + 1) != '\0') {
            char token = *(p + 1);
            int written = 0;

            switch (token) {
                case 'T': /* Timestamp in milliseconds */
                    written = snprintf(buf + pos, buf_size - pos, "%lu",
                                       (unsigned long)record->timestamp_ms);
                    break;

                case 't':
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       record->clock_text ? record->clock_text : "--:--:--");
                    break;

                case 'L': /* Level name (full) */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       log_level_name(level));
                    break;

                case 'l': /* Level name (short) */
                    if (pos < buf_size - 1) {
                        buf[pos] = log_level_short(level);
                        written = 1;
                    }
                    break;

                case 'M': /* Module name */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       module ? module : "default");
                    break;

                case 'F': /* File name */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       log_extract_filename(file));
                    break;

                case 'f': /* Function name */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       func ? func : "unknown");
                    break;

                case 'n': /* Line number */
                    written = snprintf(buf + pos, buf_size - pos, "%d", line);
                    break;

                case 'm': /* Message */
                    written = snprintf(buf + pos, buf_size - pos, "%s",
                                       user_msg ? user_msg : "");
                    break;

                case 'c': /* Color code */
                    if (options && options->color_enabled) {
                        written = snprintf(buf + pos, buf_size - pos, "%s",
                                           log_level_color(level));
                    }
                    break;

                case 'C': /* Color reset */
                    if (options && options->color_enabled) {
                        written = snprintf(buf + pos, buf_size - pos, "%s",
                                           s_color_reset);
                    }
                    break;

                case '%': /* Literal percent */
                    if (pos < buf_size - 1) {
                        buf[pos] = '%';
                        written = 1;
                    }
                    break;

                default: /* Unknown token, copy as-is */
                    if (pos < buf_size - 2) {
                        buf[pos] = '%';
                        buf[pos + 1] = token;
                        written = 2;
                    }
                    break;
            }

            if (written > 0) {
                size_t available = buf_size - pos - 1;
                pos += (size_t)written < available ? (size_t)written : available;
            }
            p += 2; /* Skip % and token */
        } else {
            /* Copy regular character */
            buf[pos++] = *p++;
        }
    }

    /* Null terminate */
    buf[pos] = '\0';

    return pos;
}
