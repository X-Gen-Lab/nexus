/**
 * \file            application.c
 * \brief           Successful PSA lifecycle and bounded TLS/MQTT consumption
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include <stdlib.h>

nx_time_us_t nx_mqtt_port_now_us(void) {
    return nx_time_now_us();
}

void nx_mqtt_port_assert(const char* file, unsigned line) {
    (void)file;
    (void)line;
    abort();
}

static void on_event(void* context, const nx_mqtt_event_t* event) {
    (void)context;
    (void)event;
}

nx_result_t security_api_run(nx_tls_runtime_t* runtime,
                             const nx_tls_runtime_config_t* runtime_config,
                             nx_tls_session_t* tls,
                             const nx_tls_config_t* tls_config, nx_mqtt_t* mqtt,
                             const nx_mqtt_memory_t* mqtt_memory) {
    const nx_time_us_t limit = nx_deadline_after(nx_time_now_us(), 1000000U);
    nx_result_t result = nx_tls_runtime_start(runtime, runtime_config, limit);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_tls_initialize(tls, tls_config, limit);
    if (result == NX_SUCCESS) {
        bool observed_want = false;
        /* A protocol-only state transition may return BUSY before BIO is used.
         */
        for (unsigned step = 0U; step < 8U; ++step) {
            nx_tls_diagnostic_t diagnostic;
            result = nx_tls_handshake_step(tls, limit);
            if (result != NX_ERROR_BUSY ||
                nx_tls_diagnostic(tls, &diagnostic) != NX_SUCCESS) {
                break;
            }
            if (diagnostic.want_read || diagnostic.want_write) {
                observed_want = true;
                break;
            }
        }
        if (result == NX_ERROR_BUSY && observed_want) {
            const nx_mqtt_config_t mqtt_config = {*nx_tls_stream(tls), on_event,
                                                  NULL, NULL, NULL};
            result = nx_mqtt_init(mqtt, &mqtt_config, mqtt_memory);
            if (result == NX_SUCCESS) {
                result = nx_mqtt_deinit(mqtt);
            }
        } else {
            result = NX_ERROR_STATE;
        }
    }
    nx_result_t cleanup = nx_tls_abort(tls);
    if (cleanup != NX_SUCCESS) {
        return cleanup;
    }
    cleanup = nx_tls_runtime_stop(runtime);
    return cleanup == NX_SUCCESS ? result : cleanup;
}
