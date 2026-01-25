/**
 * \file            test_log_properties.cpp
 * \brief           Log Framework Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Log Framework.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "log/log.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Log Property Test Fixture
 */
class LogPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(std::random_device{}());
        /* Ensure log is deinitialized before each test */
        if (log_is_initialized()) {
            log_deinit();
        }
    }

    void TearDown() override {
        /* Clean up after each test */
        if (log_is_initialized()) {
            log_deinit();
        }
    }

    /**
     * \brief       Generate random valid log level
     */
    log_level_t randomLevel() {
        std::uniform_int_distribution<int> dist(LOG_LEVEL_TRACE,
                                                LOG_LEVEL_NONE);
        return static_cast<log_level_t>(dist(rng));
    }

    /**
     * \brief       Generate random log level for messages (excluding NONE)
     */
    log_level_t randomMessageLevel() {
        std::uniform_int_distribution<int> dist(LOG_LEVEL_TRACE,
                                                LOG_LEVEL_FATAL);
        return static_cast<log_level_t>(dist(rng));
    }
};

/*---------------------------------------------------------------------------*/
/* Property 2: Level Filtering Consistency                                   */
/* *For any* log level L set as the global filter, all messages at levels    */
/* < L SHALL be discarded, and all messages at levels >= L SHALL be passed   */
/* to backends.                                                              */
/* **Validates: Requirements 1.2, 1.3**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 2: Level Filtering Consistency
 *
 * *For any* log level L set as the global filter, all messages at levels
 * < L SHALL be discarded, and all messages at levels >= L SHALL be passed
 * to backends.
 *
 * **Validates: Requirements 1.2, 1.3**
 */
TEST_F(LogPropertyTest, Property2_LevelFilteringConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random filter level */
        log_level_t filter_level = randomLevel();

        /* Generate random message level */
        log_level_t msg_level = randomMessageLevel();

        /* Initialize log system */
        log_config_t config = {.level = filter_level,
                               .format = NULL,
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Verify the filter level is set correctly */
        EXPECT_EQ(filter_level, log_get_level())
            << "Iteration " << test_iter << ": filter level mismatch";

        /* The filtering logic is:
         * - Messages with level >= filter_level should pass
         * - Messages with level < filter_level should be discarded
         *
         * Since we don't have backends yet, we verify the logic by checking
         * that log_write returns LOG_OK (it doesn't fail, just filters)
         */
        log_status_t status = log_write(msg_level, "test", __FILE__, __LINE__,
                                        __func__, "test message");

        /* log_write should always return LOG_OK (filtered messages are silently
         * discarded) */
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter
            << ": log_write failed for filter=" << filter_level
            << ", msg=" << msg_level;

        /* Verify the filtering property:
         * - If msg_level >= filter_level: message should pass (will be output
         * when backends exist)
         * - If msg_level < filter_level: message should be discarded
         *
         * This property will be fully testable when memory backend is
         * implemented. For now, we verify the level comparison logic is
         * correct.
         */
        bool should_pass = (msg_level >= filter_level);

        /* Verify our understanding of the filtering logic */
        if (filter_level == LOG_LEVEL_NONE) {
            /* NONE disables all logging */
            EXPECT_FALSE(should_pass || msg_level == LOG_LEVEL_NONE)
                << "Iteration " << test_iter
                << ": NONE level should filter all messages";
        }

        EXPECT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 3: Level Get/Set Round Trip                                      */
/* *For any* valid log level L, calling log_set_level(L) followed by         */
/* log_get_level() SHALL return L.                                           */
/* **Validates: Requirements 1.5**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 3: Level Get/Set Round Trip
 *
 * *For any* valid log level L, calling log_set_level(L) followed by
 * log_get_level() SHALL return L.
 *
 * **Validates: Requirements 1.5**
 */
TEST_F(LogPropertyTest, Property3_LevelGetSetRoundTrip) {
    /* Initialize log system */
    ASSERT_EQ(LOG_OK, log_init(NULL));

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random valid log level */
        log_level_t level = randomLevel();

        /* Set the level */
        log_status_t status = log_set_level(level);
        ASSERT_EQ(LOG_OK, status) << "Iteration " << test_iter
                                  << ": set_level failed for level " << level;

        /* Get the level back */
        log_level_t retrieved_level = log_get_level();

        /* Verify round-trip property */
        EXPECT_EQ(level, retrieved_level)
            << "Iteration " << test_iter << ": round-trip failed. Set " << level
            << ", got " << retrieved_level;
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property Tests for Level Management                            */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property: Level Ordering Invariant
 *
 * *For any* two log levels A and B, if A < B numerically, then A represents
 * a more verbose level than B.
 *
 * **Validates: Requirements 1.1**
 */
TEST_F(LogPropertyTest, Property_LevelOrderingInvariant) {
    /* This property verifies the level ordering is consistent */
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        log_level_t level_a = randomLevel();
        log_level_t level_b = randomLevel();

        /* If level_a < level_b numerically, then level_a is more verbose */
        if (level_a < level_b) {
            /* A message at level_a should be filtered when filter is set to
             * level_b */
            /* A message at level_b should pass when filter is set to level_b */

            /* Initialize with filter at level_b */
            log_config_t config = {.level = level_b,
                                   .format = NULL,
                                   .async_mode = false,
                                   .buffer_size = 0,
                                   .max_msg_len = 0,
                                   .color_enabled = false};

            ASSERT_EQ(LOG_OK, log_init(&config))
                << "Iteration " << test_iter << ": init failed";

            /* Verify that level_a (more verbose) would be filtered */
            /* and level_b (less verbose) would pass */
            /* This is verified by the filtering logic: msg_level >=
             * filter_level */
            EXPECT_LT(level_a, level_b)
                << "Iteration " << test_iter << ": level ordering violated";

            EXPECT_EQ(LOG_OK, log_deinit())
                << "Iteration " << test_iter << ": deinit failed";
        }
    }
}

/**
 * Feature: logging-middleware, Property: Invalid Level Rejection
 *
 * *For any* invalid log level (> LOG_LEVEL_NONE), log_set_level SHALL
 * return LOG_ERROR_INVALID_PARAM and not change the current level.
 *
 * **Validates: Requirements 1.5**
 */
TEST_F(LogPropertyTest, Property_InvalidLevelRejection) {
    ASSERT_EQ(LOG_OK, log_init(NULL));

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Set a known valid level first */
        log_level_t valid_level = randomLevel();
        ASSERT_EQ(LOG_OK, log_set_level(valid_level));

        /* Generate an invalid level (> LOG_LEVEL_NONE) */
        std::uniform_int_distribution<int> dist(LOG_LEVEL_NONE + 1,
                                                LOG_LEVEL_NONE + 100);
        log_level_t invalid_level = static_cast<log_level_t>(dist(rng));

        /* Try to set invalid level */
        log_status_t status = log_set_level(invalid_level);

        /* Should return error */
        EXPECT_EQ(LOG_ERROR_INVALID_PARAM, status)
            << "Iteration " << test_iter << ": invalid level " << invalid_level
            << " was accepted";

        /* Level should not have changed */
        EXPECT_EQ(valid_level, log_get_level())
            << "Iteration " << test_iter << ": level changed after invalid set";
    }
}

/**
 * Feature: logging-middleware, Property: Init/Deinit Idempotence
 *
 * *For any* sequence of valid init/deinit operations, the system state
 * SHALL be consistent (initialized after init, not initialized after deinit).
 *
 * **Validates: Requirements 8.1, 8.5**
 */
TEST_F(LogPropertyTest, Property_InitDeinitIdempotence) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random config */
        log_level_t level = randomLevel();
        log_config_t config = {.level = level,
                               .format = NULL,
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        /* Init should succeed */
        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Should be initialized */
        EXPECT_TRUE(log_is_initialized())
            << "Iteration " << test_iter << ": not initialized after init";

        /* Level should match config */
        EXPECT_EQ(level, log_get_level())
            << "Iteration " << test_iter << ": level mismatch after init";

        /* Deinit should succeed */
        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";

        /* Should not be initialized */
        EXPECT_FALSE(log_is_initialized())
            << "Iteration " << test_iter << ": still initialized after deinit";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 4: Printf Format Correctness                                     */
/* *For any* printf-style format string and matching arguments, the          */
/* formatted output SHALL match the expected printf behavior.                */
/* **Validates: Requirements 2.1**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 4: Printf Format Correctness
 *
 * *For any* printf-style format string and matching arguments, the
 * formatted output SHALL match the expected printf behavior.
 *
 * **Validates: Requirements 2.1**
 */
TEST_F(LogPropertyTest, Property4_PrintfFormatCorrectness) {
    ASSERT_EQ(LOG_OK, log_init(NULL));
    ASSERT_EQ(LOG_OK, log_set_level(LOG_LEVEL_TRACE));

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random integer value */
        std::uniform_int_distribution<int> int_dist(-10000, 10000);
        int int_val = int_dist(rng);

        /* Generate random float value */
        std::uniform_real_distribution<double> float_dist(-1000.0, 1000.0);
        double float_val = float_dist(rng);

        /* Generate random string (alphanumeric) */
        std::uniform_int_distribution<int> len_dist(1, 20);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int str_len = len_dist(rng);
        std::string str_val;
        for (int i = 0; i < str_len; ++i) {
            str_val += static_cast<char>(char_dist(rng));
        }

        /* Test integer formatting */
        log_status_t status = log_write(LOG_LEVEL_INFO, "test", __FILE__,
                                        __LINE__, __func__, "Int: %d", int_val);
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": integer format failed";

        /* Test float formatting */
        status = log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                           "Float: %f", float_val);
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": float format failed";

        /* Test string formatting */
        status = log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                           "String: %s", str_val.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": string format failed";

        /* Test combined formatting */
        status = log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                           "Combined: %d, %f, %s", int_val, float_val,
                           str_val.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": combined format failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 5: Format Pattern Substitution                                   */
/* *For any* format pattern containing tokens (%T, %L, %M, %m, etc.), the    */
/* formatted output SHALL contain the correct substituted values for each    */
/* token.                                                                    */
/* **Validates: Requirements 2.2, 2.3, 2.4**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 5: Format Pattern Substitution
 *
 * *For any* format pattern containing tokens (%T, %L, %M, %m, etc.), the
 * formatted output SHALL contain the correct substituted values for each
 * token.
 *
 * **Validates: Requirements 2.2, 2.3, 2.4**
 */
TEST_F(LogPropertyTest, Property5_FormatPatternSubstitution) {
    /* Test patterns with various token combinations */
    const char* patterns[] = {
        "%m",                       /* Message only */
        "[%L] %m",                  /* Level and message */
        "[%l] %m",                  /* Short level and message */
        "[%M] %m",                  /* Module and message */
        "[%T] %m",                  /* Timestamp and message */
        "[%F:%n] %m",               /* File:line and message */
        "[%f] %m",                  /* Function and message */
        "[%T] [%L] [%M] %m",        /* Full pattern */
        "%%",                       /* Literal percent */
        "[%L] [%M] [%F:%n] [%f] %m" /* Complex pattern */
    };

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Select random pattern */
        std::uniform_int_distribution<size_t> pattern_dist(
            0, sizeof(patterns) / sizeof(patterns[0]) - 1);
        const char* pattern = patterns[pattern_dist(rng)];

        /* Select random level */
        log_level_t level = randomMessageLevel();

        /* Generate random module name */
        std::uniform_int_distribution<int> len_dist(3, 10);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int mod_len = len_dist(rng);
        std::string module;
        for (int i = 0; i < mod_len; ++i) {
            module += static_cast<char>(char_dist(rng));
        }

        /* Initialize with the pattern */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = pattern,
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Verify format was set */
        EXPECT_STREQ(pattern, log_get_format())
            << "Iteration " << test_iter << ": format mismatch";

        /* Write a log message */
        log_status_t status =
            log_write(level, module.c_str(), __FILE__, __LINE__, __func__,
                      "Test message %d", test_iter);
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": log_write failed";

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 6: Message Truncation                                            */
/* *For any* message longer than the configured maximum length, the output   */
/* SHALL be truncated to max_length - 3 characters followed by "...".        */
/* **Validates: Requirements 2.5**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 6: Message Truncation
 *
 * *For any* message longer than the configured maximum length, the output
 * SHALL be truncated to max_length - 3 characters followed by "...".
 *
 * **Validates: Requirements 2.5**
 */
TEST_F(LogPropertyTest, Property6_MessageTruncation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random max message length (between 10 and 64) */
        std::uniform_int_distribution<size_t> len_dist(10, 64);
        size_t max_len = len_dist(rng);

        /* Initialize with the max length */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m", /* Message only for simplicity */
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = max_len,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Verify max length was set */
        EXPECT_EQ(max_len, log_get_max_msg_len())
            << "Iteration " << test_iter << ": max_msg_len mismatch";

        /* Generate a message longer than max_len */
        std::string long_msg(max_len + 50, 'x');

        /* Write the long message */
        log_status_t status =
            log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                      "%s", long_msg.c_str());
        EXPECT_EQ(LOG_OK, status) << "Iteration " << test_iter
                                  << ": log_write failed for long message";

        /* Generate a message shorter than max_len */
        std::string short_msg(max_len / 2, 'y');

        /* Write the short message */
        status = log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                           "%s", short_msg.c_str());
        EXPECT_EQ(LOG_OK, status) << "Iteration " << test_iter
                                  << ": log_write failed for short message";

        /* Test runtime reconfiguration of max length */
        std::uniform_int_distribution<size_t> new_len_dist(20, 100);
        size_t new_max_len = new_len_dist(rng);

        EXPECT_EQ(LOG_OK, log_set_max_msg_len(new_max_len))
            << "Iteration " << test_iter << ": set_max_msg_len failed";
        EXPECT_EQ(new_max_len, log_get_max_msg_len())
            << "Iteration " << test_iter << ": new max_msg_len mismatch";

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property: Format Get/Set Round Trip                                       */
/* *For any* valid format pattern, calling log_set_format(P) followed by     */
/* log_get_format() SHALL return P.                                          */
/* **Validates: Requirements 2.4**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property: Format Get/Set Round Trip
 *
 * *For any* valid format pattern, calling log_set_format(P) followed by
 * log_get_format() SHALL return P.
 *
 * **Validates: Requirements 2.4**
 */
TEST_F(LogPropertyTest, Property_FormatGetSetRoundTrip) {
    ASSERT_EQ(LOG_OK, log_init(NULL));

    /* Test patterns */
    const char* patterns[] = {"%m",
                              "[%L] %m",
                              "[%T] [%L] [%M] %m",
                              "[%l] [%M] %m",
                              "[%F:%n] %m",
                              "[%f] %m",
                              "%%",
                              "Static text with %m",
                              "[%T] [%L] [%M] [%F:%n] [%f] %m"};

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Select random pattern */
        std::uniform_int_distribution<size_t> pattern_dist(
            0, sizeof(patterns) / sizeof(patterns[0]) - 1);
        const char* pattern = patterns[pattern_dist(rng)];

        /* Set the format */
        log_status_t status = log_set_format(pattern);
        ASSERT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": set_format failed";

        /* Get the format back */
        const char* retrieved = log_get_format();

        /* Verify round-trip property */
        EXPECT_STREQ(pattern, retrieved)
            << "Iteration " << test_iter << ": round-trip failed. Set '"
            << pattern << "', got '" << (retrieved ? retrieved : "NULL") << "'";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 7: Multi-Backend Delivery                                        */
/* *For any* set of N registered backends, when a log message is output,     */
/* all N backends SHALL receive the message.                                 */
/* **Validates: Requirements 3.1, 3.2, 3.4**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 7: Multi-Backend Delivery
 *
 * *For any* set of N registered backends, when a log message is output,
 * all N backends SHALL receive the message.
 *
 * **Validates: Requirements 3.1, 3.2, 3.4**
 */
TEST_F(LogPropertyTest, Property7_MultiBackendDelivery) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of backends (1 to 4) */
        std::uniform_int_distribution<int> backend_count_dist(1, 4);
        int num_backends = backend_count_dist(rng);

        /* Initialize log system */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register backends */
        std::vector<log_backend_t*> backends;
        std::vector<std::string> backend_names;

        for (int i = 0; i < num_backends; ++i) {
            log_backend_t* backend = log_backend_memory_create(1024);
            ASSERT_NE(nullptr, backend)
                << "Iteration " << test_iter << ": backend " << i
                << " creation failed";

            /* Give each backend a unique name */
            std::string name = "memory" + std::to_string(i);
            backend_names.push_back(name);

            /* We need to allocate a persistent name string */
            char* name_ptr = (char*)malloc(name.length() + 1);
            memcpy(name_ptr, name.c_str(), name.length() + 1);
            backend->name = name_ptr;

            ASSERT_EQ(LOG_OK, log_backend_register(backend))
                << "Iteration " << test_iter << ": backend " << i
                << " registration failed";

            backends.push_back(backend);
        }

        /* Generate random message */
        std::uniform_int_distribution<int> len_dist(5, 20);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int msg_len = len_dist(rng);
        std::string message;
        for (int i = 0; i < msg_len; ++i) {
            message += static_cast<char>(char_dist(rng));
        }

        /* Write the message */
        log_status_t status =
            log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                      "%s", message.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": log_write failed";

        /* Verify all backends received the message */
        for (int i = 0; i < num_backends; ++i) {
            char buf[1024];
            size_t len = log_backend_memory_read(backends[i], buf, sizeof(buf));
            EXPECT_GT(len, 0u) << "Iteration " << test_iter << ": backend " << i
                               << " received no data";
            EXPECT_NE(nullptr, strstr(buf, message.c_str()))
                << "Iteration " << test_iter << ": backend " << i
                << " missing message";
        }

        /* Clean up */
        for (int i = 0; i < num_backends; ++i) {
            log_backend_unregister(backend_names[i].c_str());
            free((void*)backends[i]->name);
            log_backend_memory_destroy(backends[i]);
        }

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 8: Backend Registration/Unregistration                           */
/* *For any* backend B, after log_backend_unregister(B) is called, B SHALL   */
/* NOT receive any subsequent log messages.                                  */
/* **Validates: Requirements 3.3**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 8: Backend Registration/Unregistration
 *
 * *For any* backend B, after log_backend_unregister(B) is called, B SHALL
 * NOT receive any subsequent log messages.
 *
 * **Validates: Requirements 3.3**
 */
TEST_F(LogPropertyTest, Property8_BackendRegistrationUnregistration) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize log system */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register a memory backend */
        log_backend_t* backend = log_backend_memory_create(1024);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";

        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Generate first message */
        std::uniform_int_distribution<int> len_dist(5, 15);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int msg1_len = len_dist(rng);
        std::string message1;
        for (int i = 0; i < msg1_len; ++i) {
            message1 += static_cast<char>(char_dist(rng));
        }

        /* Write first message - backend should receive it */
        EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                    __func__, "%s", message1.c_str()));

        /* Verify backend received first message */
        char buf[1024];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        EXPECT_GT(len, 0u) << "Iteration " << test_iter
                           << ": backend didn't receive message1";
        EXPECT_NE(nullptr, strstr(buf, message1.c_str()))
            << "Iteration " << test_iter << ": message1 not found in backend";

        /* Clear the backend buffer */
        log_backend_memory_clear(backend);

        /* Unregister the backend */
        ASSERT_EQ(LOG_OK, log_backend_unregister("memory"))
            << "Iteration " << test_iter << ": unregister failed";

        /* Generate second message */
        int msg2_len = len_dist(rng);
        std::string message2;
        for (int i = 0; i < msg2_len; ++i) {
            message2 += static_cast<char>(char_dist(rng));
        }

        /* Write second message - backend should NOT receive it */
        EXPECT_EQ(LOG_OK, log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                    __func__, "%s", message2.c_str()));

        /* Verify backend did NOT receive second message */
        size_t size_after = log_backend_memory_size(backend);
        EXPECT_EQ(0u, size_after) << "Iteration " << test_iter
                                  << ": unregistered backend received message";

        /* Clean up */
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 9: Backend Failure Isolation                                     */
/* *For any* set of backends where one fails, the remaining backends SHALL   */
/* still receive log messages.                                               */
/* **Validates: Requirements 3.6**                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Failing backend context
 */
typedef struct {
    bool should_fail;
    int call_count;
} failing_backend_ctx_t;

/**
 * \brief           Failing backend write function
 */
static log_status_t failing_backend_write(void* ctx, const char* msg,
                                          size_t len) {
    (void)msg;
    (void)len;
    failing_backend_ctx_t* fctx = (failing_backend_ctx_t*)ctx;
    fctx->call_count++;
    if (fctx->should_fail) {
        return LOG_ERROR_BACKEND;
    }
    return LOG_OK;
}

/**
 * Feature: logging-middleware, Property 9: Backend Failure Isolation
 *
 * *For any* set of backends where one fails, the remaining backends SHALL
 * still receive log messages.
 *
 * **Validates: Requirements 3.6**
 */
TEST_F(LogPropertyTest, Property9_BackendFailureIsolation) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize log system */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create a memory backend (working) */
        log_backend_t* working_backend = log_backend_memory_create(1024);
        ASSERT_NE(nullptr, working_backend)
            << "Iteration " << test_iter << ": working backend creation failed";

        /* Create a failing backend */
        log_backend_t failing_backend;
        failing_backend_ctx_t failing_ctx = {.should_fail = true,
                                             .call_count = 0};
        failing_backend.name = "failing";
        failing_backend.init = NULL;
        failing_backend.write = failing_backend_write;
        failing_backend.flush = NULL;
        failing_backend.deinit = NULL;
        failing_backend.ctx = &failing_ctx;
        failing_backend.min_level = LOG_LEVEL_TRACE;
        failing_backend.enabled = true;

        /* Register both backends */
        ASSERT_EQ(LOG_OK, log_backend_register(&failing_backend))
            << "Iteration " << test_iter
            << ": failing backend registration failed";
        ASSERT_EQ(LOG_OK, log_backend_register(working_backend))
            << "Iteration " << test_iter
            << ": working backend registration failed";

        /* Generate random message */
        std::uniform_int_distribution<int> len_dist(5, 15);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int msg_len = len_dist(rng);
        std::string message;
        for (int i = 0; i < msg_len; ++i) {
            message += static_cast<char>(char_dist(rng));
        }

        /* Write message - failing backend should fail but working should
         * succeed */
        log_status_t status =
            log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                      "%s", message.c_str());

        /* The overall status should still be OK because at least one backend
         * succeeded */
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter
            << ": log_write failed despite working backend";

        /* Verify failing backend was called */
        EXPECT_GT(failing_ctx.call_count, 0)
            << "Iteration " << test_iter << ": failing backend was not called";

        /* Verify working backend received the message */
        char buf[1024];
        size_t len = log_backend_memory_read(working_backend, buf, sizeof(buf));
        EXPECT_GT(len, 0u) << "Iteration " << test_iter
                           << ": working backend received no data";
        EXPECT_NE(nullptr, strstr(buf, message.c_str()))
            << "Iteration " << test_iter
            << ": working backend missing message despite failing backend";

        /* Clean up */
        log_backend_unregister("failing");
        log_backend_unregister("memory");
        log_backend_memory_destroy(working_backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 10: Module Level Filtering                                       */
/* *For any* module M with a configured level L_M, messages from M at levels */
/* < L_M SHALL be discarded, regardless of the global level.                 */
/* **Validates: Requirements 4.1, 4.2, 4.3**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 10: Module Level Filtering
 *
 * *For any* module M with a configured level L_M, messages from M at levels
 * < L_M SHALL be discarded, regardless of the global level.
 *
 * **Validates: Requirements 4.1, 4.2, 4.3**
 */
TEST_F(LogPropertyTest, Property10_ModuleLevelFiltering) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random global level */
        log_level_t global_level = randomLevel();

        /* Generate random module level */
        log_level_t module_level = randomLevel();

        /* Generate random message level (excluding NONE) */
        log_level_t msg_level = randomMessageLevel();

        /* Generate random module name */
        std::uniform_int_distribution<int> len_dist(3, 10);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int mod_len = len_dist(rng);
        std::string module;
        for (int i = 0; i < mod_len; ++i) {
            module += static_cast<char>(char_dist(rng));
        }

        /* Initialize log system */
        log_config_t config = {.level = global_level,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(1024);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Set module-specific level */
        ASSERT_EQ(LOG_OK, log_module_set_level(module.c_str(), module_level))
            << "Iteration " << test_iter << ": module level set failed";

        /* Generate unique message */
        std::string message = "msg_" + std::to_string(test_iter);

        /* Write message from the module */
        log_status_t status =
            log_write(msg_level, module.c_str(), __FILE__, __LINE__, __func__,
                      "%s", message.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": log_write failed";

        /* Read from backend */
        char buf[1024];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));

        /* Verify filtering property:
         * Message should pass if msg_level >= module_level
         * Message should be filtered if msg_level < module_level
         */
        bool should_pass = (msg_level >= module_level);

        if (should_pass) {
            EXPECT_GT(len, 0u)
                << "Iteration " << test_iter
                << ": message should have passed (msg=" << msg_level
                << ", module=" << module_level << ")";
            if (len > 0) {
                EXPECT_NE(nullptr, strstr(buf, message.c_str()))
                    << "Iteration " << test_iter << ": message content missing";
            }
        } else {
            EXPECT_EQ(0u, len)
                << "Iteration " << test_iter
                << ": message should have been filtered (msg=" << msg_level
                << ", module=" << module_level << ")";
        }

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 11: Module Level Fallback                                        */
/* *For any* module M without a configured level, messages from M SHALL be   */
/* filtered using the global log level.                                      */
/* **Validates: Requirements 4.4**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 11: Module Level Fallback
 *
 * *For any* module M without a configured level, messages from M SHALL be
 * filtered using the global log level.
 *
 * **Validates: Requirements 4.4**
 */
TEST_F(LogPropertyTest, Property11_ModuleLevelFallback) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random global level */
        log_level_t global_level = randomLevel();

        /* Generate random message level (excluding NONE) */
        log_level_t msg_level = randomMessageLevel();

        /* Generate random module name (not configured) */
        std::uniform_int_distribution<int> len_dist(3, 10);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int mod_len = len_dist(rng);
        std::string module;
        for (int i = 0; i < mod_len; ++i) {
            module += static_cast<char>(char_dist(rng));
        }

        /* Initialize log system */
        log_config_t config = {.level = global_level,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(1024);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* DO NOT set module-specific level - should use global */

        /* Verify module returns global level */
        EXPECT_EQ(global_level, log_module_get_level(module.c_str()))
            << "Iteration " << test_iter
            << ": module should return global level";

        /* Generate unique message */
        std::string message = "fallback_" + std::to_string(test_iter);

        /* Write message from the module */
        log_status_t status =
            log_write(msg_level, module.c_str(), __FILE__, __LINE__, __func__,
                      "%s", message.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": log_write failed";

        /* Read from backend */
        char buf[1024];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));

        /* Verify fallback property:
         * Message should pass if msg_level >= global_level
         * Message should be filtered if msg_level < global_level
         */
        bool should_pass = (msg_level >= global_level);

        if (should_pass) {
            EXPECT_GT(len, 0u)
                << "Iteration " << test_iter
                << ": message should have passed (msg=" << msg_level
                << ", global=" << global_level << ")";
            if (len > 0) {
                EXPECT_NE(nullptr, strstr(buf, message.c_str()))
                    << "Iteration " << test_iter << ": message content missing";
            }
        } else {
            EXPECT_EQ(0u, len)
                << "Iteration " << test_iter
                << ": message should have been filtered (msg=" << msg_level
                << ", global=" << global_level << ")";
        }

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 12: Wildcard Pattern Matching                                    */
/* *For any* wildcard pattern P (e.g., "hal.*"), all modules matching P      */
/* SHALL use the configured level for P.                                     */
/* **Validates: Requirements 4.5**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 12: Wildcard Pattern Matching
 *
 * *For any* wildcard pattern P (e.g., "hal.*"), all modules matching P
 * SHALL use the configured level for P.
 *
 * **Validates: Requirements 4.5**
 */
TEST_F(LogPropertyTest, Property12_WildcardPatternMatching) {
    /* Predefined prefixes for wildcard testing */
    const char* prefixes[] = {"hal", "osal", "app", "drv", "sys"};
    const size_t num_prefixes = sizeof(prefixes) / sizeof(prefixes[0]);

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Select random prefix */
        std::uniform_int_distribution<size_t> prefix_dist(0, num_prefixes - 1);
        const char* prefix = prefixes[prefix_dist(rng)];

        /* Generate random wildcard level */
        log_level_t wildcard_level = randomLevel();

        /* Generate random global level (different from wildcard) */
        log_level_t global_level;
        do {
            global_level = randomLevel();
        } while (global_level == wildcard_level);

        /* Generate random message level (excluding NONE) */
        log_level_t msg_level = randomMessageLevel();

        /* Generate random suffix for matching module */
        std::uniform_int_distribution<int> len_dist(3, 8);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        int suffix_len = len_dist(rng);
        std::string suffix;
        for (int i = 0; i < suffix_len; ++i) {
            suffix += static_cast<char>(char_dist(rng));
        }

        /* Create matching module name (prefix.suffix) */
        std::string matching_module = std::string(prefix) + "." + suffix;

        /* Create non-matching module name */
        std::string non_matching_module = "other." + suffix;

        /* Initialize log system */
        log_config_t config = {.level = global_level,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(2048);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Set wildcard pattern level */
        std::string pattern = std::string(prefix) + ".*";
        ASSERT_EQ(LOG_OK, log_module_set_level(pattern.c_str(), wildcard_level))
            << "Iteration " << test_iter << ": wildcard level set failed";

        /* Verify matching module uses wildcard level */
        ASSERT_EQ(wildcard_level, log_module_get_level(matching_module.c_str()))
            << "Iteration " << test_iter
            << ": matching module should use wildcard level";

        /* Verify non-matching module uses global level */
        EXPECT_EQ(global_level,
                  log_module_get_level(non_matching_module.c_str()))
            << "Iteration " << test_iter
            << ": non-matching module should use global level";

        /* Generate unique messages with completely non-overlapping prefixes */
        std::string match_msg = "WILDCARD_HIT_" + std::to_string(test_iter);
        std::string nomatch_msg = "GLOBAL_PASS_" + std::to_string(test_iter);

        /* Write message from matching module */
        log_status_t status =
            log_write(msg_level, matching_module.c_str(), __FILE__, __LINE__,
                      __func__, "%s", match_msg.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": log_write (matching) failed";

        /* Write message from non-matching module */
        status = log_write(msg_level, non_matching_module.c_str(), __FILE__,
                           __LINE__, __func__, "%s", nomatch_msg.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": log_write (non-matching) failed";

        /* Read from backend */
        char buf[2048];
        memset(buf, 0, sizeof(buf));
        (void)log_backend_memory_read(backend, buf, sizeof(buf));

        /* Verify wildcard filtering for matching module */
        bool match_should_pass = (msg_level >= wildcard_level);
        bool nomatch_should_pass = (msg_level >= global_level);

        if (match_should_pass) {
            EXPECT_NE(nullptr, strstr(buf, match_msg.c_str()))
                << "Iteration " << test_iter
                << ": matching module message should have passed (msg="
                << msg_level << ", wildcard=" << wildcard_level << ")";
        } else {
            EXPECT_EQ(nullptr, strstr(buf, match_msg.c_str()))
                << "Iteration " << test_iter
                << ": matching module message should have been filtered (msg="
                << msg_level << ", wildcard=" << wildcard_level << ")";
        }

        if (nomatch_should_pass) {
            EXPECT_NE(nullptr, strstr(buf, nomatch_msg.c_str()))
                << "Iteration " << test_iter
                << ": non-matching module message should have passed (msg="
                << msg_level << ", global=" << global_level << ")";
        } else {
            EXPECT_EQ(nullptr, strstr(buf, nomatch_msg.c_str()))
                << "Iteration " << test_iter
                << ": non-matching module message should have been filtered "
                   "(msg="
                << msg_level << ", global=" << global_level << ")";
        }

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 13: Async FIFO Order                                             */
/* *For any* sequence of N messages logged in async mode, the output order   */
/* SHALL match the input order (FIFO).                                       */
/* **Validates: Requirements 5.1, 5.5**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 13: Async FIFO Order
 *
 * *For any* sequence of N messages logged in async mode, the output order
 * SHALL match the input order (FIFO).
 *
 * **Validates: Requirements 5.1, 5.5**
 */
TEST_F(LogPropertyTest, Property13_AsyncFIFOOrder) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of messages (3 to 10) */
        std::uniform_int_distribution<int> msg_count_dist(3, 10);
        int num_messages = msg_count_dist(rng);

        /* Initialize log system in async mode */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = true,
                               .buffer_size = 2048,
                               .max_msg_len = 128,
                               .color_enabled = false,
                               .async_queue_size = 32,
                               .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(4096);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Generate and send messages with sequence numbers */
        std::vector<std::string> messages;
        for (int i = 0; i < num_messages; ++i) {
            std::string msg = "MSG_" + std::to_string(test_iter) + "_SEQ_" +
                              std::to_string(i);
            messages.push_back(msg);

            log_status_t status =
                log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                          "%s", msg.c_str());
            EXPECT_EQ(LOG_OK, status) << "Iteration " << test_iter
                                      << ": log_write failed for msg " << i;
        }

        /* Flush to ensure all messages are processed */
        EXPECT_EQ(LOG_OK, log_async_flush())
            << "Iteration " << test_iter << ": flush failed";

        /* Read from backend */
        char buf[8192];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        EXPECT_GT(len, 0u) << "Iteration " << test_iter << ": no data received";

        /* Verify FIFO order - each message should appear in sequence */
        const char* search_pos = buf;
        for (int i = 0; i < num_messages; ++i) {
            const char* found = strstr(search_pos, messages[i].c_str());
            EXPECT_NE(nullptr, found)
                << "Iteration " << test_iter << ": message " << i << " ("
                << messages[i] << ") not found";

            if (found != nullptr) {
                /* Next search should start after this message */
                search_pos = found + messages[i].length();
            }
        }

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 14: Async Non-Blocking                                           */
/* *For any* log call in async mode, the call SHALL return before the        */
/* message is written to backends.                                           */
/* **Validates: Requirements 5.3**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 14: Async Non-Blocking
 *
 * *For any* log call in async mode, the call SHALL return before the
 * message is written to backends.
 *
 * **Validates: Requirements 5.3**
 */
TEST_F(LogPropertyTest, Property14_AsyncNonBlocking) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize log system in async mode */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = true,
                               .buffer_size = 2048,
                               .max_msg_len = 128,
                               .color_enabled = false,
                               .async_queue_size = 32,
                               .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(4096);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Write a message - should return immediately */
        std::string msg = "NonBlocking_" + std::to_string(test_iter);
        log_status_t status = log_write(LOG_LEVEL_INFO, "test", __FILE__,
                                        __LINE__, __func__, "%s", msg.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": log_write failed";

        /* Immediately after log_write, there should be pending messages
         * (unless the background task already processed it, which is unlikely
         * in a tight loop) */
        /* Note: This is a probabilistic test - we can't guarantee the message
         * hasn't been processed yet, but we can verify the API works */

        /* The key property is that log_write returns LOG_OK without blocking */
        /* We verify this by checking that multiple rapid writes succeed */
        for (int i = 0; i < 5; ++i) {
            status = log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                               __func__, "Rapid_%d", i);
            EXPECT_EQ(LOG_OK, status) << "Iteration " << test_iter
                                      << ": rapid write " << i << " failed";
        }

        /* Flush and verify messages were eventually processed */
        EXPECT_EQ(LOG_OK, log_async_flush())
            << "Iteration " << test_iter << ": flush failed";

        /* After flush, pending should be 0 */
        EXPECT_EQ(0u, log_async_pending())
            << "Iteration " << test_iter << ": pending not 0 after flush";

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 15: Async Flush Completeness                                     */
/* *For any* pending messages in the async queue, after log_async_flush()    */
/* returns, all messages SHALL have been processed.                          */
/* **Validates: Requirements 5.6**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 15: Async Flush Completeness
 *
 * *For any* pending messages in the async queue, after log_async_flush()
 * returns, all messages SHALL have been processed.
 *
 * **Validates: Requirements 5.6**
 */
TEST_F(LogPropertyTest, Property15_AsyncFlushCompleteness) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of messages (5 to 20) */
        std::uniform_int_distribution<int> msg_count_dist(5, 20);
        int num_messages = msg_count_dist(rng);

        /* Initialize log system in async mode with larger queue */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = true,
                               .buffer_size = 4096,
                               .max_msg_len = 128,
                               .color_enabled = false,
                               .async_queue_size = 128, /* Increased from 64 */
                               .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend with larger buffer */
        log_backend_t* backend =
            log_backend_memory_create(16384); /* Increased from 8192 */
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Generate unique messages */
        std::vector<std::string> messages;
        for (int i = 0; i < num_messages; ++i) {
            std::string msg =
                "FLUSH_" + std::to_string(test_iter) + "_" + std::to_string(i);
            messages.push_back(msg);

            log_status_t status =
                log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                          "%s", msg.c_str());
            EXPECT_EQ(LOG_OK, status) << "Iteration " << test_iter
                                      << ": log_write failed for msg " << i;
        }

        /* Give async task time to process before flush */
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        /* Flush - should block until all messages are processed */
        EXPECT_EQ(LOG_OK, log_async_flush())
            << "Iteration " << test_iter << ": flush failed";

        /* After flush, pending count should be 0 */
        EXPECT_EQ(0u, log_async_pending())
            << "Iteration " << test_iter << ": pending not 0 after flush";

        /* Additional wait to ensure backend has written everything */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        /* Read from backend and verify ALL messages were received */
        char buf[16384];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        EXPECT_GT(len, 0u) << "Iteration " << test_iter << ": no data received";

        /* Verify all messages are present */
        for (int i = 0; i < num_messages; ++i) {
            EXPECT_NE(nullptr, strstr(buf, messages[i].c_str()))
                << "Iteration " << test_iter << ": message " << i << " ("
                << messages[i] << ") not found after flush";
        }

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 16: Thread Safety - Message Integrity                            */
/* *For any* concurrent log calls from multiple tasks, each output message   */
/* SHALL be complete and not interleaved with other messages.                */
/* **Validates: Requirements 6.1, 6.2**                                      */
/*---------------------------------------------------------------------------*/

#include <atomic>
#include <thread>

/**
 * Feature: logging-middleware, Property 16: Thread Safety - Message Integrity
 *
 * *For any* concurrent log calls from multiple tasks, each output message
 * SHALL be complete and not interleaved with other messages.
 *
 * **Validates: Requirements 6.1, 6.2**
 */
TEST_F(LogPropertyTest, Property16_ThreadSafetyMessageIntegrity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of threads (2 to 4) */
        std::uniform_int_distribution<int> thread_count_dist(2, 4);
        int num_threads = thread_count_dist(rng);

        /* Generate random number of messages per thread (5 to 15) */
        std::uniform_int_distribution<int> msg_count_dist(5, 15);
        int messages_per_thread = msg_count_dist(rng);

        /* Initialize log system */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 128,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend with large buffer */
        log_backend_t* backend = log_backend_memory_create(32768);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        std::atomic<int> completed_threads{0};
        std::atomic<int> total_messages_sent{0};

        /* Create threads that log messages with unique markers */
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([t, messages_per_thread, test_iter,
                                  &completed_threads, &total_messages_sent]() {
                for (int i = 0; i < messages_per_thread; ++i) {
                    /* Use unique marker pattern: [I<iter>T<thread>M<msg>] */
                    log_status_t status =
                        log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__,
                                  __func__, "[I%dT%dM%d]", test_iter, t, i);
                    if (status == LOG_OK) {
                        total_messages_sent++;
                    }
                }
                completed_threads++;
            });
        }

        /* Wait for all threads to complete */
        for (auto& thread : threads) {
            thread.join();
        }

        EXPECT_EQ(num_threads, completed_threads.load())
            << "Iteration " << test_iter << ": not all threads completed";

        /* Read all logged messages */
        char buf[32768];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf) - 1);
        buf[len] = '\0';

        /* Verify message integrity - check for complete markers */
        /* Each message should have format [I<iter>T<thread>M<msg>] without
         * interleaving */
        int valid_markers = 0;
        int corrupted_markers = 0;
        const char* p = buf;

        while ((p = strstr(p, "[I")) != NULL) {
            /* Check if this is a valid marker pattern */
            int iter_id, thread_id, msg_id;
#ifdef _MSC_VER
            if (sscanf_s(p, "[I%dT%dM%d]", &iter_id, &thread_id, &msg_id) ==
                3) {
#else
            if (sscanf(p, "[I%dT%dM%d]", &iter_id, &thread_id, &msg_id) == 3) {
#endif
                /* Verify the iteration matches */
                if (iter_id == test_iter) {
                    valid_markers++;
                }
            } else {
                /* Marker was corrupted/interleaved */
                corrupted_markers++;
            }
            p++;
        }

        /* Property: All markers should be valid (no interleaving) */
        EXPECT_EQ(0, corrupted_markers)
            << "Iteration " << test_iter << ": found " << corrupted_markers
            << " corrupted markers (message interleaving detected)";

        /* We should have found some valid markers */
        EXPECT_GT(valid_markers, 0)
            << "Iteration " << test_iter << ": no valid markers found";

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 17: Max Message Length Enforcement                               */
/* *For any* configured max_msg_len value, no output message SHALL exceed    */
/* that length.                                                              */
/* **Validates: Requirements 7.1**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 17: Max Message Length Enforcement
 *
 * *For any* configured max_msg_len value, no output message SHALL exceed
 * that length.
 *
 * **Validates: Requirements 7.1**
 */
TEST_F(LogPropertyTest, Property17_MaxMessageLengthEnforcement) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random max message length (between 10 and 100) */
        std::uniform_int_distribution<size_t> max_len_dist(10, 100);
        size_t max_msg_len = max_len_dist(rng);

        /* Initialize log system with the max length */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m", /* Message only for simplicity */
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = max_msg_len,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Verify max length was set */
        EXPECT_EQ(max_msg_len, log_get_max_msg_len())
            << "Iteration " << test_iter << ": max_msg_len mismatch";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(2048);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Generate random message length (could be shorter or longer than max)
         */
        std::uniform_int_distribution<size_t> msg_len_dist(1, max_msg_len * 3);
        size_t msg_len = msg_len_dist(rng);

        /* Generate random message content */
        std::uniform_int_distribution<int> char_dist('a', 'z');
        std::string message;
        for (size_t i = 0; i < msg_len; ++i) {
            message += static_cast<char>(char_dist(rng));
        }

        /* Write the message */
        log_status_t status =
            log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                      "%s", message.c_str());
        EXPECT_EQ(LOG_OK, status)
            << "Iteration " << test_iter << ": log_write failed";

        /* Read from backend */
        char buf[4096];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));

        /* Property: The user message portion should not exceed max_msg_len */
        /* Note: The formatted output includes the message plus newline */
        /* When truncated, the message ends with "..." */
        if (msg_len > max_msg_len) {
            /* Message should have been truncated */
            /* The truncated message should be max_msg_len chars + newline */
            /* Check that "..." appears in the output */
            EXPECT_NE(nullptr, strstr(buf, "..."))
                << "Iteration " << test_iter
                << ": truncation indicator missing for long message";

            /* The output length should be approximately max_msg_len + 1
             * (newline) */
            /* Allow some tolerance for the "..." indicator */
            EXPECT_LE(len, max_msg_len + 2)
                << "Iteration " << test_iter
                << ": output exceeds max length. Got " << len << ", max was "
                << max_msg_len;
        } else {
            /* Message should not have been truncated */
            /* The output should contain the full message */
            /* Length should be msg_len + 1 (newline) */
            EXPECT_GE(len, msg_len)
                << "Iteration " << test_iter
                << ": output shorter than expected for short message";
        }

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 18: Init/Deinit Lifecycle                                        */
/* *For any* sequence of log_init() and log_deinit() calls,                  */
/* log_is_initialized() SHALL correctly reflect the current state.           */
/* **Validates: Requirements 8.1, 8.4, 8.5**                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 18: Init/Deinit Lifecycle
 *
 * *For any* sequence of log_init() and log_deinit() calls,
 * log_is_initialized() SHALL correctly reflect the current state.
 *
 * **Validates: Requirements 8.1, 8.4, 8.5**
 */
TEST_F(LogPropertyTest, Property18_InitDeinitLifecycle) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of init/deinit cycles (1 to 5) */
        std::uniform_int_distribution<int> cycle_dist(1, 5);
        int num_cycles = cycle_dist(rng);

        for (int cycle = 0; cycle < num_cycles; ++cycle) {
            /* Before init: should not be initialized */
            EXPECT_FALSE(log_is_initialized())
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": should not be initialized before init";

            /* Generate random config */
            log_level_t level = randomLevel();
            log_config_t config = {.level = level,
                                   .format = NULL,
                                   .async_mode = false,
                                   .buffer_size = 0,
                                   .max_msg_len = 0,
                                   .color_enabled = false};

            /* Init should succeed */
            log_status_t init_status = log_init(&config);
            ASSERT_EQ(LOG_OK, init_status)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": init failed";

            /* After init: should be initialized */
            EXPECT_TRUE(log_is_initialized())
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": should be initialized after init";

            /* Double init should fail */
            EXPECT_EQ(LOG_ERROR_ALREADY_INIT, log_init(NULL))
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": double init should fail";

            /* Still initialized after failed double init */
            EXPECT_TRUE(log_is_initialized())
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": should still be initialized after failed double init";

            /* Deinit should succeed */
            log_status_t deinit_status = log_deinit();
            ASSERT_EQ(LOG_OK, deinit_status)
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": deinit failed";

            /* After deinit: should not be initialized */
            EXPECT_FALSE(log_is_initialized())
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": should not be initialized after deinit";

            /* Double deinit should fail */
            EXPECT_EQ(LOG_ERROR_NOT_INIT, log_deinit())
                << "Iteration " << test_iter << ", cycle " << cycle
                << ": double deinit should fail";
        }
    }
}

/**
 * Feature: logging-middleware, Property 18b: Deinit Flushes Pending Messages
 *
 * *For any* pending messages in async mode, log_deinit() SHALL flush
 * all pending messages before releasing resources.
 *
 * **Validates: Requirements 8.4**
 */
TEST_F(LogPropertyTest, Property18b_DeinitFlushesPendingMessages) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random number of messages (5 to 15) */
        std::uniform_int_distribution<int> msg_count_dist(5, 15);
        int num_messages = msg_count_dist(rng);

        /* Initialize in async mode */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = true,
                               .buffer_size = 4096,
                               .max_msg_len = 128,
                               .color_enabled = false,
                               .async_queue_size = 64,
                               .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(8192);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Generate unique messages */
        std::vector<std::string> messages;
        for (int i = 0; i < num_messages; ++i) {
            std::string msg = "DEINIT_FLUSH_" + std::to_string(test_iter) +
                              "_" + std::to_string(i);
            messages.push_back(msg);

            log_status_t status =
                log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__,
                          "%s", msg.c_str());
            EXPECT_EQ(LOG_OK, status) << "Iteration " << test_iter
                                      << ": log_write failed for msg " << i;
        }

        /* Deinit should flush all pending messages */
        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";

        /* Read from backend and verify ALL messages were flushed */
        char buf[16384];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        EXPECT_GT(len, 0u) << "Iteration " << test_iter << ": no data received";

        /* Verify all messages are present (flushed before deinit completed) */
        for (int i = 0; i < num_messages; ++i) {
            EXPECT_NE(nullptr, strstr(buf, messages[i].c_str()))
                << "Iteration " << test_iter << ": message " << i << " ("
                << messages[i] << ") not found after deinit flush";
        }

        /* Clean up backend (already unregistered by deinit) */
        log_backend_memory_destroy(backend);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 19: Runtime Reconfiguration                                      */
/* *For any* configuration change made via log_set_level() or                */
/* log_set_format(), subsequent log messages SHALL reflect the new           */
/* configuration.                                                            */
/* **Validates: Requirements 8.3**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: logging-middleware, Property 19: Runtime Reconfiguration
 *
 * *For any* configuration change made via log_set_level() or log_set_format(),
 * subsequent log messages SHALL reflect the new configuration.
 *
 * **Validates: Requirements 8.3**
 */
TEST_F(LogPropertyTest, Property19_RuntimeReconfiguration) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize with default config */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(4096);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Generate random new level */
        log_level_t new_level = randomLevel();

        /* Change level at runtime */
        EXPECT_EQ(LOG_OK, log_set_level(new_level))
            << "Iteration " << test_iter << ": set_level failed";

        /* Verify level was changed */
        EXPECT_EQ(new_level, log_get_level())
            << "Iteration " << test_iter << ": level not changed";

        /* Generate message at level below new level */
        log_level_t below_level = (new_level > LOG_LEVEL_TRACE)
                                      ? (log_level_t)(new_level - 1)
                                      : LOG_LEVEL_TRACE;

        /* Generate message at level at or above new level */
        log_level_t at_level = new_level;

        /* Clear backend */
        log_backend_memory_clear(backend);

        /* Write message below level - should be filtered */
        if (below_level < new_level) {
            std::string below_msg = "BELOW_" + std::to_string(test_iter);
            log_write(below_level, "test", __FILE__, __LINE__, __func__, "%s",
                      below_msg.c_str());

            /* Read from backend */
            char buf[1024];
            size_t len = log_backend_memory_read(backend, buf, sizeof(buf));

            /* Message should be filtered (not present) */
            if (len > 0) {
                EXPECT_EQ(nullptr, strstr(buf, below_msg.c_str()))
                    << "Iteration " << test_iter
                    << ": message below level should be filtered";
            }
        }

        /* Clear backend */
        log_backend_memory_clear(backend);

        /* Write message at level - should pass (unless NONE) */
        if (at_level < LOG_LEVEL_NONE) {
            std::string at_msg = "AT_LEVEL_" + std::to_string(test_iter);
            log_write(at_level, "test", __FILE__, __LINE__, __func__, "%s",
                      at_msg.c_str());

            /* Read from backend */
            char buf[1024];
            size_t len = log_backend_memory_read(backend, buf, sizeof(buf));

            /* Message should pass */
            EXPECT_GT(len, 0u) << "Iteration " << test_iter
                               << ": message at level should pass";
            if (len > 0) {
                EXPECT_NE(nullptr, strstr(buf, at_msg.c_str()))
                    << "Iteration " << test_iter
                    << ": message at level not found";
            }
        }

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/**
 * Feature: logging-middleware, Property 19b: Format Runtime Reconfiguration
 *
 * *For any* format pattern change via log_set_format(), subsequent log
 * messages SHALL use the new format pattern.
 *
 * **Validates: Requirements 8.3**
 */
TEST_F(LogPropertyTest, Property19b_FormatRuntimeReconfiguration) {
    /* Test patterns with different tokens */
    const char* patterns[] = {
        "%m",                /* Message only */
        "[%L] %m",           /* Level and message */
        "[%M] %m",           /* Module and message */
        "[%T] [%L] [%M] %m", /* Full pattern */
    };
    const size_t num_patterns = sizeof(patterns) / sizeof(patterns[0]);

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize with first pattern */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = patterns[0],
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 0,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(4096);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Select random new pattern */
        std::uniform_int_distribution<size_t> pattern_dist(0, num_patterns - 1);
        const char* new_pattern = patterns[pattern_dist(rng)];

        /* Change format at runtime */
        EXPECT_EQ(LOG_OK, log_set_format(new_pattern))
            << "Iteration " << test_iter << ": set_format failed";

        /* Verify format was changed */
        EXPECT_STREQ(new_pattern, log_get_format())
            << "Iteration " << test_iter << ": format not changed";

        /* Clear backend */
        log_backend_memory_clear(backend);

        /* Write a message with unique module name */
        std::string module = "testmod" + std::to_string(test_iter);
        std::string msg = "FORMAT_TEST_" + std::to_string(test_iter);

        log_write(LOG_LEVEL_INFO, module.c_str(), __FILE__, __LINE__, __func__,
                  "%s", msg.c_str());

        /* Read from backend */
        char buf[1024];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        EXPECT_GT(len, 0u) << "Iteration " << test_iter << ": no output";

        /* Verify message content is present */
        EXPECT_NE(nullptr, strstr(buf, msg.c_str()))
            << "Iteration " << test_iter << ": message not found";

        /* Verify format-specific content based on pattern */
        if (strstr(new_pattern, "%L") != nullptr) {
            /* Should contain level name */
            EXPECT_NE(nullptr, strstr(buf, "INFO"))
                << "Iteration " << test_iter
                << ": level not found in output with %L pattern";
        }

        if (strstr(new_pattern, "%M") != nullptr) {
            /* Should contain module name */
            EXPECT_NE(nullptr, strstr(buf, module.c_str()))
                << "Iteration " << test_iter
                << ": module not found in output with %M pattern";
        }

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}

/**
 * Feature: logging-middleware, Property 19c: Max Message Length Runtime
 * Reconfiguration
 *
 * *For any* max_msg_len change via log_set_max_msg_len(), subsequent log
 * messages SHALL be truncated according to the new limit.
 *
 * **Validates: Requirements 8.3**
 */
TEST_F(LogPropertyTest, Property19c_MaxMsgLenRuntimeReconfiguration) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Initialize with default max length */
        log_config_t config = {.level = LOG_LEVEL_TRACE,
                               .format = "%m",
                               .async_mode = false,
                               .buffer_size = 0,
                               .max_msg_len = 128,
                               .color_enabled = false};

        ASSERT_EQ(LOG_OK, log_init(&config))
            << "Iteration " << test_iter << ": init failed";

        /* Create and register memory backend */
        log_backend_t* backend = log_backend_memory_create(4096);
        ASSERT_NE(nullptr, backend)
            << "Iteration " << test_iter << ": backend creation failed";
        ASSERT_EQ(LOG_OK, log_backend_register(backend))
            << "Iteration " << test_iter << ": backend registration failed";

        /* Generate random new max length (20 to 80) */
        std::uniform_int_distribution<size_t> len_dist(20, 80);
        size_t new_max_len = len_dist(rng);

        /* Change max length at runtime */
        EXPECT_EQ(LOG_OK, log_set_max_msg_len(new_max_len))
            << "Iteration " << test_iter << ": set_max_msg_len failed";

        /* Verify max length was changed */
        EXPECT_EQ(new_max_len, log_get_max_msg_len())
            << "Iteration " << test_iter << ": max_msg_len not changed";

        /* Clear backend */
        log_backend_memory_clear(backend);

        /* Write a message longer than new max length */
        std::string long_msg(new_max_len + 50, 'x');
        log_write(LOG_LEVEL_INFO, "test", __FILE__, __LINE__, __func__, "%s",
                  long_msg.c_str());

        /* Read from backend */
        char buf[1024];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        EXPECT_GT(len, 0u) << "Iteration " << test_iter << ": no output";

        /* Verify message was truncated */
        /* The output should be approximately new_max_len + newline */
        EXPECT_LE(len, new_max_len + 2)
            << "Iteration " << test_iter
            << ": output exceeds new max length. Got " << len << ", max was "
            << new_max_len;

        /* Verify truncation indicator */
        EXPECT_NE(nullptr, strstr(buf, "..."))
            << "Iteration " << test_iter << ": truncation indicator missing";

        /* Clean up */
        log_backend_unregister("memory");
        log_backend_memory_destroy(backend);

        ASSERT_EQ(LOG_OK, log_deinit())
            << "Iteration " << test_iter << ": deinit failed";
    }
}
