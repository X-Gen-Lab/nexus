/**
 * \file            application.c
 * \brief           Real bounded TLS/MQTT calls, without supplier storage access
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"

#define OPERATION_US 1000000U

typedef struct {
    bool subscribed;
    bool published;
    bool unsubscribed;
} events_t;

/* This boot-only fixture retains callback storage even on a failed join. */
static events_t boot_events;

static nx_time_us_t deadline(void) {
    return nx_deadline_after(nx_time_now_us(), OPERATION_US);
}

nx_time_us_t nx_mqtt_port_now_us(void) {
    return nx_time_now_us();
}

void nx_mqtt_port_assert(const char* file, unsigned line) {
    (void)file;
    (void)line;
    for (;;) {
    }
}

static void on_event(void* context, const nx_mqtt_event_t* event) {
    events_t* events = context;
    if (event->status == NX_SUCCESS) {
        if (event->kind == NX_MQTT_EVENT_SUBACK) {
            events->subscribed = true;
        } else if (event->kind == NX_MQTT_EVENT_PUBACK) {
            events->published = true;
        } else if (event->kind == NX_MQTT_EVENT_UNSUBACK) {
            events->unsubscribed = true;
        }
    }
}

static nx_result_t idle(void* context, nx_time_us_t limit) {
    if (nx_time_now_us() >= limit) {
        return NX_ERROR_TIMEOUT;
    }
    return nx_tls_service(context);
}

static nx_result_t process_until(nx_mqtt_t* mqtt, const bool* event) {
    nx_mqtt_call_t call = {deadline(), 64U};
    while (!*event && nx_time_now_us() < call.deadline) {
        nx_result_t result = nx_mqtt_process(mqtt, call);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            return result;
        }
    }
    return *event ? NX_SUCCESS : NX_ERROR_TIMEOUT;
}

static nx_result_t exchange(nx_mqtt_t* mqtt, events_t* events) {
    static const char topic[] = "link/probe";
    static const uint8_t payload[] = "bounded-service-link";
    const nx_mqtt_connect_t connect = {
        "nexus-arm-link", 14U, NULL, 0U, NULL, 0U, 30U};
    nx_result_t result =
        nx_mqtt_connect(mqtt, &connect, (nx_mqtt_call_t){deadline(), 64U});
    uint16_t packet_id = 0U;
    if (result == NX_SUCCESS) {
        result =
            nx_mqtt_subscribe(mqtt, topic, sizeof(topic) - 1U, 1U, &packet_id,
                              (nx_mqtt_call_t){deadline(), 64U});
    }
    if (result == NX_SUCCESS) {
        result = process_until(mqtt, &events->subscribed);
    }
    if (result == NX_SUCCESS) {
        const nx_mqtt_publish_t publish = {topic,   sizeof(topic) - 1U,
                                           payload, sizeof(payload) - 1U,
                                           1U,      false};
        result = nx_mqtt_publish(mqtt, &publish, &packet_id,
                                 (nx_mqtt_call_t){deadline(), 64U});
    }
    if (result == NX_SUCCESS) {
        result = process_until(mqtt, &events->published);
    }
    if (result == NX_SUCCESS) {
        result =
            nx_mqtt_unsubscribe(mqtt, topic, sizeof(topic) - 1U, &packet_id,
                                (nx_mqtt_call_t){deadline(), 64U});
    }
    if (result == NX_SUCCESS) {
        result = process_until(mqtt, &events->unsubscribed);
    }
    if (result == NX_SUCCESS) {
        result = nx_mqtt_disconnect(mqtt, (nx_mqtt_call_t){deadline(), 64U});
    }
    return result;
}

nx_result_t security_link_run(
    nx_tls_runtime_t* runtime, const nx_tls_runtime_config_t* runtime_config,
    nx_tls_session_t* tls, const nx_tls_config_t* tls_config, nx_mqtt_t* mqtt,
    const nx_mqtt_memory_t* mqtt_memory, bool* joined) {
    *joined = false;
    nx_result_t result =
        nx_tls_runtime_start(runtime, runtime_config, deadline());
    if (result != NX_SUCCESS) {
        *joined = true;
        return result;
    }
    result = nx_tls_initialize(tls, tls_config, deadline());
    if (result == NX_SUCCESS) {
        nx_time_us_t limit = deadline();
        do {
            result = nx_tls_handshake_step(tls, limit);
        } while (result == NX_ERROR_BUSY && nx_time_now_us() < limit);
    }
    bool mqtt_open = false;
    if (result == NX_SUCCESS) {
        const nx_mqtt_config_t config = {*nx_tls_stream(tls), on_event,
                                         &boot_events, idle, tls};
        result = nx_mqtt_init(mqtt, &config, mqtt_memory);
        mqtt_open = result == NX_SUCCESS;
        if (mqtt_open) {
            result = exchange(mqtt, &boot_events);
        }
        if (mqtt_open) {
            nx_result_t cleanup = nx_mqtt_deinit(mqtt);
            if (cleanup != NX_SUCCESS) {
                return cleanup;
            }
        }
    }
    if (result == NX_SUCCESS) {
        nx_time_us_t limit = deadline();
        nx_result_t cleanup;
        do {
            cleanup = nx_tls_service(tls);
        } while (cleanup == NX_ERROR_BUSY && nx_time_now_us() < limit);
        if (cleanup == NX_SUCCESS) {
            do {
                cleanup = nx_tls_close_step(tls, limit);
            } while (cleanup == NX_ERROR_BUSY && nx_time_now_us() < limit);
        }
        if (cleanup != NX_SUCCESS) {
            result = cleanup;
        }
    }
    nx_result_t cleanup = nx_tls_abort(tls);
    if (cleanup != NX_SUCCESS) {
        return cleanup;
    }
    cleanup = nx_tls_runtime_stop(runtime);
    if (cleanup != NX_SUCCESS) {
        return cleanup;
    }
    *joined = true;
    return result;
}
