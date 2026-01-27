/**
 * \file            test_init_helpers.h
 * \brief           Test helper functions for Init Framework tests
 * \author          Nexus Team
 */

#ifndef TEST_INIT_HELPERS_H
#define TEST_INIT_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cstddef>
#include <stdbool.h>
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* Test Helper Macros                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Expect value within range
 */
#define EXPECT_IN_RANGE(val, min, max)                                         \
    do {                                                                       \
        EXPECT_GE(val, min);                                                   \
        EXPECT_LE(val, max);                                                   \
    } while (0)

/**
 * \brief           Expect approximately equal (for floating point)
 */
#define EXPECT_APPROX_EQ(val1, val2, epsilon) EXPECT_NEAR(val1, val2, epsilon)

/*---------------------------------------------------------------------------*/
/* Test Execution Tracking                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Maximum number of tracked executions
 */
#define MAX_TRACKED_EXECUTIONS 100

/**
 * \brief           Execution record
 */
typedef struct {
    const char* name;   /**< Function name */
    uint32_t timestamp; /**< Execution timestamp */
    int result;         /**< Function result */
} execution_record_t;

/**
 * \brief           Execution tracker
 */
typedef struct {
    execution_record_t records[MAX_TRACKED_EXECUTIONS];
    int count;
} execution_tracker_t;

/**
 * \brief           Initialize execution tracker
 */
void execution_tracker_init(execution_tracker_t* tracker);

/**
 * \brief           Record execution
 */
void execution_tracker_record(execution_tracker_t* tracker, const char* name,
                              int result);

/**
 * \brief           Get execution count
 */
int execution_tracker_get_count(const execution_tracker_t* tracker);

/**
 * \brief           Check if function was executed
 */
bool execution_tracker_was_executed(const execution_tracker_t* tracker,
                                    const char* name);

/**
 * \brief           Get execution order
 */
int execution_tracker_get_order(const execution_tracker_t* tracker,
                                const char* name);

/*---------------------------------------------------------------------------*/
/* Mock Init Functions                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mock init function that succeeds
 */
int mock_init_success(void);

/**
 * \brief           Mock init function that fails
 */
int mock_init_fail(void);

/**
 * \brief           Mock init function with delay
 */
int mock_init_with_delay(uint32_t delay_ms);

/*---------------------------------------------------------------------------*/
/* Test Utilities                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get current timestamp in microseconds
 */
uint64_t test_get_timestamp_us(void);

/**
 * \brief           Get current timestamp in milliseconds
 */
uint32_t test_get_timestamp_ms(void);

/**
 * \brief           Sleep for specified milliseconds
 */
void test_sleep_ms(uint32_t ms);

/**
 * \brief           Sleep for specified microseconds
 */
void test_sleep_us(uint32_t us);

/*---------------------------------------------------------------------------*/
/* Memory Utilities                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Fill memory with pattern
 */
void test_memfill(void* ptr, size_t size, uint8_t pattern);

/**
 * \brief           Check if memory contains pattern
 */
bool test_memcheck(const void* ptr, size_t size, uint8_t pattern);

/**
 * \brief           Get memory usage statistics
 */
typedef struct {
    size_t total_allocated;
    size_t current_allocated;
    size_t peak_allocated;
    int allocation_count;
} memory_stats_t;

void test_get_memory_stats(memory_stats_t* stats);
void test_reset_memory_stats(void);

/*---------------------------------------------------------------------------*/
/* String Utilities                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Safe string copy
 */
void test_strncpy_safe(char* dest, const char* src, size_t size);

/**
 * \brief           String comparison (case insensitive)
 */
int test_stricmp(const char* s1, const char* s2);

/**
 * \brief           Check if string starts with prefix
 */
bool test_str_starts_with(const char* str, const char* prefix);

/**
 * \brief           Check if string ends with suffix
 */
bool test_str_ends_with(const char* str, const char* suffix);

/*---------------------------------------------------------------------------*/
/* Performance Measurement                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Performance counter
 */
typedef struct {
    uint64_t start_time;
    uint64_t end_time;
    bool running;
} perf_counter_t;

/**
 * \brief           Start performance counter
 */
void perf_counter_start(perf_counter_t* counter);

/**
 * \brief           Stop performance counter
 */
void perf_counter_stop(perf_counter_t* counter);

/**
 * \brief           Get elapsed time in microseconds
 */
uint64_t perf_counter_elapsed_us(const perf_counter_t* counter);

/**
 * \brief           Get elapsed time in milliseconds
 */
uint32_t perf_counter_elapsed_ms(const perf_counter_t* counter);

/*---------------------------------------------------------------------------*/
/* Test Assertions                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Assert that value is in range
 */
#define TEST_ASSERT_IN_RANGE(val, min, max)                                    \
    do {                                                                       \
        if ((val) < (min) || (val) > (max)) {                                  \
            fprintf(stderr, "Assertion failed: %s not in range [%d, %d]\n",    \
                    #val, (int)(min), (int)(max));                             \
            return false;                                                      \
        }                                                                      \
    } while (0)

/**
 * \brief           Assert that pointer is not NULL
 */
#define TEST_ASSERT_NOT_NULL(ptr)                                              \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            fprintf(stderr, "Assertion failed: %s is NULL\n", #ptr);           \
            return false;                                                      \
        }                                                                      \
    } while (0)

/**
 * \brief           Assert that condition is true
 */
#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "Assertion failed: %s\n", #cond);                  \
            return false;                                                      \
        }                                                                      \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* TEST_INIT_HELPERS_H */
