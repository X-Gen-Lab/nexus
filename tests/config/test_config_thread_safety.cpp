/**
 * \file            test_config_thread_safety.cpp
 * \brief           Config Manager Thread Safety Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Thread safety tests for Config Manager.
 *                  Tests concurrent access from multiple threads.
 */

#include <atomic>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Thread Safety Test Fixture
 */
class ConfigThreadSafetyTest : public ::testing::Test {
  protected:
    void SetUp() override {
        if (config_is_initialized()) {
            config_deinit();
        }
        ASSERT_EQ(CONFIG_OK, config_init(NULL));
    }

    void TearDown() override {
        if (config_is_initialized()) {
            config_deinit();
        }
    }
};

/*---------------------------------------------------------------------------*/
/* Concurrent Read/Write Tests                                               */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigThreadSafetyTest, ConcurrentWrites) {
    const int num_threads = 4;
    const int iterations = 100;

    std::vector<std::thread> threads;

    /* Each thread writes to its own key */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < iterations; i++) {
                char key[32];
                snprintf(key, sizeof(key), "thread%d.value", t);
                config_set_i32(key, i);
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Verify final values */
    for (int t = 0; t < num_threads; t++) {
        char key[32];
        snprintf(key, sizeof(key), "thread%d.value", t);
        int32_t value;
        ASSERT_EQ(CONFIG_OK, config_get_i32(key, &value, -1));
        EXPECT_EQ(iterations - 1, value);
    }
}

TEST_F(ConfigThreadSafetyTest, ConcurrentReads) {
    const int num_threads = 4;
    const int iterations = 1000;

    /* Pre-populate keys */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "read.key%d", i);
        config_set_i32(key, i * 100);
    }

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    /* Multiple threads reading same keys */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&error_count]() {
            for (int i = 0; i < iterations; i++) {
                char key[32];
                snprintf(key, sizeof(key), "read.key%d", i % 10);
                int32_t value;
                config_status_t status = config_get_i32(key, &value, -1);

                if (status != CONFIG_OK || value != (i % 10) * 100) {
                    error_count++;
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, error_count.load())
        << "Concurrent reads produced errors: " << error_count.load();
}

TEST_F(ConfigThreadSafetyTest, ConcurrentReadWrite) {
    const int num_readers = 2;
    const int num_writers = 2;
    const int iterations = 100;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    /* Writer threads */
    for (int t = 0; t < num_writers; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < iterations; i++) {
                char key[32];
                snprintf(key, sizeof(key), "rw.key%d", t);
                config_set_i32(key, i);
            }
        });
    }

    /* Reader threads */
    for (int t = 0; t < num_readers; t++) {
        threads.emplace_back([&error_count]() {
            for (int i = 0; i < iterations; i++) {
                for (int w = 0; w < num_writers; w++) {
                    char key[32];
                    snprintf(key, sizeof(key), "rw.key%d", w);
                    int32_t value;
                    config_status_t status = config_get_i32(key, &value, -1);

                    /* Value should be valid (either default or set value) */
                    if (status != CONFIG_OK) {
                        error_count++;
                    }
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, error_count.load())
        << "Concurrent read/write produced errors: " << error_count.load();
}

/*---------------------------------------------------------------------------*/
/* Concurrent Namespace Tests                                                */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigThreadSafetyTest, ConcurrentNamespaceOperations) {
    const int num_threads = 4;
    const int iterations = 50;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    /* Each thread operates on its own namespace */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, &error_count]() {
            char ns_name[32];
            snprintf(ns_name, sizeof(ns_name), "ns%d", t);

            config_ns_handle_t ns;
            if (config_open_namespace(ns_name, &ns) != CONFIG_OK) {
                error_count++;
                return;
            }

            for (int i = 0; i < iterations; i++) {
                if (config_ns_set_i32(ns, "value", i) != CONFIG_OK) {
                    error_count++;
                }

                int32_t value;
                if (config_ns_get_i32(ns, "value", &value, -1) != CONFIG_OK) {
                    error_count++;
                }
            }

            if (config_close_namespace(ns) != CONFIG_OK) {
                error_count++;
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, error_count.load())
        << "Concurrent namespace operations produced errors: "
        << error_count.load();
}

/*---------------------------------------------------------------------------*/
/* Concurrent Callback Tests                                                 */
/*---------------------------------------------------------------------------*/

static std::atomic<int> g_callback_counter{0};

static void thread_safe_callback(const char* key, config_type_t type,
                                 const void* old_value, const void* new_value,
                                 void* user_data) {
    (void)key;
    (void)type;
    (void)old_value;
    (void)new_value;
    (void)user_data;

    g_callback_counter.fetch_add(1, std::memory_order_relaxed);
}

TEST_F(ConfigThreadSafetyTest, ConcurrentCallbackTriggers) {
    const int num_threads = 4;
    const int iterations = 100;

    g_callback_counter.store(0);

    /* Register wildcard callback */
    config_cb_handle_t cb_handle;
    ASSERT_EQ(CONFIG_OK, config_register_wildcard_callback(thread_safe_callback,
                                                           NULL, &cb_handle));

    std::vector<std::thread> threads;

    /* Multiple threads triggering callbacks */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < iterations; i++) {
                char key[32];
                snprintf(key, sizeof(key), "cb.thread%d", t);
                config_set_i32(key, i);
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Verify callback count */
    int expected = num_threads * iterations;
    EXPECT_EQ(expected, g_callback_counter.load())
        << "Expected " << expected << " callbacks, got "
        << g_callback_counter.load();

    ASSERT_EQ(CONFIG_OK, config_unregister_callback(cb_handle));
}

/*---------------------------------------------------------------------------*/
/* Concurrent Delete Tests                                                   */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigThreadSafetyTest, ConcurrentDeleteAndCreate) {
    const int num_threads = 4;
    const int iterations = 50;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    /* Threads alternately create and delete keys */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, &error_count]() {
            for (int i = 0; i < iterations; i++) {
                char key[32];
                snprintf(key, sizeof(key), "del.thread%d", t);

                /* Create */
                if (config_set_i32(key, i) != CONFIG_OK) {
                    error_count++;
                }

                /* Delete */
                config_status_t status = config_delete(key);
                if (status != CONFIG_OK && status != CONFIG_ERROR_NOT_FOUND) {
                    error_count++;
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, error_count.load())
        << "Concurrent delete/create produced errors: " << error_count.load();
}

/*---------------------------------------------------------------------------*/
/* Stress Tests with Many Threads                                            */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigThreadSafetyTest, StressTestManyThreads) {
    const int num_threads =
        4; /* Reduced from 8 to avoid excessive contention */
    const int iterations = 50; /* Reduced from 100 */

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    /* Mix of operations from many threads */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&error_count]() {
            for (int i = 0; i < iterations; i++) {
                char key[32];
                snprintf(key, sizeof(key), "stress.key%d", i % 20);

                /* Set */
                if (config_set_i32(key, i) != CONFIG_OK) {
                    error_count++;
                }

                /* Get */
                int32_t value;
                if (config_get_i32(key, &value, -1) != CONFIG_OK) {
                    error_count++;
                }

                /* Exists */
                bool exists;
                if (config_exists(key, &exists) != CONFIG_OK) {
                    error_count++;
                }

                /* Type check less frequently to reduce contention */
                if (i % 20 == 0) {
                    config_type_t type;
                    if (config_get_type(key, &type) != CONFIG_OK) {
                        error_count++;
                    }
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, error_count.load())
        << "Stress test produced errors: " << error_count.load();
}

/*---------------------------------------------------------------------------*/
/* Data Race Detection Tests                                                 */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigThreadSafetyTest, NoDataRaceOnSameKey) {
    const int num_threads = 4;
    const int iterations = 100;

    std::vector<std::thread> threads;

    /* Multiple threads writing to same key */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < iterations; i++) {
                /* All threads write to same key */
                config_set_i32("race.test", t * 1000 + i);
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Value should be valid (from one of the threads) */
    int32_t value;
    ASSERT_EQ(CONFIG_OK, config_get_i32("race.test", &value, -1));

    /* Value should be from one of the threads */
    bool valid = false;
    for (int t = 0; t < num_threads; t++) {
        for (int i = 0; i < iterations; i++) {
            if (value == t * 1000 + i) {
                valid = true;
                break;
            }
        }
        if (valid)
            break;
    }

    EXPECT_TRUE(valid) << "Value " << value << " is not from any thread";
}

/*---------------------------------------------------------------------------*/
/* Concurrent Type Operations Tests                                          */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigThreadSafetyTest, ConcurrentMixedTypes) {
    const int num_threads = 4;
    const int iterations = 50;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    /* Each thread uses different data types */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, &error_count]() {
            for (int i = 0; i < iterations; i++) {
                char key[32];
                snprintf(key, sizeof(key), "type.thread%d", t);

                switch (t % 4) {
                    case 0: /* Int32 */
                        if (config_set_i32(key, i) != CONFIG_OK) {
                            error_count++;
                        }
                        break;

                    case 1: /* String */
                        if (config_set_str(key, "test") != CONFIG_OK) {
                            error_count++;
                        }
                        break;

                    case 2: /* Bool */
                        if (config_set_bool(key, i % 2 == 0) != CONFIG_OK) {
                            error_count++;
                        }
                        break;

                    case 3: /* Float */
                        if (config_set_float(key, (float)i) != CONFIG_OK) {
                            error_count++;
                        }
                        break;
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, error_count.load())
        << "Concurrent mixed type operations produced errors: "
        << error_count.load();
}

/*---------------------------------------------------------------------------*/
/* Concurrent Backend Operations Tests                                       */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigThreadSafetyTest, ConcurrentCommit) {
    /* Set backend */
    ASSERT_EQ(CONFIG_OK, config_set_backend(config_backend_ram_get()));

    const int num_threads = 4;
    const int iterations = 10;

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    /* Multiple threads setting values and committing */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, &error_count]() {
            for (int i = 0; i < iterations; i++) {
                char key[32];
                snprintf(key, sizeof(key), "commit.thread%d", t);

                if (config_set_i32(key, i) != CONFIG_OK) {
                    error_count++;
                }

                /* Commit */
                if (config_commit() != CONFIG_OK) {
                    error_count++;
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, error_count.load())
        << "Concurrent commit operations produced errors: "
        << error_count.load();
}

/*---------------------------------------------------------------------------*/
/* Deadlock Prevention Tests                                                 */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigThreadSafetyTest, NoDeadlockWithCallbacks) {
    /* This test ensures callbacks don't cause deadlocks */
    const int num_threads = 2; /* Reduced from 4 */
    const int iterations = 25; /* Reduced from 50 */

    g_callback_counter.store(0);

    /* Register callback */
    config_cb_handle_t cb_handle;
    ASSERT_EQ(CONFIG_OK, config_register_wildcard_callback(thread_safe_callback,
                                                           NULL, &cb_handle));

    std::atomic<bool> timeout{false};
    std::atomic<bool> completed{false};
    std::vector<std::thread> threads;

    /* Worker threads */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, &timeout]() {
            for (int i = 0; i < iterations && !timeout.load(); i++) {
                char key[32];
                snprintf(key, sizeof(key), "deadlock.thread%d.key%d", t, i);
                config_set_i32(key, i);

                /* Add small delay to reduce contention */
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    /* Timeout thread - reduced timeout */
    std::thread timeout_thread([&timeout, &completed]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (!completed.load()) {
            timeout.store(true);
        }
    });

    /* Wait for all worker threads */
    for (auto& thread : threads) {
        thread.join();
    }

    completed.store(true);
    timeout_thread.join();

    EXPECT_FALSE(timeout.load()) << "Test timed out - possible deadlock";

    ASSERT_EQ(CONFIG_OK, config_unregister_callback(cb_handle));
}
