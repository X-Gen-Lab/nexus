/**
 * \file            test_log_performance.cpp
 * \brief           Log Framework Performance Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Performance tests for Log Framework.
 *                  Measures throughput, latency, and resource usage.
 */

#include "test_log_helpers.h"
#include <chrono>
#include <gtest/gtest.h>

extern "C" {
#include "log/log.h"
#include "log/log_backend.h"
}

/*---------------------------------------------------------------------------*/
/* Performance Test Fixture                                                  */
/*---------------------------------------------------------------------------*/

class LogPerformanceTest : public LogTestBase {
  protected:
    static constexpr int PERF_ITERATIONS = 10000;
    static constexpr int WARMUP_ITERATIONS = 100;

    /**
     * \brief       Warmup before performance test
     */
    void Warmup() {
        for (int i = 0; i < WARMUP_ITERATIONS; i++) {
            LOG_INFO("Warmup %d", i);
        }
    }

    /**
     * \brief       Print performance results
     */
    void PrintResults(const char* test_name, int iterations,
                      double elapsed_ms) {
        double throughput = iterations / (elapsed_ms / 1000.0);
        double latency_us = (elapsed_ms * 1000.0) / iterations;

        std::cout << "\n=== " << test_name << " ===" << std::endl;
        std::cout << "Iterations: " << iterations << std::endl;
        std::cout << "Total time: " << elapsed_ms << " ms" << std::endl;
        std::cout << "Throughput: " << throughput << " msg/s" << std::endl;
        std::cout << "Avg latency: " << latency_us << " μs/msg" << std::endl;
    }
};

/*---------------------------------------------------------------------------*/
/* Throughput Tests                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test synchronous mode throughput
 */
TEST_F(LogPerformanceTest, SyncModeThroughput) {
    log_config_t config = LOG_CONFIG_DEFAULT;
    config.async_mode = false;
    config.format = "%m";
    InitLog(&config);

    log_set_level(LOG_LEVEL_TRACE);

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    Warmup();
    log_backend_memory_clear(backend);

    PerformanceTimer timer;

    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Performance test message %d", i);
    }

    double elapsed = timer.ElapsedMs();
    PrintResults("Sync Mode Throughput", PERF_ITERATIONS, elapsed);

    /* Verify messages were written */
    EXPECT_GT(log_backend_memory_size(backend), 0u);

    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test level filtering performance
 */
TEST_F(LogPerformanceTest, LevelFilteringPerformance) {
    InitLog();
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    /* Test 1: All messages pass filter */
    log_set_level(LOG_LEVEL_TRACE);
    Warmup();
    log_backend_memory_clear(backend);

    PerformanceTimer timer1;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Test message");
    }
    double elapsed_pass = timer1.ElapsedMs();

    /* Test 2: All messages filtered */
    log_set_level(LOG_LEVEL_FATAL);
    log_backend_memory_clear(backend);

    PerformanceTimer timer2;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Test message");
    }
    double elapsed_filter = timer2.ElapsedMs();

    PrintResults("Level Filtering (Pass)", PERF_ITERATIONS, elapsed_pass);
    PrintResults("Level Filtering (Filtered)", PERF_ITERATIONS, elapsed_filter);

    /* Filtered should be faster */
    EXPECT_LT(elapsed_filter, elapsed_pass);

    std::cout << "Filtering overhead: "
              << (elapsed_filter / elapsed_pass * 100.0) << "%" << std::endl;

    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test format complexity performance
 */
TEST_F(LogPerformanceTest, FormatComplexityPerformance) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    /* Test 1: Simple format */
    log_set_format("%m");
    Warmup();
    log_backend_memory_clear(backend);

    PerformanceTimer timer1;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Test");
    }
    double elapsed_simple = timer1.ElapsedMs();

    /* Test 2: Complex format */
    log_set_format("[%T] [%L] [%M] %F:%n %f() %m");
    log_backend_memory_clear(backend);

    PerformanceTimer timer2;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Test");
    }
    double elapsed_complex = timer2.ElapsedMs();

    PrintResults("Simple Format", PERF_ITERATIONS, elapsed_simple);
    PrintResults("Complex Format", PERF_ITERATIONS, elapsed_complex);

    std::cout << "Format overhead: "
              << ((elapsed_complex - elapsed_simple) / elapsed_simple * 100.0)
              << "%" << std::endl;

    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test message length performance
 */
TEST_F(LogPerformanceTest, MessageLengthPerformance) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    /* Test 1: Short messages */
    Warmup();
    log_backend_memory_clear(backend);

    PerformanceTimer timer1;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Short");
    }
    double elapsed_short = timer1.ElapsedMs();

    /* Test 2: Long messages */
    std::string long_msg(100, 'x');
    log_backend_memory_clear(backend);

    PerformanceTimer timer2;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("%s", long_msg.c_str());
    }
    double elapsed_long = timer2.ElapsedMs();

    PrintResults("Short Messages", PERF_ITERATIONS, elapsed_short);
    PrintResults("Long Messages", PERF_ITERATIONS, elapsed_long);

    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test multiple backends performance
 */
TEST_F(LogPerformanceTest, MultipleBackendsPerformance) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    /* Test 1: Single backend */
    log_backend_t* backend1 = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend1);
    ASSERT_LOG_OK(log_backend_register(backend1));

    Warmup();
    log_backend_memory_clear(backend1);

    PerformanceTimer timer1;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Test");
    }
    double elapsed_single = timer1.ElapsedMs();

    /* Test 2: Multiple backends */
    log_backend_t* backend2 = log_backend_memory_create(65536);
    log_backend_t* backend3 = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend2);
    ASSERT_NE(nullptr, backend3);

    backend2->name = "memory2";
    backend3->name = "memory3";

    ASSERT_LOG_OK(log_backend_register(backend2));
    ASSERT_LOG_OK(log_backend_register(backend3));

    log_backend_memory_clear(backend1);

    PerformanceTimer timer2;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Test");
    }
    double elapsed_multiple = timer2.ElapsedMs();

    PrintResults("Single Backend", PERF_ITERATIONS, elapsed_single);
    PrintResults("Three Backends", PERF_ITERATIONS, elapsed_multiple);

    std::cout << "Multi-backend overhead: "
              << ((elapsed_multiple - elapsed_single) / elapsed_single * 100.0)
              << "%" << std::endl;

    log_backend_unregister("memory");
    log_backend_unregister("memory2");
    log_backend_unregister("memory3");
    log_backend_memory_destroy(backend1);
    log_backend_memory_destroy(backend2);
    log_backend_memory_destroy(backend3);
}

/**
 * \brief           Test module filtering performance
 */
TEST_F(LogPerformanceTest, ModuleFilteringPerformance) {
    InitLog();
    log_set_level(LOG_LEVEL_INFO);
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    /* Test 1: No module filters */
    Warmup();
    log_backend_memory_clear(backend);

    PerformanceTimer timer1;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        log_write(LOG_LEVEL_INFO, "test.module", __FILE__, __LINE__, __func__,
                  "Test");
    }
    double elapsed_no_filter = timer1.ElapsedMs();

    /* Test 2: With module filters */
    log_module_set_level("test.*", LOG_LEVEL_DEBUG);
    log_backend_memory_clear(backend);

    PerformanceTimer timer2;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        log_write(LOG_LEVEL_INFO, "test.module", __FILE__, __LINE__, __func__,
                  "Test");
    }
    double elapsed_with_filter = timer2.ElapsedMs();

    PrintResults("No Module Filter", PERF_ITERATIONS, elapsed_no_filter);
    PrintResults("With Module Filter", PERF_ITERATIONS, elapsed_with_filter);

    std::cout << "Module filter overhead: "
              << ((elapsed_with_filter - elapsed_no_filter) /
                  elapsed_no_filter * 100.0)
              << "%" << std::endl;

    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test printf formatting performance
 */
TEST_F(LogPerformanceTest, PrintfFormattingPerformance) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    /* Test 1: No formatting */
    Warmup();
    log_backend_memory_clear(backend);

    PerformanceTimer timer1;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Static message");
    }
    double elapsed_static = timer1.ElapsedMs();

    /* Test 2: Simple formatting */
    log_backend_memory_clear(backend);

    PerformanceTimer timer2;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Value: %d", i);
    }
    double elapsed_simple = timer2.ElapsedMs();

    /* Test 3: Complex formatting */
    log_backend_memory_clear(backend);

    PerformanceTimer timer3;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Values: %d, %s, %.2f", i, "test", 3.14);
    }
    double elapsed_complex = timer3.ElapsedMs();

    PrintResults("Static Message", PERF_ITERATIONS, elapsed_static);
    PrintResults("Simple Formatting", PERF_ITERATIONS, elapsed_simple);
    PrintResults("Complex Formatting", PERF_ITERATIONS, elapsed_complex);

    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/**
 * \brief           Test memory backend performance
 */
TEST_F(LogPerformanceTest, MemoryBackendPerformance) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    /* Test different buffer sizes */
    size_t sizes[] = {1024, 4096, 16384, 65536};

    for (size_t size : sizes) {
        log_backend_t* backend = log_backend_memory_create(size);
        ASSERT_NE(nullptr, backend);

        /* Rename to avoid conflicts */
        std::string name = "memory_" + std::to_string(size);
        backend->name = name.c_str();

        ASSERT_LOG_OK(log_backend_register(backend));

        PerformanceTimer timer;
        for (int i = 0; i < PERF_ITERATIONS; i++) {
            LOG_INFO("Test message %d", i);
        }
        double elapsed = timer.ElapsedMs();

        std::string test_name =
            "Memory Backend (" + std::to_string(size) + " bytes)";
        PrintResults(test_name.c_str(), PERF_ITERATIONS, elapsed);

        log_backend_unregister(name.c_str());
        log_backend_memory_destroy(backend);
    }
}

/**
 * \brief           Test worst-case performance
 */
TEST_F(LogPerformanceTest, WorstCasePerformance) {
    /* Worst case: complex format, long message, multiple backends */
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("[%T] [%L] [%M] %F:%n %f() %m");

    /* Create multiple backends */
    log_backend_t* backend1 = log_backend_memory_create(65536);
    log_backend_t* backend2 = log_backend_memory_create(65536);
    log_backend_t* backend3 = log_backend_memory_create(65536);

    backend2->name = "memory2";
    backend3->name = "memory3";

    ASSERT_LOG_OK(log_backend_register(backend1));
    ASSERT_LOG_OK(log_backend_register(backend2));
    ASSERT_LOG_OK(log_backend_register(backend3));

    /* Long message */
    std::string long_msg(200, 'x');

    PerformanceTimer timer;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        log_write(LOG_LEVEL_INFO, "test.module.submodule", __FILE__, __LINE__,
                  __func__, "%s %d", long_msg.c_str(), i);
    }
    double elapsed = timer.ElapsedMs();

    PrintResults("Worst Case", PERF_ITERATIONS, elapsed);

    log_backend_unregister("memory");
    log_backend_unregister("memory2");
    log_backend_unregister("memory3");
    log_backend_memory_destroy(backend1);
    log_backend_memory_destroy(backend2);
    log_backend_memory_destroy(backend3);
}

/**
 * \brief           Test best-case performance
 */
TEST_F(LogPerformanceTest, BestCasePerformance) {
    /* Best case: simple format, short message, single backend, filtered */
    InitLog();
    log_set_level(LOG_LEVEL_FATAL); /* Filter everything */
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_LOG_OK(log_backend_register(backend));

    PerformanceTimer timer;
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        LOG_INFO("Test"); /* Will be filtered */
    }
    double elapsed = timer.ElapsedMs();

    PrintResults("Best Case (Filtered)", PERF_ITERATIONS, elapsed);

    log_backend_unregister("memory");
    log_backend_memory_destroy(backend);
}

/*---------------------------------------------------------------------------*/
/* End of Performance Tests                                                  */
/*---------------------------------------------------------------------------*/
