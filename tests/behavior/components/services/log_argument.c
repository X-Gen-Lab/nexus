/**
 * \file            log_argument.c
 * \brief           Exercise invalid log levels through the actual C11 API
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-26
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/log.h"

nx_result_t nx_test_log_init_level(nx_log_t* logger, nx_log_sink_port_t sink,
                                   int level);
nx_result_t nx_test_log_write_level(nx_log_t* logger, int level,
                                    const void* bytes, size_t length,
                                    nx_time_us_t deadline);

nx_result_t nx_test_log_init_level(nx_log_t* logger, nx_log_sink_port_t sink,
                                   int level) {
    return nx_log_init(logger, sink, (nx_log_level_t)level);
}

nx_result_t nx_test_log_write_level(nx_log_t* logger, int level,
                                    const void* bytes, size_t length,
                                    nx_time_us_t deadline) {
    return nx_log_write(logger, (nx_log_level_t)level, bytes, length, deadline);
}
