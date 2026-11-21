/**
 * \file            log_format.h
 * \brief           Caller-buffer formatting before log publication
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LOG_FORMAT_H
#define NEXUS_COMPONENTS_LOG_FORMAT_H
#include "nexus/components/log.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Format one complete record into exclusive caller storage.
 *
 * \param[in,out]   logger: Live logger, with its existing task-only contract.
 *
 * \param[in]       level: Record severity.
 *
 * \param[out]      buffer: Exclusive scratch, disjoint from logger and inputs.
 *
 * \param[in]       capacity: Positive scratch capacity, including terminating
 * NUL.
 *
 * \param[in]       deadline: Existing sink's absolute monotonic deadline.
 *
 * \param[in]       format: Valid format terminated within 256 bytes.
 *
 * \param[in]       arguments: Arguments matching the admitted format.
 *
 * \return          SUCCESS or log error; INVALID for unsupported format;
 *                  OVERFLOW when a complete record does not fit.
 *
 * \note            Task-only; uses bounded allocation-free rendering. Integer,
 *                  pointer, character and precision-limited string conversions
 *                  are admitted. Width/precision are literal and at most 1024;
 *                  dynamic widths, floating point and %n are rejected. Every
 *                  string input supplies its explicit precision and valid
 *                  readable storage. No partial record reaches the sink. No
 *                  heap, shared scratch, implicit newline or worker is added.
 */
nx_result_t nx_log_vprintf(nx_log_t* logger, nx_log_level_t level, char* buffer,
                           size_t capacity, nx_time_us_t deadline,
                           const char* format, va_list arguments);

/**
 * \brief           Variadic entry with the same bounds and borrow contract.
 *
 * \param[in,out]   logger: Live task-only logger.
 *
 * \param[in]       level: Record severity.
 *
 * \param[out]      buffer: Exclusive scratch through return.
 *
 * \param[in]       capacity: Scratch bytes, including terminating NUL.
 *
 * \param[in]       deadline: Absolute sink deadline.
 *
 * \param[in]       format: Admitted format followed by matching arguments.
 *
 * \return          Same result as nx_log_vprintf.
 */
nx_result_t nx_log_printf(nx_log_t* logger, nx_log_level_t level, char* buffer,
                          size_t capacity, nx_time_us_t deadline,
                          const char* format, ...);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_LOG_FORMAT_H */
