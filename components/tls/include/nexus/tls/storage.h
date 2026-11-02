/**
 * \file            storage.h
 * \brief           Exact pinned TLS/PSA backend storage, composition only
 * \author          Nexus Team
 */
#ifndef NEXUS_TLS_STORAGE_H
#define NEXUS_TLS_STORAGE_H
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "nexus/tls/tls.h"
struct nx_tls_runtime {
    nx_tls_runtime_config_t config;
    nx_time_us_t entropy_deadline;
    nx_time_us_t operation_deadline;
    int64_t utc_snapshot;
    bool initialized;
    bool session_active;
    bool callback_active;
    bool entropy_failed;
    bool calendar_active;
    bool calendar_failed;
};
struct nx_tls_session {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config ssl_config;
    mbedtls_x509_crt trust;
    mbedtls_x509_crt identity;
    mbedtls_pk_context private_key;
    struct {
        nx_tls_runtime_t* runtime;
        const nx_byte_stream_t* transport;
        uint8_t* tx_stage;
        size_t tx_stage_bytes;
    } config;
    nx_byte_stream_t stream;
    nx_tls_diagnostic_t diagnostic;
    nx_time_us_t handshake_deadline;
    nx_time_us_t write_deadline;
    nx_time_us_t close_deadline;
    nx_time_us_t operation_deadline;
    size_t stage_bytes;
    size_t stage_offset;
    bool initialized;
    bool bio_active;
    bool local_closed;
};
#endif
