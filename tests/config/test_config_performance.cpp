/**
 * \file            test_config_performance.cpp
 * \brief           Config Manager Performance Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Performance and benchmark tests for Config Manager.
 */

#include <chrono>
#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "config/config.h"
}

/**
 * \brief           Config Performance Test Fixture
 */
class ConfigPerformanceTest : public ::testing::Test {
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

    /* Helper to measure execution time */
    template <typename Func> double measure_time_ms(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        return elapsed.count();
    }
};

/*---------------------------------------------------------------------------*/
/* Set Operation Benchmarks                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigPerformanceTest, BenchmarkSetI32Operations) {
    const int iterations = 1000;

    double elapsed_ms = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            char key[32];
            snprintf(key, sizeof(key), "bench.key%d", i % 100);
            config_set_i32(key, i);
        }
    });

    double ops_per_sec = (iterations / elapsed_ms) * 1000.0;

    std::cout << "Set I32 Operations:\n";
    std::cout << "  Iterations: " << iterations << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
    std::cout << "  Throughput: " << ops_per_sec << " ops/sec\n";

    /* Performance requirement: > 10,000 ops/sec */
    EXPECT_GT(ops_per_sec, 10000.0)
        << "Set operations too slow: " << ops_per_sec << " ops/sec";
}

TEST_F(ConfigPerformanceTest, BenchmarkSetStrOperations) {
    const int iterations = 1000;

    double elapsed_ms = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            char key[32];
            snprintf(key, sizeof(key), "bench.str%d", i % 100);
            config_set_str(key, "TestValue");
        }
    });

    double ops_per_sec = (iterations / elapsed_ms) * 1000.0;

    std::cout << "Set String Operations:\n";
    std::cout << "  Iterations: " << iterations << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
    std::cout << "  Throughput: " << ops_per_sec << " ops/sec\n";

    EXPECT_GT(ops_per_sec, 8000.0)
        << "String set operations too slow: " << ops_per_sec << " ops/sec";
}

/*---------------------------------------------------------------------------*/
/* Get Operation Benchmarks                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigPerformanceTest, BenchmarkGetI32Operations) {
    const int iterations = 10000;

    /* Pre-populate keys */
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "bench.key%d", i);
        config_set_i32(key, i);
    }

    double elapsed_ms = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            char key[32];
            snprintf(key, sizeof(key), "bench.key%d", i % 100);
            int32_t value;
            config_get_i32(key, &value, 0);
        }
    });

    double ops_per_sec = (iterations / elapsed_ms) * 1000.0;

    std::cout << "Get I32 Operations:\n";
    std::cout << "  Iterations: " << iterations << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
    std::cout << "  Throughput: " << ops_per_sec << " ops/sec\n";

    /* Performance requirement: > 40,000 ops/sec */
    EXPECT_GT(ops_per_sec, 40000.0)
        << "Get operations too slow: " << ops_per_sec << " ops/sec";
}

TEST_F(ConfigPerformanceTest, BenchmarkGetStrOperations) {
    const int iterations = 10000;

    /* Pre-populate keys */
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "bench.str%d", i);
        config_set_str(key, "TestValue");
    }

    double elapsed_ms = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            char key[32];
            snprintf(key, sizeof(key), "bench.str%d", i % 100);
            char buffer[64];
            config_get_str(key, buffer, sizeof(buffer));
        }
    });

    double ops_per_sec = (iterations / elapsed_ms) * 1000.0;

    std::cout << "Get String Operations:\n";
    std::cout << "  Iterations: " << iterations << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
    std::cout << "  Throughput: " << ops_per_sec << " ops/sec\n";

    EXPECT_GT(ops_per_sec, 30000.0)
        << "String get operations too slow: " << ops_per_sec << " ops/sec";
}

/*---------------------------------------------------------------------------*/
/* Commit Benchmark                                                          */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigPerformanceTest, BenchmarkCommit) {
    /* Set backend */
    ASSERT_EQ(CONFIG_OK, config_set_backend(config_backend_ram_get()));

    /* Set 50 keys */
    for (int i = 0; i < 50; i++) {
        char key[32];
        snprintf(key, sizeof(key), "bench.key%d", i);
        config_set_i32(key, i);
    }

    double elapsed_ms = measure_time_ms([&]() { config_commit(); });

    std::cout << "Commit 50 keys:\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";

    /* Performance requirement: < 50ms for 50 keys */
    EXPECT_LT(elapsed_ms, 50.0) << "Commit too slow: " << elapsed_ms << " ms";
}

/*---------------------------------------------------------------------------*/
/* Memory Usage Tests                                                        */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigPerformanceTest, MemoryUsageDefault) {
    /* This test documents memory usage with default configuration */
    ASSERT_EQ(CONFIG_OK, config_deinit());

    config_manager_config_t config = {.max_keys = 64,
                                      .max_key_len = 32,
                                      .max_value_size = 256,
                                      .max_namespaces = 8,
                                      .max_callbacks = 16,
                                      .auto_commit = false};

    ASSERT_EQ(CONFIG_OK, config_init(&config));

    /* Calculate expected memory usage */
    size_t config_entry_size = 32 + 256 + 8; /* key + value + metadata */
    size_t total_config_memory = 64 * config_entry_size;
    size_t namespace_memory = 8 * 20; /* Approximate */
    size_t callback_memory = 16 * 40; /* Approximate */
    size_t estimated_total =
        total_config_memory + namespace_memory + callback_memory;

    std::cout << "Memory Usage (Default Config):\n";
    std::cout << "  Config entries: " << total_config_memory << " bytes\n";
    std::cout << "  Namespaces: " << namespace_memory << " bytes\n";
    std::cout << "  Callbacks: " << callback_memory << " bytes\n";
    std::cout << "  Estimated total: " << estimated_total << " bytes (~"
              << (estimated_total / 1024) << " KB)\n";

    /* Fill with data */
    for (int i = 0; i < 50; i++) {
        char key[32];
        snprintf(key, sizeof(key), "mem.key%d", i);
        config_set_i32(key, i);
    }

    size_t count;
    config_get_count(&count);
    std::cout << "  Keys stored: " << count << "\n";
    std::cout << "  Per-key overhead: ~"
              << (total_config_memory / config.max_keys) << " bytes\n";
}

TEST_F(ConfigPerformanceTest, MemoryUsageMinimal) {
    /* Test with minimal configuration */
    ASSERT_EQ(CONFIG_OK, config_deinit());

    config_manager_config_t config = {.max_keys = 32,
                                      .max_key_len = 16,
                                      .max_value_size = 64,
                                      .max_namespaces = 4,
                                      .max_callbacks = 4,
                                      .auto_commit = false};

    ASSERT_EQ(CONFIG_OK, config_init(&config));

    size_t config_entry_size = 16 + 64 + 8;
    size_t total_config_memory = 32 * config_entry_size;

    std::cout << "Memory Usage (Minimal Config):\n";
    std::cout << "  Config entries: " << total_config_memory << " bytes\n";
    std::cout << "  Estimated total: ~" << (total_config_memory / 1024)
              << " KB\n";
}

/*---------------------------------------------------------------------------*/
/* Stress Tests                                                              */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigPerformanceTest, StressTestRapidUpdates) {
    const int iterations = 10000;

    /* Rapidly update same key */
    double elapsed_ms = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            config_set_i32("stress.value", i);
        }
    });

    /* Verify final value */
    int32_t value;
    ASSERT_EQ(CONFIG_OK, config_get_i32("stress.value", &value, 0));
    EXPECT_EQ(iterations - 1, value);

    std::cout << "Stress Test - Rapid Updates:\n";
    std::cout << "  Iterations: " << iterations << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
    std::cout << "  Throughput: " << ((iterations / elapsed_ms) * 1000.0)
              << " ops/sec\n";
}

TEST_F(ConfigPerformanceTest, StressTestManyKeys) {
    ASSERT_EQ(CONFIG_OK, config_deinit());

    /* Initialize with maximum keys */
    config_manager_config_t config = {.max_keys = 256,
                                      .max_key_len = 32,
                                      .max_value_size = 256,
                                      .max_namespaces = 8,
                                      .max_callbacks = 16,
                                      .auto_commit = false};

    ASSERT_EQ(CONFIG_OK, config_init(&config));

    /* Create many keys */
    double elapsed_ms = measure_time_ms([&]() {
        for (int i = 0; i < 256; i++) {
            char key[32];
            snprintf(key, sizeof(key), "stress.key%d", i);
            ASSERT_EQ(CONFIG_OK, config_set_i32(key, i));
        }
    });

    std::cout << "Stress Test - Many Keys:\n";
    std::cout << "  Keys created: 256\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";

    /* Verify all keys */
    for (int i = 0; i < 256; i++) {
        char key[32];
        snprintf(key, sizeof(key), "stress.key%d", i);
        int32_t value;
        ASSERT_EQ(CONFIG_OK, config_get_i32(key, &value, -1));
        EXPECT_EQ(i, value);
    }

    size_t count;
    config_get_count(&count);
    EXPECT_EQ(256u, count);
}

TEST_F(ConfigPerformanceTest, StressTestMixedOperations) {
    const int iterations = 1000;

    double elapsed_ms = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            char key[32];
            snprintf(key, sizeof(key), "mixed.key%d", i % 50);

            /* Mix of operations */
            config_set_i32(key, i);

            int32_t value;
            config_get_i32(key, &value, 0);

            if (i % 10 == 0) {
                bool exists;
                config_exists(key, &exists);
            }

            if (i % 20 == 0) {
                config_type_t type;
                config_get_type(key, &type);
            }
        }
    });

    std::cout << "Stress Test - Mixed Operations:\n";
    std::cout << "  Iterations: " << iterations << "\n";
    std::cout << "  Time: " << elapsed_ms << " ms\n";
    std::cout << "  Throughput: " << ((iterations / elapsed_ms) * 1000.0)
              << " ops/sec\n";
}

/*---------------------------------------------------------------------------*/
/* Callback Performance Tests                                                */
/*---------------------------------------------------------------------------*/

static int g_callback_counter = 0;

static void perf_callback(const char* key, config_type_t type,
                          const void* old_value, const void* new_value,
                          void* user_data) {
    (void)key;
    (void)type;
    (void)old_value;
    (void)new_value;
    (void)user_data;
    g_callback_counter++;
}

TEST_F(ConfigPerformanceTest, CallbackOverhead) {
    const int iterations = 1000;

    /* Measure without callback */
    double elapsed_no_cb = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            config_set_i32("perf.nocb", i);
        }
    });

    /* Register callback */
    config_cb_handle_t cb_handle;
    ASSERT_EQ(CONFIG_OK, config_register_callback("perf.withcb", perf_callback,
                                                  NULL, &cb_handle));

    g_callback_counter = 0;

    /* Measure with callback */
    double elapsed_with_cb = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            config_set_i32("perf.withcb", i);
        }
    });

    EXPECT_EQ(iterations, g_callback_counter);

    double overhead_ms = elapsed_with_cb - elapsed_no_cb;
    double overhead_per_call = overhead_ms / iterations;

    std::cout << "Callback Overhead:\n";
    std::cout << "  Without callback: " << elapsed_no_cb << " ms\n";
    std::cout << "  With callback: " << elapsed_with_cb << " ms\n";
    std::cout << "  Overhead: " << overhead_ms << " ms\n";
    std::cout << "  Per-call overhead: " << overhead_per_call << " ms\n";

    /* Callback overhead should be < 0.01ms per call */
    EXPECT_LT(overhead_per_call, 0.01)
        << "Callback overhead too high: " << overhead_per_call << " ms";

    ASSERT_EQ(CONFIG_OK, config_unregister_callback(cb_handle));
}

/*---------------------------------------------------------------------------*/
/* Namespace Performance Tests                                               */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigPerformanceTest, NamespaceOperationOverhead) {
    const int iterations = 1000;

    /* Measure direct operations */
    double elapsed_direct = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            char key[32];
            snprintf(key, sizeof(key), "direct.key%d", i % 100);
            config_set_i32(key, i);
        }
    });

    /* Measure namespace operations */
    config_ns_handle_t ns;
    ASSERT_EQ(CONFIG_OK, config_open_namespace("perf_ns", &ns));

    double elapsed_ns = measure_time_ms([&]() {
        for (int i = 0; i < iterations; i++) {
            char key[32];
            snprintf(key, sizeof(key), "key%d", i % 100);
            config_ns_set_i32(ns, key, i);
        }
    });

    ASSERT_EQ(CONFIG_OK, config_close_namespace(ns));

    double overhead_ms = elapsed_ns - elapsed_direct;
    double overhead_per_call = overhead_ms / iterations;

    std::cout << "Namespace Operation Overhead:\n";
    std::cout << "  Direct operations: " << elapsed_direct << " ms\n";
    std::cout << "  Namespace operations: " << elapsed_ns << " ms\n";
    std::cout << "  Overhead: " << overhead_ms << " ms\n";
    std::cout << "  Per-call overhead: " << overhead_per_call << " ms\n";

    /* Namespace overhead should be minimal */
    EXPECT_LT(overhead_per_call, 0.005)
        << "Namespace overhead too high: " << overhead_per_call << " ms";
}

/*---------------------------------------------------------------------------*/
/* Search Performance Tests                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(ConfigPerformanceTest, SearchPerformanceLinear) {
    /* Test search performance with increasing number of keys */
    const int key_counts[] = {10, 50, 100, 200};

    for (int key_count : key_counts) {
        ASSERT_EQ(CONFIG_OK, config_deinit());
        ASSERT_EQ(CONFIG_OK, config_init(NULL));

        /* Populate keys */
        for (int i = 0; i < key_count; i++) {
            char key[32];
            snprintf(key, sizeof(key), "search.key%d", i);
            config_set_i32(key, i);
        }

        /* Measure search time (worst case - last key) */
        char last_key[32];
        snprintf(last_key, sizeof(last_key), "search.key%d", key_count - 1);

        const int iterations = 1000;
        double elapsed_ms = measure_time_ms([&]() {
            for (int i = 0; i < iterations; i++) {
                int32_t value;
                config_get_i32(last_key, &value, 0);
            }
        });

        double avg_search_time_us = (elapsed_ms / iterations) * 1000.0;

        std::cout << "Search Performance (" << key_count << " keys):\n";
        std::cout << "  Average search time: " << avg_search_time_us << " μs\n";

        /* Search should be fast even with many keys */
        EXPECT_LT(avg_search_time_us, 10.0)
            << "Search too slow with " << key_count << " keys";
    }
}
