/** Stateless logging formatter; no OS, HAL, clock or allocation. SPDX-License-Identifier: MIT */
#ifndef LOG_FORMAT_H
#define LOG_FORMAT_H
#include "log_def.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char* pattern;
    bool color_enabled;
} log_format_options_t;
typedef struct {
    log_level_t level;
    const char* module;
    const char* file;
    int line;
    const char* function;
    const char* message;
    uint32_t timestamp_ms;
    const char* clock_text; /**< Optional externally supplied HH:MM:SS. */
} log_record_t;
/** Caller owns all storage for this synchronous bounded formatting call. The
 * returned length excludes NUL and is always < capacity. No callbacks occur. */
size_t log_format_record(char* output, size_t capacity,
                          const log_format_options_t* options,
                          const log_record_t* record);
#ifdef __cplusplus
}
#endif
#endif
