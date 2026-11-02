/**
 * \file            tls.c
 * \brief           One explicit finite TLS client/PSA ownership domain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/platform_util.h"
#include "nexus/core/fault.h"
#include "nexus/tls/storage.h"
#include "psa/crypto.h"
#include "psa/crypto_extra.h"
#include <limits.h>
#include <string.h>

/* One selected synchronous crypto profile owns PSA and its allocator. */
static nx_tls_runtime_t* active_runtime;

static void wipe(void* memory, size_t bytes) {
    volatile uint8_t* output = memory;
    for (size_t i = 0U; i < bytes; ++i) {
        output[i] = 0U;
    }
}

static bool overlaps(const void* first, size_t first_size, const void* second,
                     size_t second_size) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    if (first_size > UINTPTR_MAX - a || second_size > UINTPTR_MAX - b) {
        return true;
    }
    return first_size != 0U && second_size != 0U && a < b + second_size &&
           b < a + first_size;
}

static _Noreturn void invariant_failure(int status) {
    (void)status;
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_STATE, __FILE__, __LINE__);
    for (;;) {
    }
}

mbedtls_ms_time_t mbedtls_ms_time(void) {
    nx_tls_runtime_t* runtime = active_runtime;
    if (runtime == NULL || !runtime->callback_active) {
        return 0;
    }
    nx_time_us_t now =
        runtime->config.clock.read(runtime->config.clock.context);
    return (mbedtls_ms_time_t)(now / UINT64_C(1000));
}

struct tm* mbedtls_platform_gmtime_r(const mbedtls_time_t* seconds,
                                     struct tm* output) {
    if (seconds == NULL || output == NULL) {
        return NULL;
    }
    /* Selected newlib gmtime_r uses caller storage and has no heap dependency.
     */
    struct tm* result = gmtime_r(seconds, output);
    if (result == NULL && active_runtime != NULL &&
        active_runtime->callback_active) {
        active_runtime->calendar_failed = true;
    }
    return result;
}

static bool trusted_utc(nx_tls_runtime_t* runtime, int64_t* seconds) {
    if (runtime->config.utc.read(runtime->config.utc.context, seconds) !=
            NX_SUCCESS ||
        *seconds < 0 || *seconds > INT64_C(253402300799) ||
        (int64_t)(mbedtls_time_t)*seconds != *seconds) {
        return false;
    }
    mbedtls_time_t value = (mbedtls_time_t)*seconds;
    struct tm calendar;
    return mbedtls_platform_gmtime_r(&value, &calendar) != NULL &&
           calendar.tm_year >= 70 && calendar.tm_year <= 8099;
}

static nx_result_t begin_crypto(nx_tls_runtime_t* runtime,
                                nx_time_us_t deadline, bool calendar) {
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (runtime->entropy_failed || runtime->callback_active) {
        return NX_ERROR_STATE;
    }
    /* Every user callback runs inside the join guard, including clocks. */
    runtime->callback_active = true;
    nx_time_us_t now =
        runtime->config.clock.read(runtime->config.clock.context);
    if (now >= deadline) {
        runtime->callback_active = false;
        return NX_ERROR_TIMEOUT;
    }
    if (calendar) {
        int64_t seconds;
        if (!trusted_utc(runtime, &seconds)) {
            runtime->callback_active = false;
            return NX_ERROR_PERMISSION;
        }
        runtime->utc_snapshot = seconds;
    }
    nx_time_us_t budget = runtime->config.entropy_timeout_us;
    runtime->entropy_deadline =
        budget < deadline - now ? now + budget : deadline;
    runtime->operation_deadline = deadline;
    runtime->calendar_failed = false;
    runtime->calendar_active = calendar;
    runtime->callback_active = true;
    return NX_SUCCESS;
}

static nx_result_t end_crypto(nx_tls_runtime_t* runtime) {
    if (runtime->calendar_active) {
        int64_t seconds;
        if (!trusted_utc(runtime, &seconds)) {
            runtime->calendar_failed = true;
        }
    }
    nx_time_us_t now =
        runtime->config.clock.read(runtime->config.clock.context);
    runtime->callback_active = false;
    runtime->calendar_active = false;
    if (runtime->entropy_failed) {
        return NX_ERROR_IO;
    }
    if (runtime->calendar_failed) {
        return NX_ERROR_PERMISSION;
    }
    return now >= runtime->operation_deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

static mbedtls_time_t trusted_time(mbedtls_time_t* output) {
    nx_tls_runtime_t* runtime = active_runtime;
    mbedtls_time_t result = 0;
    if (runtime != NULL && runtime->callback_active &&
        runtime->calendar_active) {
        int64_t current;
        if (!trusted_utc(runtime, &current)) {
            runtime->calendar_failed = true;
        }
        result = (mbedtls_time_t)runtime->utc_snapshot;
    } else if (runtime != NULL) {
        runtime->calendar_failed = true;
    }
    if (output != NULL) {
        *output = result;
    }
    return result;
}

int mbedtls_platform_get_entropy(psa_driver_get_entropy_flags_t flags,
                                 size_t* estimate_bits, unsigned char* output,
                                 size_t output_size) {
    if (estimate_bits == NULL) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    *estimate_bits = 0U;
    if ((output == NULL && output_size != 0U) || output_size > SIZE_MAX / 8U ||
        output_size > UINTPTR_MAX - (uintptr_t)output) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    if (flags != PSA_DRIVER_GET_ENTROPY_FLAGS_NONE) {
        wipe(output, output_size);
        return PSA_ERROR_NOT_SUPPORTED;
    }
    nx_tls_runtime_t* runtime = active_runtime;
    if (runtime == NULL || !runtime->callback_active ||
        runtime->entropy_failed) {
        wipe(output, output_size);
        return PSA_ERROR_BAD_STATE;
    }
    nx_result_t result =
        nx_entropy_fill(runtime->config.entropy, output, output_size,
                        &runtime->config.clock, runtime->entropy_deadline);
    if (result != NX_SUCCESS) {
        runtime->entropy_failed = true;
        return PSA_ERROR_INSUFFICIENT_ENTROPY;
    }
    *estimate_bits = output_size * 8U;
    return PSA_SUCCESS;
}

nx_result_t nx_tls_runtime_start(nx_tls_runtime_t* runtime,
                                 const nx_tls_runtime_config_t* config,
                                 nx_time_us_t deadline) {
    if (runtime == NULL || config == NULL || config->arena == NULL ||
        config->arena_bytes != NX_TLS_ARENA_BYTES ||
        (uintptr_t)config->arena % _Alignof(max_align_t) != 0U ||
        config->entropy == NULL || config->entropy->ops == NULL ||
        config->entropy->context == NULL ||
        config->entropy->ops->try_read == NULL || config->clock.read == NULL ||
        config->utc.read == NULL || config->entropy_timeout_us == 0U ||
        config->entropy_timeout_us == NX_DEADLINE_NEVER ||
        deadline == NX_DEADLINE_NEVER ||
        overlaps(runtime, sizeof(*runtime), config, sizeof(*config)) ||
        overlaps(runtime, sizeof(*runtime), config->arena,
                 config->arena_bytes)) {
        return NX_ERROR_INVALID;
    }
    const struct {
        const void* pointer;
        size_t bytes;
    } inputs[] = {{config, sizeof(*config)},
                  {config->entropy, sizeof(*config->entropy)},
                  {config->entropy->ops, sizeof(*config->entropy->ops)}};
    for (size_t i = 0U; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        if (overlaps(inputs[i].pointer, inputs[i].bytes, runtime,
                     sizeof(*runtime)) ||
            overlaps(inputs[i].pointer, inputs[i].bytes, config->arena,
                     config->arena_bytes)) {
            return NX_ERROR_INVALID;
        }
    }
    if (config->entropy_quality != NX_TLS_ENTROPY_FULL_SOURCE) {
        return NX_ERROR_PERMISSION;
    }
    if (active_runtime != NULL) {
        return NX_ERROR_BUSY;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->config = *config;
    active_runtime = runtime;
    nx_result_t result = begin_crypto(runtime, deadline, false);
    if (result != NX_SUCCESS) {
        active_runtime = NULL;
        return result;
    }
    (void)mbedtls_platform_set_exit(invariant_failure);
    mbedtls_memory_buffer_alloc_init(config->arena, config->arena_bytes);
    (void)mbedtls_platform_set_time(trusted_time);
    psa_status_t status = psa_crypto_init();
    nx_result_t callbacks = end_crypto(runtime);
    if (status != PSA_SUCCESS || callbacks != NX_SUCCESS) {
        mbedtls_psa_crypto_free();
        mbedtls_memory_buffer_alloc_free();
        wipe(config->arena, config->arena_bytes);
        active_runtime = NULL;
        return callbacks != NX_SUCCESS ? callbacks : NX_ERROR_IO;
    }
    runtime->initialized = true;
    return NX_SUCCESS;
}

nx_result_t nx_tls_runtime_stop(nx_tls_runtime_t* runtime) {
    if (runtime == NULL || !runtime->initialized || active_runtime != runtime) {
        return NX_ERROR_STATE;
    }
    if (runtime->session_active || runtime->callback_active) {
        return NX_ERROR_BUSY;
    }
    mbedtls_psa_crypto_free();
    mbedtls_memory_buffer_alloc_free();
    wipe(runtime->config.arena, runtime->config.arena_bytes);
    runtime->initialized = false;
    active_runtime = NULL;
    return NX_SUCCESS;
}

nx_result_t nx_tls_abort(nx_tls_session_t* session) {
    if (session == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!session->initialized) {
        return NX_SUCCESS;
    }
    if (session->bio_active || session->config.runtime->callback_active) {
        return NX_ERROR_BUSY;
    }
    mbedtls_ssl_free(&session->ssl);
    mbedtls_ssl_config_free(&session->ssl_config);
    mbedtls_pk_free(&session->private_key);
    mbedtls_x509_crt_free(&session->identity);
    mbedtls_x509_crt_free(&session->trust);
    wipe(session->config.tx_stage, session->config.tx_stage_bytes);
    session->config.runtime->session_active = false;
    session->initialized = false;
    session->stage_bytes = 0U;
    session->stage_offset = 0U;
    session->diagnostic.pending_plaintext = 0U;
    session->diagnostic.state = NX_TLS_CLOSED;
    return NX_SUCCESS;
}

static nx_result_t fail(nx_tls_session_t* session, nx_result_t result,
                        int library_error) {
    session->diagnostic.state = NX_TLS_FAILED;
    session->diagnostic.result = result;
    session->diagnostic.library_error = library_error;
    session->diagnostic.want_read = false;
    session->diagnostic.want_write = false;
    return result;
}

static bool live(const nx_tls_session_t* session) {
    return session != NULL && session->initialized &&
           session->config.runtime == active_runtime &&
           active_runtime != NULL && active_runtime->initialized;
}

static nx_time_us_t preflight_clock(nx_tls_runtime_t* runtime) {
    runtime->callback_active = true;
    nx_time_us_t now =
        runtime->config.clock.read(runtime->config.clock.context);
    runtime->callback_active = false;
    return now;
}

static bool outside_storage(const nx_tls_session_t* session,
                            const void* pointer, size_t bytes) {
    const nx_tls_runtime_t* runtime = session->config.runtime;
    return !overlaps(pointer, bytes, session, sizeof(*session)) &&
           !overlaps(pointer, bytes, runtime, sizeof(*runtime)) &&
           !overlaps(pointer, bytes, runtime->config.arena,
                     runtime->config.arena_bytes) &&
           !overlaps(pointer, bytes, session->config.tx_stage,
                     session->config.tx_stage_bytes) &&
           !overlaps(pointer, bytes, session->config.transport,
                     sizeof(*session->config.transport)) &&
           !overlaps(pointer, bytes, session->config.transport->ops,
                     sizeof(*session->config.transport->ops)) &&
           !overlaps(pointer, bytes, runtime->config.entropy,
                     sizeof(*runtime->config.entropy)) &&
           !overlaps(pointer, bytes, runtime->config.entropy->ops,
                     sizeof(*runtime->config.entropy->ops));
}

static int send_bio(void* context, const unsigned char* data, size_t bytes) {
    nx_tls_session_t* session = context;
    if (!session->bio_active || !session->config.runtime->callback_active ||
        bytes > INT_MAX) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    size_t accepted = 0U;
    const nx_byte_stream_t* port = session->config.transport;
    nx_result_t result = port->ops->write(port->context, data, bytes, &accepted,
                                          session->operation_deadline);
    if (result == NX_ERROR_BUSY && accepted == 0U) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    if (result != NX_SUCCESS || accepted > bytes ||
        (accepted == 0U && bytes != 0U)) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    return (int)accepted;
}

static int receive_bio(void* context, unsigned char* data, size_t bytes) {
    nx_tls_session_t* session = context;
    if (!session->bio_active || !session->config.runtime->callback_active ||
        bytes > INT_MAX) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    size_t received = 0U;
    const nx_byte_stream_t* port = session->config.transport;
    nx_result_t result = port->ops->read(port->context, data, bytes, &received,
                                         session->operation_deadline);
    if (result == NX_ERROR_BUSY && received == 0U) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    if (result != NX_SUCCESS || received > bytes) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    return (int)received;
}

static nx_result_t stream_write(void* context, const uint8_t* data,
                                size_t bytes, size_t* accepted,
                                nx_time_us_t deadline) {
    return nx_tls_try_write(context, data, bytes, accepted, deadline);
}
static nx_result_t stream_read(void* context, uint8_t* data, size_t bytes,
                               size_t* received, nx_time_us_t deadline) {
    return nx_tls_try_read(context, data, bytes, received, deadline);
}
static const nx_byte_stream_ops_t stream_ops = {stream_write, stream_read};

static bool valid_session_config(nx_tls_session_t* session,
                                 const nx_tls_config_t* config) {
    if (session == NULL || config == NULL || config->runtime == NULL ||
        config->transport == NULL || config->transport->ops == NULL ||
        config->transport->context == NULL ||
        config->transport->ops->read == NULL ||
        config->transport->ops->write == NULL || config->trust_der == NULL ||
        config->trust_bytes == 0U || config->trust_bytes > NX_TLS_DER_MAX ||
        config->tx_stage == NULL ||
        config->tx_stage_bytes != NX_TLS_TX_STAGE_BYTES ||
        config->hostname == NULL || config->hostname_length == 0U ||
        config->hostname_length > 253U ||
        config->hostname[config->hostname_length] != '\0' ||
        memchr(config->hostname, '\0', config->hostname_length) != NULL ||
        config->client_bytes > NX_TLS_DER_MAX || config->key_bytes > 2048U ||
        ((config->client_der == NULL) != (config->client_bytes == 0U)) ||
        ((config->key_der == NULL) != (config->key_bytes == 0U)) ||
        ((config->client_bytes == 0U) != (config->key_bytes == 0U))) {
        return false;
    }
    const nx_tls_runtime_t* runtime = config->runtime;
    if (overlaps(session, sizeof(*session), config, sizeof(*config)) ||
        overlaps(session, sizeof(*session), runtime, sizeof(*runtime)) ||
        overlaps(session, sizeof(*session), runtime->config.arena,
                 runtime->config.arena_bytes) ||
        overlaps(config->tx_stage, config->tx_stage_bytes, session,
                 sizeof(*session)) ||
        overlaps(config->tx_stage, config->tx_stage_bytes, runtime,
                 sizeof(*runtime)) ||
        overlaps(config->tx_stage, config->tx_stage_bytes,
                 runtime->config.arena, runtime->config.arena_bytes) ||
        overlaps(config->transport, sizeof(*config->transport), session,
                 sizeof(*session))) {
        return false;
    }
    const struct {
        const void* pointer;
        size_t bytes;
    } inputs[] = {{config, sizeof(*config)},
                  {config->transport, sizeof(*config->transport)},
                  {config->transport->ops, sizeof(*config->transport->ops)},
                  {config->trust_der, config->trust_bytes},
                  {config->client_der, config->client_bytes},
                  {config->key_der, config->key_bytes},
                  {config->hostname, config->hostname_length + 1U}};
    for (size_t i = 0U; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        if (overlaps(inputs[i].pointer, inputs[i].bytes, session,
                     sizeof(*session)) ||
            overlaps(inputs[i].pointer, inputs[i].bytes, runtime,
                     sizeof(*runtime)) ||
            overlaps(inputs[i].pointer, inputs[i].bytes, runtime->config.arena,
                     runtime->config.arena_bytes) ||
            overlaps(inputs[i].pointer, inputs[i].bytes, config->tx_stage,
                     config->tx_stage_bytes)) {
            return false;
        }
    }
    return true;
}

nx_result_t nx_tls_initialize(nx_tls_session_t* session,
                              const nx_tls_config_t* config,
                              nx_time_us_t deadline) {
    if (!valid_session_config(session, config) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    nx_tls_runtime_t* runtime = config->runtime;
    if (!runtime->initialized || runtime != active_runtime) {
        return NX_ERROR_STATE;
    }
    if (runtime->session_active || session->initialized ||
        runtime->callback_active) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = begin_crypto(runtime, deadline, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    memset(session, 0, sizeof(*session));
    session->config.runtime = config->runtime;
    session->config.transport = config->transport;
    session->config.tx_stage = config->tx_stage;
    session->config.tx_stage_bytes = config->tx_stage_bytes;
    session->initialized = true;
    runtime->session_active = true;
    mbedtls_ssl_init(&session->ssl);
    mbedtls_ssl_config_init(&session->ssl_config);
    mbedtls_x509_crt_init(&session->trust);
    mbedtls_x509_crt_init(&session->identity);
    mbedtls_pk_init(&session->private_key);
    int code = mbedtls_x509_crt_parse_der(&session->trust, config->trust_der,
                                          config->trust_bytes);
    if (code == 0 && config->client_bytes != 0U) {
        code = mbedtls_x509_crt_parse_der(
            &session->identity, config->client_der, config->client_bytes);
        if (code == 0) {
            code = mbedtls_pk_parse_key(&session->private_key, config->key_der,
                                        config->key_bytes, NULL, 0U);
        }
    }
    if (code == 0) {
        code = mbedtls_ssl_config_defaults(
            &session->ssl_config, MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    }
    if (code == 0) {
        mbedtls_ssl_conf_authmode(&session->ssl_config,
                                  MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&session->ssl_config, &session->trust, NULL);
        code = mbedtls_ssl_conf_max_frag_len(&session->ssl_config,
                                             MBEDTLS_SSL_MAX_FRAG_LEN_1024);
    }
    if (code == 0 && config->client_bytes != 0U) {
        code = mbedtls_ssl_conf_own_cert(
            &session->ssl_config, &session->identity, &session->private_key);
    }
    if (code == 0) {
        code = mbedtls_ssl_setup(&session->ssl, &session->ssl_config);
    }
    if (code == 0) {
        code = mbedtls_ssl_set_hostname(&session->ssl, config->hostname);
    }
    result = end_crypto(runtime);
    if (code != 0 || result != NX_SUCCESS) {
        nx_tls_abort(session);
        return result != NX_SUCCESS ? result : NX_ERROR_IO;
    }
    mbedtls_ssl_set_bio(&session->ssl, session, send_bio, receive_bio, NULL);
    session->stream.ops = &stream_ops;
    session->stream.context = session;
    session->diagnostic.state = NX_TLS_INITIALIZED;
    return NX_SUCCESS;
}

static nx_result_t begin_session(nx_tls_session_t* session,
                                 nx_time_us_t deadline, bool calendar) {
    nx_result_t result =
        begin_crypto(session->config.runtime, deadline, calendar);
    if (result == NX_SUCCESS) {
        session->operation_deadline = deadline;
        session->bio_active = true;
        session->diagnostic.want_read = false;
        session->diagnostic.want_write = false;
    }
    return result;
}

static nx_result_t finish_session(nx_tls_session_t* session, int code) {
    session->bio_active = false;
    nx_result_t result = end_crypto(session->config.runtime);
    if (result != NX_SUCCESS) {
        return fail(session, result, code);
    }
    if (code == MBEDTLS_ERR_SSL_WANT_READ ||
        code == MBEDTLS_ERR_SSL_WANT_WRITE) {
        session->diagnostic.want_read = code == MBEDTLS_ERR_SSL_WANT_READ;
        session->diagnostic.want_write = code == MBEDTLS_ERR_SSL_WANT_WRITE;
        return NX_ERROR_BUSY;
    }
    if (code < 0) {
        uint32_t flags = mbedtls_ssl_get_verify_result(&session->ssl);
        session->diagnostic.verify_flags = flags;
        result = code == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED ||
                         (flags != 0U && flags != UINT32_MAX)
                     ? NX_ERROR_PERMISSION
                     : NX_ERROR_IO;
        return fail(session, result, code);
    }
    return NX_SUCCESS;
}

nx_result_t nx_tls_handshake_step(nx_tls_session_t* session,
                                  nx_time_us_t deadline) {
    if (live(session) && session->config.runtime->callback_active) {
        return NX_ERROR_BUSY;
    }
    if (!live(session)) {
        return NX_ERROR_STATE;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (session->diagnostic.state == NX_TLS_ESTABLISHED) {
        return NX_SUCCESS;
    }
    if (session->diagnostic.state != NX_TLS_INITIALIZED &&
        session->diagnostic.state != NX_TLS_HANDSHAKING) {
        return NX_ERROR_STATE;
    }
    if (session->handshake_deadline == 0U) {
        session->handshake_deadline = deadline;
    }
    deadline = session->handshake_deadline;
    nx_result_t result = begin_session(session, deadline, true);
    if (result != NX_SUCCESS) {
        return fail(session, result, 0);
    }
    int code = 0;
    if (session->diagnostic.state == NX_TLS_INITIALIZED) {
        psa_status_t reseeded = psa_random_reseed(NULL, 0U);
        if (reseeded != PSA_SUCCESS) {
            code = (int)reseeded;
        }
        session->diagnostic.state = NX_TLS_HANDSHAKING;
    }
    if (code == 0) {
        code = mbedtls_ssl_handshake_step(&session->ssl);
    }
    result = finish_session(session, code);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!mbedtls_ssl_is_handshake_over(&session->ssl)) {
        return NX_ERROR_BUSY;
    }
    uint32_t flags = mbedtls_ssl_get_verify_result(&session->ssl);
    session->diagnostic.verify_flags = flags;
    if (flags != 0U) {
        return fail(session, NX_ERROR_PERMISSION, 0);
    }
    session->diagnostic.state = NX_TLS_ESTABLISHED;
    return NX_SUCCESS;
}

static nx_time_us_t stage_deadline(const nx_tls_session_t* session) {
    nx_time_us_t deadline = session->write_deadline;
    if (session->close_deadline != 0U && session->close_deadline < deadline) {
        deadline = session->close_deadline;
    }
    return deadline;
}

nx_result_t nx_tls_service(nx_tls_session_t* session) {
    if (live(session) && session->config.runtime->callback_active) {
        return NX_ERROR_BUSY;
    }
    if (!live(session) || (session->diagnostic.state != NX_TLS_ESTABLISHED &&
                           session->diagnostic.state != NX_TLS_CLOSING)) {
        return NX_ERROR_STATE;
    }
    if (session->stage_bytes == 0U) {
        return NX_SUCCESS;
    }
    nx_time_us_t deadline = stage_deadline(session);
    nx_result_t result = begin_session(session, deadline, false);
    if (result != NX_SUCCESS) {
        return fail(session, result, 0);
    }
    size_t remaining = session->stage_bytes - session->stage_offset;
    int code = mbedtls_ssl_write(
        &session->ssl, session->config.tx_stage + session->stage_offset,
        remaining);
    result = finish_session(session, code);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (code == 0 || (size_t)code > remaining) {
        return fail(session, NX_ERROR_IO, code);
    }
    session->stage_offset += (size_t)code;
    session->diagnostic.pending_plaintext =
        session->stage_bytes - session->stage_offset;
    if (session->stage_offset != session->stage_bytes) {
        return NX_ERROR_BUSY;
    }
    wipe(session->config.tx_stage, session->config.tx_stage_bytes);
    session->stage_bytes = 0U;
    session->stage_offset = 0U;
    return NX_SUCCESS;
}

nx_result_t nx_tls_try_write(nx_tls_session_t* session, const uint8_t* data,
                             size_t bytes, size_t* accepted,
                             nx_time_us_t deadline) {
    if (accepted == NULL ||
        (session != NULL &&
         overlaps(accepted, sizeof(*accepted), session, sizeof(*session))) ||
        (live(session) &&
         (!outside_storage(session, accepted, sizeof(*accepted)) ||
          overlaps(data, bytes, accepted, sizeof(*accepted))))) {
        return NX_ERROR_INVALID;
    }
    *accepted = 0U;
    if (live(session) && session->config.runtime->callback_active) {
        return NX_ERROR_BUSY;
    }
    if (!live(session) || session->diagnostic.state != NX_TLS_ESTABLISHED) {
        return NX_ERROR_STATE;
    }
    if ((data == NULL && bytes != 0U) || deadline == NX_DEADLINE_NEVER ||
        !outside_storage(session, data, bytes) ||
        !outside_storage(session, accepted, sizeof(*accepted)) ||
        overlaps(data, bytes, accepted, sizeof(*accepted))) {
        return NX_ERROR_INVALID;
    }
    if (bytes == 0U) {
        return NX_SUCCESS;
    }
    nx_tls_runtime_t* runtime = session->config.runtime;
    if (preflight_clock(runtime) >= deadline) {
        return NX_ERROR_TIMEOUT;
    }
    if (session->stage_bytes != 0U && deadline < stage_deadline(session)) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_tls_service(session);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (preflight_clock(runtime) >= deadline) {
        return NX_ERROR_TIMEOUT;
    }
    size_t count =
        bytes < NX_TLS_TX_STAGE_BYTES ? bytes : NX_TLS_TX_STAGE_BYTES;
    memcpy(session->config.tx_stage, data, count);
    session->stage_bytes = count;
    session->stage_offset = 0U;
    session->write_deadline = deadline;
    session->diagnostic.pending_plaintext = count;
    *accepted = count;
    return NX_SUCCESS;
}

nx_result_t nx_tls_try_read(nx_tls_session_t* session, uint8_t* data,
                            size_t bytes, size_t* received,
                            nx_time_us_t deadline) {
    if (received == NULL ||
        (session != NULL &&
         overlaps(received, sizeof(*received), session, sizeof(*session))) ||
        (live(session) &&
         (!outside_storage(session, received, sizeof(*received)) ||
          overlaps(data, bytes, received, sizeof(*received))))) {
        return NX_ERROR_INVALID;
    }
    *received = 0U;
    if (live(session) && session->config.runtime->callback_active) {
        return NX_ERROR_BUSY;
    }
    if (!live(session) || (session->diagnostic.state != NX_TLS_ESTABLISHED &&
                           session->diagnostic.state != NX_TLS_CLOSING &&
                           session->diagnostic.state != NX_TLS_CLOSED)) {
        return NX_ERROR_STATE;
    }
    if ((data == NULL && bytes != 0U) || deadline == NX_DEADLINE_NEVER ||
        !outside_storage(session, data, bytes) ||
        !outside_storage(session, received, sizeof(*received)) ||
        overlaps(data, bytes, received, sizeof(*received))) {
        return NX_ERROR_INVALID;
    }
    if (bytes == 0U || session->diagnostic.peer_closed) {
        return NX_SUCCESS;
    }
    nx_tls_runtime_t* runtime = session->config.runtime;
    if (preflight_clock(runtime) >= deadline) {
        return NX_ERROR_TIMEOUT;
    }
    if (session->stage_bytes != 0U) {
        if (deadline < stage_deadline(session)) {
            return NX_ERROR_BUSY;
        }
        nx_result_t progress = nx_tls_service(session);
        if (progress != NX_SUCCESS) {
            return progress;
        }
    }
    nx_result_t result = begin_session(session, deadline, false);
    if (result != NX_SUCCESS) {
        return result;
    }
    size_t capacity =
        bytes < MBEDTLS_SSL_IN_CONTENT_LEN ? bytes : MBEDTLS_SSL_IN_CONTENT_LEN;
    int code = mbedtls_ssl_read(&session->ssl, data, capacity);
    if (code == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        session->bio_active = false;
        result = end_crypto(runtime);
        if (result != NX_SUCCESS) {
            return fail(session, result, code);
        }
        session->diagnostic.peer_closed = true;
        session->diagnostic.state =
            session->local_closed ? NX_TLS_CLOSED : NX_TLS_CLOSING;
        return NX_SUCCESS;
    }
    result = finish_session(session, code);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (code == 0) {
        return fail(session, NX_ERROR_IO, code);
    }
    *received = (size_t)code;
    return NX_SUCCESS;
}

nx_result_t nx_tls_close_step(nx_tls_session_t* session,
                              nx_time_us_t deadline) {
    if (live(session) && session->config.runtime->callback_active) {
        return NX_ERROR_BUSY;
    }
    if (!live(session) || (session->diagnostic.state != NX_TLS_ESTABLISHED &&
                           session->diagnostic.state != NX_TLS_CLOSING &&
                           session->diagnostic.state != NX_TLS_CLOSED)) {
        return NX_ERROR_STATE;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (session->local_closed) {
        return NX_SUCCESS;
    }
    if (session->close_deadline == 0U) {
        session->close_deadline = deadline;
    }
    deadline = session->close_deadline;
    session->diagnostic.state = NX_TLS_CLOSING;
    if (session->stage_bytes != 0U) {
        nx_result_t progress = nx_tls_service(session);
        if (progress != NX_SUCCESS) {
            return progress;
        }
    }
    nx_result_t result = begin_session(session, deadline, false);
    if (result != NX_SUCCESS) {
        return fail(session, result, 0);
    }
    int code = mbedtls_ssl_close_notify(&session->ssl);
    result = finish_session(session, code);
    if (result == NX_SUCCESS) {
        session->local_closed = true;
        if (session->diagnostic.peer_closed) {
            session->diagnostic.state = NX_TLS_CLOSED;
        }
    }
    return result;
}

const nx_byte_stream_t* nx_tls_stream(const nx_tls_session_t* session) {
    return live(session) ? &session->stream : NULL;
}

nx_result_t nx_tls_diagnostic(const nx_tls_session_t* session,
                              nx_tls_diagnostic_t* diagnostic) {
    if (session == NULL || diagnostic == NULL ||
        overlaps(session, sizeof(*session), diagnostic, sizeof(*diagnostic)) ||
        (live(session) &&
         !outside_storage(session, diagnostic, sizeof(*diagnostic)))) {
        return NX_ERROR_INVALID;
    }
    *diagnostic = session->diagnostic;
    return NX_SUCCESS;
}
