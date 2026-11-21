/**
 * \file            log_format_impl.h
 * \brief           Private bounded rendering shared by Log and shell
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_LOG_FORMAT_IMPL_H
#define NEXUS_LOG_FORMAT_IMPL_H
#include "nexus/core/status.h"
#include <stdarg.h>

/**
 * \brief Render the admitted integer/string subset without libc formatting.
 * \note Private task-context helper; length is valid only on SUCCESS. Scratch
 *       is terminated for valid arguments and positive capacity. Input,
 *       scratch and output-count spans are separate and stable through return.
 */
nx_result_t nx_log_render(char* buffer, size_t capacity, size_t* length,
                          const char* format, va_list arguments);
#endif /* NEXUS_LOG_FORMAT_IMPL_H */
