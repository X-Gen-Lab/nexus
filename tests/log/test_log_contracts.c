/* Observable logger lifetime/concurrency regressions using the real Native OSAL. */
#define _POSIX_C_SOURCE 200809L
#include "log/log.h"
#include "osal/osal.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void pause_ms(unsigned ms) {
    struct timespec time = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&time, NULL);
}
static uint32_t now_ms(void) {
    uint32_t now;
    assert(osal_get_time_ms(&now) == OSAL_OK);
    return now;
}
static void wait_flag(atomic_bool* flag) {
    uint32_t started = now_ms();
    while (!atomic_load(flag) && (uint32_t)(now_ms() - started) < 2000) pause_ms(1);
    assert(atomic_load(flag));
}
static void assert_no_resources(void) {
    osal_stats_t stats;
    assert(osal_get_stats(&stats) == OSAL_OK);
    assert(!stats.task_count && !stats.mutex_count && !stats.sem_count &&
           !stats.queue_count && !stats.timer_count && !stats.event_count && !stats.mem_alloc_count);
}
static log_config_t async_config(void) {
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.level = LOG_LEVEL_TRACE;
    config.format = "%m";
    config.async_mode = true;
    config.async_queue_size = 8;
    config.async_policy = LOG_ASYNC_POLICY_BLOCK;
    return config;
}
static void test_immediate_shutdown_drains(void) {
    for (unsigned i = 0; i < 30; ++i) {
        log_config_t config = async_config();
        assert(log_init(&config) == LOG_OK);
        log_backend_t* memory = log_backend_memory_create(4096);
        assert(memory && log_backend_register(memory) == LOG_OK);
        assert(log_backend_memory_destroy(memory) == LOG_ERROR_BUSY);
        assert(log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                         "accepted-before-stop") == LOG_OK);
        assert(log_deinit() == LOG_OK);
        char text[4096];
        assert(log_backend_memory_read(memory, text, sizeof(text)) > 0);
        assert(strstr(text, "accepted-before-stop"));
        assert(log_backend_memory_destroy(memory) == LOG_OK);
        assert_no_resources();
    }
}
typedef struct {
    atomic_bool entered, release, written, flush_done;
    atomic_uint flush_calls;
    log_status_t flush_result;
} blocked_context_t;
static log_status_t blocked_write(void* arg, const char* message, size_t length) {
    (void)message; (void)length;
    blocked_context_t* context = arg;
    atomic_store(&context->entered, true);
    wait_flag(&context->release);
    atomic_store(&context->written, true);
    return LOG_OK;
}
static log_status_t blocked_flush(void* arg) {
    blocked_context_t* context = arg;
    assert(atomic_load(&context->written));
    atomic_fetch_add(&context->flush_calls, 1);
    return LOG_OK;
}
static void* flush_worker(void* arg) {
    blocked_context_t* context = arg;
    context->flush_result = log_async_flush();
    atomic_store(&context->flush_done, true);
    return NULL;
}
static log_backend_t blocked_backend(blocked_context_t* context) {
    return (log_backend_t){.name = "blocked", .ctx = context, .write = blocked_write,
        .flush = blocked_flush, .enabled = true, .min_level = LOG_LEVEL_TRACE};
}
static void test_flush_includes_callback(void) {
    blocked_context_t context = {0};
    log_backend_t backend = blocked_backend(&context);
    log_config_t config = async_config();
    assert(log_init(&config) == LOG_OK && log_backend_register(&backend) == LOG_OK);
    assert(log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__, "queued") == LOG_OK);
    wait_flag(&context.entered);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, flush_worker, &context) == 0);
    pause_ms(20);
    assert(!atomic_load(&context.flush_done));
    atomic_store(&context.release, true);
    assert(pthread_join(thread, NULL) == 0);
    assert(context.flush_result == LOG_OK && atomic_load(&context.flush_calls) == 1);
    assert(log_deinit() == LOG_OK);
    assert_no_resources();
}
static void test_busy_shutdown_retains_ownership(void) {
    blocked_context_t context = {0};
    log_backend_t backend = blocked_backend(&context);
    log_config_t config = async_config();
    assert(log_init(&config) == LOG_OK && log_backend_register(&backend) == LOG_OK);
    assert(log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__, "queued") == LOG_OK);
    wait_flag(&context.entered);
    uint32_t started = now_ms();
    assert(log_deinit() == LOG_ERROR_BUSY);
    assert((uint32_t)(now_ms() - started) >= 900 && (uint32_t)(now_ms() - started) < 1600);
    assert(log_is_initialized());
    osal_stats_t stats;
    assert(osal_get_stats(&stats) == OSAL_OK && stats.task_count == 1 && stats.queue_count == 1);
    atomic_store(&context.release, true);
    assert(log_deinit() == LOG_OK);
    assert_no_resources();
}
static log_status_t callback_reentry(void* unused, const char* message, size_t length) {
    (void)unused; (void)message; (void)length;
    assert(log_deinit() == LOG_ERROR_BUSY);
    assert(log_backend_unregister("reentrant") == LOG_ERROR_BUSY);
    assert(log_async_flush() == LOG_ERROR_BUSY);
    assert(log_write_raw("recursive", 9) == LOG_ERROR_BUSY);
    return LOG_OK;
}
static void test_callback_reentry(void) {
    log_backend_t backend = {.name = "reentrant", .write = callback_reentry,
        .enabled = true, .min_level = LOG_LEVEL_TRACE};
    assert(log_init(NULL) == LOG_OK && log_backend_register(&backend) == LOG_OK);
    assert(log_write_raw("message", 7) == LOG_OK);
    assert(log_backend_unregister("reentrant") == LOG_OK);
    assert(log_deinit() == LOG_OK);
    assert_no_resources();
}
static atomic_bool producers_stop;
static atomic_uint producer_failures;
static log_backend_t* concurrent_memory;
static void* producer(void* unused) {
    (void)unused;
    for (unsigned i = 0; i < 2000 && !atomic_load(&producers_stop); ++i) {
        log_status_t result = log_write(LOG_LEVEL_INFO, "parallel", __FILE__, __LINE__, __func__,
                                         "message-%u", i);
        if (result != LOG_OK && result != LOG_ERROR_BUSY && result != LOG_ERROR_NOT_INIT &&
            result != LOG_ERROR_FULL && result != LOG_ERROR_TIMEOUT)
            atomic_fetch_add(&producer_failures, 1);
    }
    return NULL;
}
static void* memory_reader(void* unused) {
    (void)unused;
    char text[31];
    while (!atomic_load(&producers_stop)) {
        assert(log_backend_memory_size(concurrent_memory) <= 4096);
        assert(log_backend_memory_read(concurrent_memory, text, sizeof(text)) <= sizeof(text));
        log_backend_memory_clear(concurrent_memory);
    }
    return NULL;
}
static void test_concurrent_close_and_ring_access(void) {
    log_config_t config = async_config();
    config.async_policy = LOG_ASYNC_POLICY_DROP_OLDEST;
    assert(log_init(&config) == LOG_OK);
    concurrent_memory = log_backend_memory_create(4096);
    assert(concurrent_memory && log_backend_register(concurrent_memory) == LOG_OK);
    atomic_store(&producers_stop, false);
    atomic_store(&producer_failures, 0);
    pthread_t workers[5];
    for (unsigned i = 0; i < 4; ++i) assert(pthread_create(&workers[i], NULL, producer, NULL) == 0);
    assert(pthread_create(&workers[4], NULL, memory_reader, NULL) == 0);
    pause_ms(10);
    assert(log_deinit() == LOG_OK);
    atomic_store(&producers_stop, true);
    for (unsigned i = 0; i < 5; ++i) assert(pthread_join(workers[i], NULL) == 0);
    assert(!atomic_load(&producer_failures));
    assert(log_backend_memory_destroy(concurrent_memory) == LOG_OK);
    assert_no_resources();
}
static void test_metadata_truncation(void) {
    assert(log_init(NULL) == LOG_OK && log_set_format("%M%F%m") == LOG_OK);
    log_backend_t* memory = log_backend_memory_create(4096);
    assert(memory && log_backend_register(memory) == LOG_OK);
    char metadata[4096];
    memset(metadata, 'x', sizeof(metadata) - 1);
    metadata[sizeof(metadata) - 1] = 0;
    assert(log_write(LOG_LEVEL_INFO, metadata, metadata, 1, __func__, "payload") == LOG_OK);
    assert(log_backend_memory_size(memory) > 0 && log_backend_memory_size(memory) < LOG_MAX_MSG_LEN * 2);
    assert(log_deinit() == LOG_OK && log_backend_memory_destroy(memory) == LOG_OK);
    assert_no_resources();
}
typedef struct { bool flush_error, deinit_error; unsigned deinit_calls; } failure_context_t;
static log_status_t successful_write(void* ctx, const char* msg, size_t len) {
    (void)ctx; (void)msg; (void)len;
    return LOG_OK;
}
static log_status_t failing_flush(void* ctx) {
    return ((failure_context_t*)ctx)->flush_error ? LOG_ERROR : LOG_OK;
}
static log_status_t failing_deinit(void* ctx) {
    failure_context_t* failure = ctx;
    ++failure->deinit_calls;
    return failure->deinit_error ? LOG_ERROR : LOG_OK;
}
static void test_backend_failure_ownership(void) {
    failure_context_t failure = {.flush_error = true};
    log_backend_t backend = {.name = "failure", .ctx = &failure,
        .write = successful_write, .flush = failing_flush, .deinit = failing_deinit,
        .enabled = true, .min_level = LOG_LEVEL_TRACE};
    log_config_t config = async_config();
    assert(log_init(&config) == LOG_OK && log_backend_register(&backend) == LOG_OK);
    assert(log_async_flush() == LOG_ERROR_BACKEND);
    assert(log_deinit() == LOG_ERROR_BACKEND);
    assert(log_is_initialized() && failure.deinit_calls == 0);
    assert(log_init(NULL) == LOG_ERROR_BUSY);
    failure.flush_error = false;
    failure.deinit_error = true;
    assert(log_deinit() == LOG_ERROR_BACKEND && failure.deinit_calls == 1);
    failure.deinit_error = false;
    assert(log_deinit() == LOG_OK && failure.deinit_calls == 2);
    assert_no_resources();

    failure.deinit_error = true;
    assert(log_init(NULL) == LOG_OK && log_backend_register(&backend) == LOG_OK);
    assert(log_backend_unregister("failure") == LOG_ERROR_BACKEND);
    assert(log_backend_get("failure") == &backend);
    assert(log_write_raw("retained", 8) == LOG_OK);
    failure.deinit_error = false;
    assert(log_backend_unregister("failure") == LOG_OK);
    assert(log_deinit() == LOG_OK);
    assert_no_resources();
}
static log_status_t delayed_flush(void* arg) {
    blocked_context_t* context = arg;
    atomic_store(&context->entered, true);
    wait_flag(&context->release);
    return LOG_OK;
}
static void test_late_flush_reply_is_safe(void) {
    blocked_context_t context = {0};
    log_backend_t backend = {.name = "late-reply", .ctx = &context,
        .write = successful_write, .flush = delayed_flush, .enabled = true,
        .min_level = LOG_LEVEL_TRACE};
    log_config_t config = async_config();
    assert(log_init(&config) == LOG_OK && log_backend_register(&backend) == LOG_OK);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, flush_worker, &context) == 0);
    wait_flag(&context.entered);
    assert(pthread_join(thread, NULL) == 0);
    assert(context.flush_result == LOG_ERROR_TIMEOUT);
    /* The reply token is gone while the callback still owns the backend.
     * Releasing it exercises a late send to a stale generation, never a
     * callback writing through an expired caller stack pointer. */
    atomic_store(&context.release, true);
    assert(log_deinit() == LOG_OK);
    assert_no_resources();
}
int main(void) {
    assert(osal_init() == OSAL_OK);
    test_immediate_shutdown_drains();
    test_flush_includes_callback();
    test_busy_shutdown_retains_ownership();
    test_callback_reentry();
    test_concurrent_close_and_ring_access();
    test_metadata_truncation();
    test_backend_failure_ownership();
    test_late_flush_reply_is_safe();
    puts("8 real logging lifetime/concurrency contract groups passed");
    return 0;
}
