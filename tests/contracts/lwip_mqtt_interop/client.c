/**
 * \file            client.c
 * \brief           Real client lwIP/TCP/TLS/MQTT chain to a Mosquitto peer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "fixture.h"
#include "nexus/mqtt/core_mqtt_storage.h"
#include "nexus/tls/storage.h"
#include <errno.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>

typedef struct {
    size_t reads;
    size_t writes;
    nx_tls_session_t* session;
    unsigned seen;
    uint16_t published;
    bool bad_event;
} host_t;

enum { GOT_SUBACK = 1u, GOT_PUBACK = 2u, GOT_PUBLISH = 4u, GOT_UNSUBACK = 8u };
static const char topic[] = "nexus/host/e2e";
static const uint8_t payload[] = "bounded TLS MQTT echo";

nx_time_us_t nx_mqtt_port_now_us(void) {
    return nx_chain_now_us();
}
void nx_mqtt_port_assert(const char* file, unsigned line) {
    fprintf(stderr, "unexpected library assertion %s:%u\n", file, line);
    abort();
}
static nx_time_us_t clock_now(void* context) {
    (void)context;
    return nx_mqtt_port_now_us();
}
static nx_result_t utc_now(void* context, int64_t* seconds) {
    (void)context;
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return NX_ERROR_IO;
    }
    *seconds = (int64_t)now;
    return NX_SUCCESS;
}
static nx_result_t entropy(void* context, uint32_t* word) {
    (void)context;
    uint32_t value = 0u;
    ssize_t n = getrandom(&value, sizeof(value), GRND_NONBLOCK);
    if (n == (ssize_t)sizeof(value)) {
        *word = value;
        return NX_SUCCESS;
    }
    return n < 0 && (errno == EAGAIN || errno == EINTR) ? NX_ERROR_BUSY
                                                        : NX_ERROR_IO;
}
static nx_result_t wait_tick(host_t* host, nx_time_us_t deadline) {
    (void)host;
    return nx_chain_wait(deadline);
}
static nx_result_t idle(void* context, nx_time_us_t deadline) {
    host_t* host = context;
    nx_result_t result = nx_tls_service(host->session);
    if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
        return result;
    }
    return wait_tick(host, deadline);
}
static void event(void* context, const nx_mqtt_event_t* e) {
    host_t* host = context;
    printf("event=%u packet=%u qos=%u status=%d\n", (unsigned)e->kind,
           (unsigned)e->packet_id, (unsigned)e->qos, (int)e->status);
    if (e->status != NX_SUCCESS) {
        host->bad_event = true;
    }
    switch (e->kind) {
        case NX_MQTT_EVENT_SUBACK:
            host->seen |= GOT_SUBACK;
            host->bad_event |= e->qos != 1u;
            break;
        case NX_MQTT_EVENT_PUBACK:
            host->seen |= GOT_PUBACK;
            host->bad_event |= e->packet_id != host->published;
            break;
        case NX_MQTT_EVENT_PUBLISH:
            host->seen |= GOT_PUBLISH;
            host->bad_event |=
                e->qos != 1u || e->topic_size != sizeof(topic) - 1u ||
                e->payload_size != sizeof(payload) - 1u ||
                memcmp(e->topic, topic, sizeof(topic) - 1u) != 0 ||
                memcmp(e->payload, payload, sizeof(payload) - 1u) != 0;
            break;
        case NX_MQTT_EVENT_UNSUBACK:
            host->seen |= GOT_UNSUBACK;
            break;
    }
}
static bool load(const char* directory, const char* filename, uint8_t* bytes,
                 size_t capacity, size_t* size) {
    char path[512];
    int n = snprintf(path, sizeof(path), "%s/%s", directory, filename);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return false;
    }
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    *size = fread(bytes, 1u, capacity, file);
    bool valid = !ferror(file) && *size != 0u && fgetc(file) == EOF;
    return fclose(file) == 0 && valid;
}
static nx_mqtt_call_t call(nx_time_us_t deadline) {
    nx_mqtt_call_t value = {deadline, 10000u};
    return value;
}
static nx_result_t drive(host_t* host, nx_mqtt_t* mqtt, unsigned wanted,
                         nx_time_us_t deadline) {
    while ((host->seen & wanted) != wanted && !host->bad_event) {
        nx_result_t result = idle(host, deadline);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            return result;
        }
        result = nx_mqtt_process(mqtt, call(deadline));
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            return result;
        }
    }
    return host->bad_event ? NX_ERROR_STATE : NX_SUCCESS;
}
#define CHECK(expression)                                                      \
    do {                                                                       \
        if (!(expression)) {                                                   \
            fprintf(stderr, "failed line=%u expression=%s\n",                  \
                    (unsigned)__LINE__, #expression);                          \
            goto cleanup;                                                      \
        }                                                                      \
    } while (0)

int nx_chain_client(int argc, char** argv) {
    if (argc != 4) {
        return 2;
    }
    char* end = NULL;
    unsigned long port_value = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || port_value == 0u ||
        port_value > UINT16_MAX) {
        return 2;
    }
    bool fault = strcmp(argv[2], "fault") == 0;
    bool blackhole = strcmp(argv[2], "deadline") == 0;
    bool partial = strcmp(argv[2], "backpressure") == 0;
    bool missing = strcmp(argv[2], "missing-client") == 0;
    if (!fault && !blackhole && !partial && !missing &&
        strcmp(argv[2], "clean") != 0) {
        return 2;
    }
    int status = 1;
    alignas(max_align_t) static uint8_t arena[NX_TLS_ARENA_BYTES];
    uint8_t stage[NX_TLS_TX_STAGE_BYTES];
    uint8_t ca[NX_TLS_DER_MAX], identity[NX_TLS_DER_MAX], key[NX_TLS_DER_MAX];
    size_t ca_size = 0u, identity_size = 0u, key_size = 0u;
    nx_tls_runtime_t runtime = {0};
    nx_tls_session_t tls = {0};
    nx_mqtt_t mqtt = {0};
    host_t host = {.session = &tls};
    const nx_byte_stream_t* transport = NULL;
    const nx_entropy_ops_t entropy_ops = {entropy};
    const nx_entropy_port_t entropy_port = {&entropy_ops, &host};
    nx_tls_runtime_config_t rt = {arena,
                                  sizeof(arena),
                                  &entropy_port,
                                  {clock_now, NULL},
                                  {utc_now, NULL},
                                  100000u,
                                  NX_TLS_ENTROPY_FULL_SOURCE};
    nx_time_us_t deadline = nx_mqtt_port_now_us() + UINT64_C(5000000);
    CHECK(load(argv[3], "ca.der", ca, sizeof(ca), &ca_size));
    CHECK(load(argv[3], "client.der", identity, sizeof(identity),
               &identity_size));
    CHECK(load(argv[3], "client.key.der", key, sizeof(key), &key_size));
    CHECK(
        nx_chain_connect((unsigned)port_value, argv[2], &transport, deadline));
    CHECK(nx_tls_runtime_start(&runtime, &rt, deadline) == NX_SUCCESS);
    nx_tls_config_t config = {&runtime,
                              transport,
                              ca,
                              ca_size,
                              missing ? NULL : identity,
                              missing ? 0u : identity_size,
                              missing ? NULL : key,
                              missing ? 0u : key_size,
                              "server.test",
                              11u,
                              stage,
                              sizeof(stage)};
    CHECK(nx_tls_initialize(&tls, &config, deadline) == NX_SUCCESS);
    nx_result_t result = NX_ERROR_BUSY;
    while (result == NX_ERROR_BUSY) {
        result = nx_tls_handshake_step(&tls, deadline);
        if (result == NX_ERROR_BUSY) {
            CHECK(wait_tick(&host, deadline) == NX_SUCCESS);
        }
    }
    if (missing) {
        nx_tls_diagnostic_t diagnostic = {0};
        CHECK(nx_tls_diagnostic(&tls, &diagnostic) == NX_SUCCESS);
        CHECK(diagnostic.state == NX_TLS_FAILED);
        CHECK(diagnostic.library_error == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE);
        printf("missing_client_peer_fatal_alert=%d library=%d\n", (int)result,
               diagnostic.library_error);
        status = 0;
        goto cleanup;
    }
    CHECK(result == NX_SUCCESS);
    printf("authenticated_tls=%s cipher=%s\n",
           mbedtls_ssl_get_version(&tls.ssl),
           mbedtls_ssl_get_ciphersuite(&tls.ssl));
    CHECK(strcmp(mbedtls_ssl_get_version(&tls.ssl), "TLSv1.2") == 0);
    uint8_t network[512];
    MQTTPubAckInfo_t outgoing[2], incoming[2];
    nx_mqtt_control_t controls[2];
    const nx_mqtt_memory_t memory = {
        network, sizeof(network), outgoing, 2u, incoming, 2u, controls, 2u};
    const nx_mqtt_config_t mqtt_config = {*nx_tls_stream(&tls), event, &host,
                                          idle, &host};
    CHECK(nx_mqtt_init(&mqtt, &mqtt_config, &memory) == NX_SUCCESS);
    const nx_mqtt_connect_t connection = {"nexus-e2e", 9u, NULL, 0u,
                                          NULL,        0u, 30u};
    deadline = nx_mqtt_port_now_us() + UINT64_C(5000000);
    CHECK(nx_mqtt_connect(&mqtt, &connection, call(deadline)) == NX_SUCCESS);
    uint16_t id = 0u;
    CHECK(nx_mqtt_subscribe(&mqtt, topic, sizeof(topic) - 1u, 1u, &id,
                            call(deadline)) == NX_SUCCESS);
    CHECK(drive(&host, &mqtt, GOT_SUBACK, deadline) == NX_SUCCESS);
    if (blackhole) {
        CHECK(nx_chain_fault(true));
        deadline = nx_mqtt_port_now_us() + 150000u;
    }
    const nx_mqtt_publish_t publish = {
        topic, sizeof(topic) - 1u, payload, sizeof(payload) - 1u, 1u, false};
    CHECK(nx_mqtt_publish(&mqtt, &publish, &host.published, call(deadline)) ==
          NX_SUCCESS);
    if (fault || blackhole) {
        do {
            result = nx_tls_service(&tls);
            if (result == NX_ERROR_BUSY) {
                CHECK(wait_tick(&host, deadline) == NX_SUCCESS);
            }
        } while (result == NX_ERROR_BUSY);
        CHECK(result == NX_SUCCESS);
        if (fault) {
            CHECK(nx_chain_fault(false));
        }
        do {
            result = nx_mqtt_process(&mqtt, call(deadline));
        } while ((result == NX_SUCCESS || result == NX_ERROR_BUSY) &&
                 nx_mqtt_port_now_us() < deadline);
        CHECK(result == (blackhole ? NX_ERROR_TIMEOUT : NX_ERROR_IO));
        CHECK(nx_mqtt_fault(&mqtt) == result);
        if (blackhole) {
            CHECK(nx_mqtt_port_now_us() >= deadline);
        }
        nx_chain_counters(&host.reads, &host.writes);
        size_t reads = host.reads, writes = host.writes;
        CHECK(nx_mqtt_publish(&mqtt, &publish, &id, call(deadline)) ==
              NX_ERROR_STATE);
        CHECK(nx_mqtt_process(&mqtt, call(deadline)) == NX_ERROR_STATE);
        nx_chain_counters(&host.reads, &host.writes);
        CHECK(host.reads == reads && host.writes == writes);
        printf("transport_fault=%d deadline=%u replay_provider_calls=0\n",
               (int)result, blackhole ? 1u : 0u);
        status = 0;
        goto cleanup;
    }
    CHECK(drive(&host, &mqtt, GOT_PUBACK | GOT_PUBLISH, deadline) ==
          NX_SUCCESS);
    CHECK(nx_mqtt_unsubscribe(&mqtt, topic, sizeof(topic) - 1u, &id,
                              call(deadline)) == NX_SUCCESS);
    CHECK(drive(&host, &mqtt, GOT_UNSUBACK, deadline) == NX_SUCCESS);
    CHECK(nx_mqtt_disconnect(&mqtt, call(deadline)) == NX_SUCCESS);
    do {
        result = nx_tls_service(&tls);
        if (result == NX_ERROR_BUSY) {
            CHECK(wait_tick(&host, deadline) == NX_SUCCESS);
        }
    } while (result == NX_ERROR_BUSY);
    CHECK(result == NX_SUCCESS);
    do {
        result = nx_tls_close_step(&tls, deadline);
        if (result == NX_ERROR_BUSY) {
            CHECK(wait_tick(&host, deadline) == NX_SUCCESS);
        }
    } while (result == NX_ERROR_BUSY);
    CHECK(result == NX_SUCCESS);
    uint8_t final_byte = 0u;
    size_t final_count = 0u;
    do {
        result = nx_tls_try_read(&tls, &final_byte, 1u, &final_count, deadline);
        if (result == NX_ERROR_BUSY) {
            CHECK(wait_tick(&host, deadline) == NX_SUCCESS);
        }
    } while (result == NX_ERROR_BUSY);
    CHECK(result == NX_SUCCESS && final_count == 0u);
    CHECK(nx_chain_finish(deadline));
    printf("mqtt_clean_close=locally_accepted mqtt_events=%u\n", host.seen);
    status = 0;
cleanup:
    if (nx_mqtt_deinit(&mqtt) != NX_SUCCESS ||
        nx_tls_abort(&tls) != NX_SUCCESS) {
        status = 1;
    }
    if (!nx_chain_stop()) {
        status = 1;
    }
    if (nx_tls_runtime_stop(&runtime) != NX_SUCCESS) {
        status = 1;
    }
    nx_chain_counters(&host.reads, &host.writes);
    printf("result=%d rx_frames=%zu provider_service_calls=%zu\n", status,
           host.reads, host.writes);
    return status;
}
