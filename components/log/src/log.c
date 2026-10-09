/**
 * \file            log.c
 * \brief           Explicit bounded synchronous logging without global state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/log.h"

/** \brief Acquire only nonblocking logger metadata, never wait under a lock. */
static nx_result_t begin_write(nx_log_t* logger) {
    if (__atomic_load_n(&logger->stopped, __ATOMIC_ACQUIRE) != 0) {
        return NX_ERROR_STATE;
    }
    uint32_t expected = 0;
    if (!__atomic_compare_exchange_n(&logger->busy, &expected, 1, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        return NX_ERROR_BUSY;
    }
    if (__atomic_load_n(&logger->stopped, __ATOMIC_ACQUIRE) != 0) {
        __atomic_store_n(&logger->busy, 0, __ATOMIC_RELEASE);
        return NX_ERROR_STATE;
    }
    return NX_SUCCESS;
}

/** \brief Bind exactly the sink chosen by the external caller. */
nx_result_t nx_log_init(nx_log_t* logger, nx_log_sink_port_t sink,
                        nx_log_level_t minimum) {
    if (logger == NULL || sink.write == NULL ||
        (unsigned)minimum > (unsigned)NX_LOG_ERROR) {
        return NX_ERROR_INVALID;
    }
    logger->sink = sink;
    logger->minimum = minimum;
    logger->busy = 0;
    logger->stopped = 0;
    return NX_SUCCESS;
}

/** \brief Deliver bytes synchronously and return the sink's actual result. */
nx_result_t nx_log_write(nx_log_t* logger, nx_log_level_t level,
                         const void* bytes, size_t length,
                         nx_time_us_t deadline) {
    if (logger == NULL || (bytes == NULL && length != 0) ||
        (unsigned)level > (unsigned)NX_LOG_ERROR) {
        return NX_ERROR_INVALID;
    }
    if (__atomic_load_n(&logger->stopped, __ATOMIC_ACQUIRE) != 0) {
        return NX_ERROR_STATE;
    }
    if (level < logger->minimum) {
        return NX_SUCCESS;
    }
    nx_result_t result = begin_write(logger);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = logger->sink.write(logger->sink.context, level, bytes, length,
                                deadline);
    __atomic_store_n(&logger->busy, 0, __ATOMIC_RELEASE);
    return result;
}

/** \brief Explicitly flush only a declared synchronous capability. */
nx_result_t nx_log_flush(nx_log_t* logger, nx_time_us_t deadline) {
    if (logger == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = begin_write(logger);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = logger->sink.flush != NULL
                 ? logger->sink.flush(logger->sink.context, deadline)
                 : NX_ERROR_UNSUPPORTED;
    __atomic_store_n(&logger->busy, 0, __ATOMIC_RELEASE);
    return result;
}

/** \brief Close admission while preserving an already active sink call. */
nx_result_t nx_log_stop(nx_log_t* logger) {
    if (logger == NULL) {
        return NX_ERROR_INVALID;
    }
    __atomic_store_n(&logger->stopped, 1, __ATOMIC_RELEASE);
    return __atomic_load_n(&logger->busy, __ATOMIC_ACQUIRE) == 0
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}
