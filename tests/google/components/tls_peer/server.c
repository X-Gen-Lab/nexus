/**
 * \file            server.c
 * \brief           Independent actual Mbed TLS mutual-auth host fixture peer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Host getrandom is explicit test input, not MCU fallback.
 */
#define _POSIX_C_SOURCE 200809L
#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/pk.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"
#include "psa/crypto_extra.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

_Alignas(max_align_t) static uint8_t arena[65536];
static uint64_t limit_us;

static uint64_t now_us(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return UINT64_MAX;
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000) +
           (uint64_t)value.tv_nsec / 1000U;
}
mbedtls_ms_time_t mbedtls_ms_time(void) {
    return (mbedtls_ms_time_t)(now_us() / UINT64_C(1000));
}
struct tm* mbedtls_platform_gmtime_r(const mbedtls_time_t* seconds,
                                     struct tm* output) {
    return seconds != NULL && output != NULL ? gmtime_r(seconds, output) : NULL;
}
static _Noreturn void fixture_invariant_failure(int status) {
    (void)status;
    _exit(75);
}
static void erase(uint8_t* output, size_t bytes) {
    volatile uint8_t* cursor = output;
    for (size_t i = 0U; i < bytes; ++i) {
        cursor[i] = 0U;
    }
}
int mbedtls_platform_get_entropy(psa_driver_get_entropy_flags_t flags,
                                 size_t* estimate, unsigned char* output,
                                 size_t bytes) {
    *estimate = 0U;
    if (flags != PSA_DRIVER_GET_ENTROPY_FLAGS_NONE || bytes > SIZE_MAX / 8U) {
        return PSA_ERROR_NOT_SUPPORTED;
    }
    size_t copied = 0U;
    while (copied < bytes && now_us() < limit_us) {
        ssize_t count =
            getrandom(output + copied, bytes - copied, GRND_NONBLOCK);
        if (count > 0) {
            copied += (size_t)count;
        } else if (count < 0 && errno != EINTR && errno != EAGAIN) {
            break;
        }
    }
    if (copied != bytes) {
        erase(output, bytes);
        return PSA_ERROR_INSUFFICIENT_ENTROPY;
    }
    *estimate = bytes * 8U;
    return 0;
}
static mbedtls_time_t utc(mbedtls_time_t* output) {
    mbedtls_time_t value = (mbedtls_time_t)1800000000;
    if (output != NULL) {
        *output = value;
    }
    return value;
}
static int write_bytes(void* context, const unsigned char* input,
                       size_t bytes) {
    int descriptor = *(int*)context;
    ssize_t count = send(descriptor, input, bytes, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (count >= 0) {
        return (int)count;
    }
    return errno == EAGAIN || errno == EWOULDBLOCK
               ? MBEDTLS_ERR_SSL_WANT_WRITE
               : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
static int read_bytes(void* context, unsigned char* output, size_t bytes) {
    int descriptor = *(int*)context;
    ssize_t count = recv(descriptor, output, bytes, MSG_DONTWAIT);
    if (count >= 0) {
        return (int)count;
    }
    return errno == EAGAIN || errno == EWOULDBLOCK
               ? MBEDTLS_ERR_SSL_WANT_READ
               : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
static int wait_io(int descriptor, int status) {
    if (now_us() >= limit_us) {
        return -1;
    }
    struct pollfd ready = {
        descriptor, status == MBEDTLS_ERR_SSL_WANT_WRITE ? POLLOUT : POLLIN, 0};
    return poll(&ready, 1U, 10) < 0 && errno != EINTR ? -1 : 0;
}
static size_t read_file(const char* path, uint8_t* bytes, size_t capacity) {
    int descriptor = open(path, O_RDONLY);
    if (descriptor < 0) {
        return 0U;
    }
    ssize_t count = read(descriptor, bytes, capacity);
    uint8_t extra;
    ssize_t rest = read(descriptor, &extra, 1U);
    close(descriptor);
    return count > 0 && rest == 0 ? (size_t)count : 0U;
}
int main(int argc, char** argv) {
    if (argc != 5) {
        return 2;
    }
    char* end;
    long parsed = strtol(argv[1], &end, 10);
    if (*end != '\0' || parsed < 0 || parsed > INT_MAX) {
        return 3;
    }
    int descriptor = (int)parsed;
    uint8_t ca_bytes[4096], own_bytes[4096], key_bytes[2048];
    size_t ca_size = read_file(argv[2], ca_bytes, sizeof(ca_bytes));
    size_t own_size = read_file(argv[3], own_bytes, sizeof(own_bytes));
    size_t key_size = read_file(argv[4], key_bytes, sizeof(key_bytes));
    if (ca_size == 0U || own_size == 0U || key_size == 0U) {
        return 4;
    }
    limit_us = now_us() + UINT64_C(5000000);
    (void)mbedtls_platform_set_exit(fixture_invariant_failure);
    mbedtls_memory_buffer_alloc_init(arena, sizeof(arena));
    (void)mbedtls_platform_set_time(utc);
    if (psa_crypto_init() != PSA_SUCCESS) {
        return 5;
    }
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt ca, identity;
    mbedtls_pk_context key;
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&config);
    mbedtls_x509_crt_init(&ca);
    mbedtls_x509_crt_init(&identity);
    mbedtls_pk_init(&key);
    int result = mbedtls_x509_crt_parse_der(&ca, ca_bytes, ca_size);
    if (result == 0) {
        result = mbedtls_x509_crt_parse_der(&identity, own_bytes, own_size);
    }
    if (result == 0) {
        result = mbedtls_pk_parse_key(&key, key_bytes, key_size, NULL, 0U);
    }
    erase(key_bytes, sizeof(key_bytes));
    if (result == 0) {
        result = mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER,
                                             MBEDTLS_SSL_TRANSPORT_STREAM,
                                             MBEDTLS_SSL_PRESET_DEFAULT);
    }
    if (result == 0) {
        mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&config, &ca, NULL);
        result = mbedtls_ssl_conf_own_cert(&config, &identity, &key);
    }
    if (result == 0) {
        result = mbedtls_ssl_setup(&ssl, &config);
    }
    if (result == 0) {
        mbedtls_ssl_set_bio(&ssl, &descriptor, write_bytes, read_bytes, NULL);
        for (;;) {
            result = mbedtls_ssl_handshake(&ssl);
            if (result == 0) {
                break;
            }
            if ((result != MBEDTLS_ERR_SSL_WANT_READ &&
                 result != MBEDTLS_ERR_SSL_WANT_WRITE) ||
                wait_io(descriptor, result) != 0) {
                break;
            }
        }
    }
    while (result == 0) {
        uint8_t bytes[1024];
        int received = mbedtls_ssl_read(&ssl, bytes, sizeof(bytes));
        if (received > 0) {
            size_t offset = 0U;
            while (offset < (size_t)received) {
                int written = mbedtls_ssl_write(&ssl, bytes + offset,
                                                (size_t)received - offset);
                if (written > 0) {
                    offset += (size_t)written;
                } else if ((written == MBEDTLS_ERR_SSL_WANT_READ ||
                            written == MBEDTLS_ERR_SSL_WANT_WRITE) &&
                           wait_io(descriptor, written) == 0) {
                    continue;
                } else {
                    result = written;
                    break;
                }
            }
        } else if (received == MBEDTLS_ERR_SSL_WANT_READ ||
                   received == MBEDTLS_ERR_SSL_WANT_WRITE) {
            result = wait_io(descriptor, received);
        } else if (received == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            do {
                result = mbedtls_ssl_close_notify(&ssl);
            } while ((result == MBEDTLS_ERR_SSL_WANT_READ ||
                      result == MBEDTLS_ERR_SSL_WANT_WRITE) &&
                     wait_io(descriptor, result) == 0);
            break;
        } else {
            result = received == 0 ? -1 : received;
        }
    }
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_pk_free(&key);
    mbedtls_x509_crt_free(&identity);
    mbedtls_x509_crt_free(&ca);
    mbedtls_psa_crypto_free();
    mbedtls_memory_buffer_alloc_free();
    erase(arena, sizeof(arena));
    close(descriptor);
    if (result != 0) {
        fprintf(stderr, "insecure TLS fixture peer ended with %d\n", result);
    }
    return result == 0 ? 0 : 20;
}
