/**
 * \file            components_log.c
 * \brief           Explicit logger recursion, stop and sink error regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            abort();                                                           \
        }                                                                      \
    } while (0)

/** \brief Sink fixture proves bytes are borrowed only through the call. */
typedef struct {
    nx_log_t* logger;
    unsigned calls;
    bool stop;
    nx_result_t result;
} fixture_t;

/** \brief Propagate real sink errors and reject recursive writes. */
static nx_result_t write_bytes(void* context, nx_log_level_t level,
                               const void* bytes, size_t length,
                               nx_time_us_t deadline) {
    fixture_t* fixture = context;
    CHECK(level == NX_LOG_INFO && deadline == 100);
    CHECK(length == 4 && memcmp(bytes, "test", 4) == 0);
    ++fixture->calls;
    CHECK(nx_log_write(fixture->logger, NX_LOG_INFO, "test", 4, deadline) ==
          NX_ERROR_BUSY);
    if (fixture->stop) {
        CHECK(nx_log_stop(fixture->logger) == NX_ERROR_BUSY);
    }
    return fixture->result;
}

/**
 * \brief           Run explicit filtering/error/stop behavior without global
 *                  discovery.
 */
int main(void) {
    nx_log_t logger;
    fixture_t fixture = {&logger, 0, false, NX_ERROR_IO};
    nx_log_sink_port_t sink = {&fixture, write_bytes, NULL};
    CHECK(nx_log_init(&logger, sink, (nx_log_level_t)-1) == NX_ERROR_INVALID);
    CHECK(nx_log_init(&logger, sink, (nx_log_level_t)(NX_LOG_ERROR + 1)) ==
          NX_ERROR_INVALID);
    CHECK(nx_log_init(&logger, sink, NX_LOG_INFO) == NX_SUCCESS);
    CHECK(nx_log_write(&logger, (nx_log_level_t)-1, "test", 4, 100) ==
          NX_ERROR_INVALID);
    CHECK(nx_log_write(&logger, (nx_log_level_t)(NX_LOG_ERROR + 1), "test", 4,
                       100) == NX_ERROR_INVALID);
    CHECK(nx_log_write(&logger, NX_LOG_DEBUG, "test", 4, 100) == NX_SUCCESS);
    CHECK(fixture.calls == 0);
    CHECK(nx_log_write(&logger, NX_LOG_INFO, "test", 4, 100) == NX_ERROR_IO);
    CHECK(fixture.calls == 1 && logger.busy == 0);
    CHECK(nx_log_flush(&logger, 100) == NX_ERROR_UNSUPPORTED);
    fixture.stop = true;
    fixture.result = NX_SUCCESS;
    CHECK(nx_log_write(&logger, NX_LOG_INFO, "test", 4, 100) == NX_SUCCESS);
    CHECK(nx_log_stop(&logger) == NX_SUCCESS);
    CHECK(nx_log_write(&logger, NX_LOG_INFO, "test", 4, 100) == NX_ERROR_STATE);
    CHECK(nx_log_flush(&logger, 100) == NX_ERROR_STATE);
    puts("Logger sink errors, recursion, filtering and stop drain passed");
    return 0;
}
