/**
 * \file            test_init_helpers.cpp
 * \brief           Implementation of test helper functions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "test_init_helpers.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

/*---------------------------------------------------------------------------*/
/* Execution Tracker Implementation                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize execution tracker
 */
void execution_tracker_init(execution_tracker_t* tracker) {
    if (tracker == nullptr) {
        return;
    }

    tracker->count = 0;
    for (int i = 0; i < MAX_TRACKED_EXECUTIONS; i++) {
        tracker->records[i].name = nullptr;
        tracker->records[i].timestamp = 0;
        tracker->records[i].result = 0;
    }
}

/**
 * \brief           Record execution
 */
void execution_tracker_record(execution_tracker_t* tracker, const char* name,
                              int result) {
    if (tracker == nullptr || name == nullptr) {
        return;
    }

    if (tracker->count >= MAX_TRACKED_EXECUTIONS) {
        return;
    }

    tracker->records[tracker->count].name = name;
    tracker->records[tracker->count].timestamp = test_get_timestamp_ms();
    tracker->records[tracker->count].result = result;
    tracker->count++;
}

/**
 * \brief           Get execution count
 */
int execution_tracker_get_count(const execution_tracker_t* tracker) {
    if (tracker == nullptr) {
        return 0;
    }

    return tracker->count;
}

/**
 * \brief           Check if function was executed
 */
bool execution_tracker_was_executed(const execution_tracker_t* tracker,
                                    const char* name) {
    if (tracker == nullptr || name == nullptr) {
        return false;
    }

    for (int i = 0; i < tracker->count; i++) {
        if (tracker->records[i].name != nullptr &&
            strcmp(tracker->records[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

/**
 * \brief           Get execution order
 */
int execution_tracker_get_order(const execution_tracker_t* tracker,
                                const char* name) {
    if (tracker == nullptr || name == nullptr) {
        return -1;
    }

    for (int i = 0; i < tracker->count; i++) {
        if (tracker->records[i].name != nullptr &&
            strcmp(tracker->records[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

/*---------------------------------------------------------------------------*/
/* Mock Init Functions                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mock init function that succeeds
 */
int mock_init_success(void) {
    return 0;
}

/**
 * \brief           Mock init function that fails
 */
int mock_init_fail(void) {
    return -1;
}

/**
 * \brief           Mock init function with delay
 */
int mock_init_with_delay(uint32_t delay_ms) {
    test_sleep_ms(delay_ms);
    return 0;
}

/*---------------------------------------------------------------------------*/
/* Test Utilities Implementation                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get current timestamp in microseconds
 */
uint64_t test_get_timestamp_us(void) {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration)
        .count();
}

/**
 * \brief           Get current timestamp in milliseconds
 */
uint32_t test_get_timestamp_ms(void) {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration)
            .count());
}

/**
 * \brief           Sleep for specified milliseconds
 */
void test_sleep_ms(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * \brief           Sleep for specified microseconds
 */
void test_sleep_us(uint32_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

/*---------------------------------------------------------------------------*/
/* Memory Utilities Implementation                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Fill memory with pattern
 */
void test_memfill(void* ptr, size_t size, uint8_t pattern) {
    if (ptr == nullptr) {
        return;
    }

    memset(ptr, pattern, size);
}

/**
 * \brief           Check if memory contains pattern
 */
bool test_memcheck(const void* ptr, size_t size, uint8_t pattern) {
    if (ptr == nullptr) {
        return false;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(ptr);
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != pattern) {
            return false;
        }
    }

    return true;
}

/* Global memory statistics */
static memory_stats_t g_memory_stats = {0, 0, 0, 0};

/**
 * \brief           Get memory usage statistics
 */
void test_get_memory_stats(memory_stats_t* stats) {
    if (stats == nullptr) {
        return;
    }

    *stats = g_memory_stats;
}

/**
 * \brief           Reset memory statistics
 */
void test_reset_memory_stats(void) {
    g_memory_stats.total_allocated = 0;
    g_memory_stats.current_allocated = 0;
    g_memory_stats.peak_allocated = 0;
    g_memory_stats.allocation_count = 0;
}

/*---------------------------------------------------------------------------*/
/* String Utilities Implementation                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Safe string copy
 */
void test_strncpy_safe(char* dest, const char* src, size_t size) {
    if (dest == nullptr || src == nullptr || size == 0) {
        return;
    }

#ifdef _MSC_VER
    strncpy_s(dest, size, src, _TRUNCATE);
#else
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
#endif
}

/**
 * \brief           String comparison (case insensitive)
 */
int test_stricmp(const char* s1, const char* s2) {
    if (s1 == nullptr || s2 == nullptr) {
        return -1;
    }

#ifdef _WIN32
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

/**
 * \brief           Check if string starts with prefix
 */
bool test_str_starts_with(const char* str, const char* prefix) {
    if (str == nullptr || prefix == nullptr) {
        return false;
    }

    size_t str_len = strlen(str);
    size_t prefix_len = strlen(prefix);

    if (prefix_len > str_len) {
        return false;
    }

    return strncmp(str, prefix, prefix_len) == 0;
}

/**
 * \brief           Check if string ends with suffix
 */
bool test_str_ends_with(const char* str, const char* suffix) {
    if (str == nullptr || suffix == nullptr) {
        return false;
    }

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) {
        return false;
    }

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/*---------------------------------------------------------------------------*/
/* Performance Measurement Implementation                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Start performance counter
 */
void perf_counter_start(perf_counter_t* counter) {
    if (counter == nullptr) {
        return;
    }

    counter->start_time = test_get_timestamp_us();
    counter->running = true;
}

/**
 * \brief           Stop performance counter
 */
void perf_counter_stop(perf_counter_t* counter) {
    if (counter == nullptr) {
        return;
    }

    counter->end_time = test_get_timestamp_us();
    counter->running = false;
}

/**
 * \brief           Get elapsed time in microseconds
 */
uint64_t perf_counter_elapsed_us(const perf_counter_t* counter) {
    if (counter == nullptr) {
        return 0;
    }

    if (counter->running) {
        return test_get_timestamp_us() - counter->start_time;
    }

    return counter->end_time - counter->start_time;
}

/**
 * \brief           Get elapsed time in milliseconds
 */
uint32_t perf_counter_elapsed_ms(const perf_counter_t* counter) {
    return static_cast<uint32_t>(perf_counter_elapsed_us(counter) / 1000);
}
