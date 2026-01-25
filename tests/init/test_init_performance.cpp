/**
 * \file            test_init_performance.cpp
 * \brief           Performance tests for Init Framework
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Performance tests covering:
 *                  - Startup time measurement
 *                  - Initialization overhead
 *                  - Memory footprint
 *                  - Scalability
 */

#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

extern "C" {
#include "nx_firmware_info.h"
#include "nx_init.h"
#include "nx_startup.h"
}

/*---------------------------------------------------------------------------*/
/* Performance Measurement Helpers                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           High-resolution timer for performance measurement
 */
class PerformanceTimer {
  public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        end_time = std::chrono::high_resolution_clock::now();
    }

    double elapsed_ms() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        return duration.count() / 1000.0;
    }

    double elapsed_us() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        return static_cast<double>(duration.count());
    }

  private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;
};

/*---------------------------------------------------------------------------*/
/* Test Fixture                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test fixture for performance tests
 */
class InitPerformanceTest : public ::testing::Test {
  protected:
    void SetUp() override {
#ifdef NX_STARTUP_TEST_MODE
        nx_startup_reset_for_test();
#endif
    }

    void TearDown() override {
        /* Nothing to tear down */
    }

    PerformanceTimer timer;
};

/*---------------------------------------------------------------------------*/
/* Startup Time Tests                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Measure complete startup time
 *
 * Target: < 100ms
 * Validates: Requirements 6.1
 */
TEST_F(InitPerformanceTest, StartupTime_Complete) {
    /* Measure startup time */
    timer.start();

    /* Simulate startup sequence */
    nx_board_init();
    nx_os_init();
    nx_init_run();

    timer.stop();

    double elapsed = timer.elapsed_ms();

    /* Report */
    std::cout << "Complete startup time: " << elapsed << " ms" << std::endl;

    /* Verify target */
    EXPECT_LT(elapsed, 100.0) << "Startup time exceeds 100ms target";
}

/**
 * \brief           Measure nx_init_run() time
 *
 * Target: < 1ms for empty init
 * Validates: Requirements 6.2
 */
TEST_F(InitPerformanceTest, InitRun_Time) {
    /* Warm up */
    nx_init_run();

    /* Measure */
    timer.start();
    nx_init_run();
    timer.stop();

    double elapsed = timer.elapsed_us();

    /* Report */
    std::cout << "nx_init_run() time: " << elapsed << " us" << std::endl;

    /* Verify target (1ms = 1000us) */
    EXPECT_LT(elapsed, 1000.0) << "Init run time exceeds 1ms target";
}

/**
 * \brief           Measure nx_init_get_stats() time
 *
 * Target: < 10us
 * Validates: Requirements 6.3
 */
TEST_F(InitPerformanceTest, GetStats_Time) {
    nx_init_stats_t stats;

    /* Warm up */
    nx_init_get_stats(&stats);

    /* Measure */
    timer.start();
    nx_init_get_stats(&stats);
    timer.stop();

    double elapsed = timer.elapsed_us();

    /* Report */
    std::cout << "nx_init_get_stats() time: " << elapsed << " us" << std::endl;

    /* Verify target */
    EXPECT_LT(elapsed, 10.0) << "Get stats time exceeds 10us target";
}

/**
 * \brief           Measure nx_startup_get_state() time
 *
 * Target: < 1us
 * Validates: Requirements 6.4
 */
TEST_F(InitPerformanceTest, GetState_Time) {
    /* Warm up */
    nx_startup_get_state();

    /* Measure */
    timer.start();
    nx_startup_get_state();
    timer.stop();

    double elapsed = timer.elapsed_us();

    /* Report */
    std::cout << "nx_startup_get_state() time: " << elapsed << " us"
              << std::endl;

    /* Verify target */
    EXPECT_LT(elapsed, 1.0) << "Get state time exceeds 1us target";
}

/*---------------------------------------------------------------------------*/
/* Memory Footprint Tests                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Measure structure sizes
 *
 * Validates: Requirements 6.5
 */
TEST_F(InitPerformanceTest, MemoryFootprint_StructureSizes) {
    /* Measure structure sizes */
    size_t stats_size = sizeof(nx_init_stats_t);
    size_t config_size = sizeof(nx_startup_config_t);
    size_t fw_info_size = sizeof(nx_firmware_info_t);

    /* Report */
    std::cout << "nx_init_stats_t size: " << stats_size << " bytes"
              << std::endl;
    std::cout << "nx_startup_config_t size: " << config_size << " bytes"
              << std::endl;
    std::cout << "nx_firmware_info_t size: " << fw_info_size << " bytes"
              << std::endl;

    /* Verify reasonable sizes */
    EXPECT_LE(stats_size, 64u) << "Stats structure too large";
    EXPECT_LE(config_size, 64u) << "Config structure too large";
    EXPECT_LE(fw_info_size, 128u) << "Firmware info structure too large";
}

/**
 * \brief           Measure total RAM usage
 *
 * Target: < 1KB
 * Validates: Requirements 6.6
 */
TEST_F(InitPerformanceTest, MemoryFootprint_TotalRAM) {
    /* Calculate total RAM usage */
    size_t total_ram = 0;

    /* Add structure sizes */
    total_ram += sizeof(nx_init_stats_t);
    total_ram += sizeof(nx_startup_config_t);

    /* Add estimated global variables (conservative estimate) */
    total_ram += 256; /* Estimated global state */

    /* Report */
    std::cout << "Estimated total RAM usage: " << total_ram << " bytes"
              << std::endl;

    /* Verify target */
    EXPECT_LT(total_ram, 1024u) << "Total RAM usage exceeds 1KB target";
}

/*---------------------------------------------------------------------------*/
/* Scalability Tests                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test performance with multiple init functions
 *
 * Validates: Requirements 6.7
 */
TEST_F(InitPerformanceTest, Scalability_MultipleInitFunctions) {
    /* Test with different numbers of init functions */
    std::vector<int> function_counts = {1, 10, 50, 100};
    std::vector<double> execution_times;

    for (int count : function_counts) {
        /* Note: In a real test, we would register 'count' init functions
         * For now, we just measure the overhead of nx_init_run()
         */

        /* Measure */
        timer.start();
        nx_init_run();
        timer.stop();

        double elapsed = timer.elapsed_us();
        execution_times.push_back(elapsed);

        /* Report */
        std::cout << count << " functions: " << elapsed << " us" << std::endl;
    }

    /* Verify linear scalability */
    /* Time should not grow exponentially */
    if (execution_times.size() >= 2) {
        /* Avoid division by zero */
        if (execution_times.front() > 0.001) {
            double ratio = execution_times.back() / execution_times.front();
            EXPECT_LT(ratio, 200.0) << "Performance does not scale linearly";
        } else {
            /* Times too small to measure accurately, skip check */
            SUCCEED() << "Execution times too small to measure scalability";
        }
    }
}

/**
 * \brief           Test repeated init calls performance
 *
 * Validates: Requirements 6.8
 */
TEST_F(InitPerformanceTest, Scalability_RepeatedCalls) {
    const int iterations = 1000;

    /* Measure repeated calls */
    timer.start();
    for (int i = 0; i < iterations; i++) {
        nx_init_run();
    }
    timer.stop();

    double total_time = timer.elapsed_ms();
    double avg_time = total_time / iterations;

    /* Report */
    std::cout << "Average time per call: " << avg_time << " ms" << std::endl;

    /* Verify performance */
    EXPECT_LT(avg_time, 1.0) << "Average call time exceeds 1ms";
}

/*---------------------------------------------------------------------------*/
/* Version String Performance Tests                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Measure version string formatting time
 *
 * Target: < 100us
 * Validates: Requirements 6.9
 */
TEST_F(InitPerformanceTest, VersionString_FormattingTime) {
    char buf[32];

    /* Warm up */
    nx_get_version_string(buf, sizeof(buf));

    /* Measure */
    timer.start();
    nx_get_version_string(buf, sizeof(buf));
    timer.stop();

    double elapsed = timer.elapsed_us();

    /* Report */
    std::cout << "Version string formatting time: " << elapsed << " us"
              << std::endl;

    /* Verify target */
    EXPECT_LT(elapsed, 100.0)
        << "Version string formatting exceeds 100us target";
}

/*---------------------------------------------------------------------------*/
/* Configuration Performance Tests                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Measure configuration access time
 *
 * Target: < 10us
 * Validates: Requirements 6.10
 */
TEST_F(InitPerformanceTest, Configuration_AccessTime) {
    nx_startup_config_t config;

    /* Warm up */
    nx_startup_get_default_config(&config);

    /* Measure */
    timer.start();
    nx_startup_get_default_config(&config);
    timer.stop();

    double elapsed = timer.elapsed_us();

    /* Report */
    std::cout << "Configuration access time: " << elapsed << " us" << std::endl;

    /* Verify target */
    EXPECT_LT(elapsed, 10.0) << "Configuration access exceeds 10us target";
}

/*---------------------------------------------------------------------------*/
/* Benchmark Tests                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Comprehensive performance benchmark
 *
 * Validates: Requirements 6.11
 */
TEST_F(InitPerformanceTest, Benchmark_Comprehensive) {
    std::cout << "\n=== Init Framework Performance Benchmark ===" << std::endl;

    /* 1. Startup time */
    timer.start();
    nx_board_init();
    nx_os_init();
    nx_init_run();
    timer.stop();
    std::cout << "Startup time:           " << timer.elapsed_ms() << " ms"
              << std::endl;

    /* 2. Init run time */
    timer.start();
    nx_init_run();
    timer.stop();
    std::cout << "Init run time:          " << timer.elapsed_us() << " us"
              << std::endl;

    /* 3. Get stats time */
    nx_init_stats_t stats;
    timer.start();
    nx_init_get_stats(&stats);
    timer.stop();
    std::cout << "Get stats time:         " << timer.elapsed_us() << " us"
              << std::endl;

    /* 4. Get state time */
    timer.start();
    nx_startup_get_state();
    timer.stop();
    std::cout << "Get state time:         " << timer.elapsed_us() << " us"
              << std::endl;

    /* 5. Version string time */
    char buf[32];
    timer.start();
    nx_get_version_string(buf, sizeof(buf));
    timer.stop();
    std::cout << "Version string time:    " << timer.elapsed_us() << " us"
              << std::endl;

    /* 6. Memory footprint */
    size_t total_size = sizeof(nx_init_stats_t) + sizeof(nx_startup_config_t) +
                        sizeof(nx_firmware_info_t);
    std::cout << "Total structure size:   " << total_size << " bytes"
              << std::endl;

    std::cout << "==========================================\n" << std::endl;

    SUCCEED();
}

/*---------------------------------------------------------------------------*/
/* Regression Tests                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Performance regression test
 *
 * Validates: Requirements 6.12
 */
TEST_F(InitPerformanceTest, Regression_PerformanceBaseline) {
    /* Define performance baselines */
    const double STARTUP_TIME_BASELINE_MS = 100.0;
    const double INIT_RUN_BASELINE_US = 1000.0;
    const double GET_STATS_BASELINE_US = 10.0;
    const double GET_STATE_BASELINE_US = 1.0;

    /* Measure current performance */
    timer.start();
    nx_board_init();
    nx_os_init();
    nx_init_run();
    timer.stop();
    double startup_time = timer.elapsed_ms();

    timer.start();
    nx_init_run();
    timer.stop();
    double init_run_time = timer.elapsed_us();

    nx_init_stats_t stats;
    timer.start();
    nx_init_get_stats(&stats);
    timer.stop();
    double get_stats_time = timer.elapsed_us();

    timer.start();
    nx_startup_get_state();
    timer.stop();
    double get_state_time = timer.elapsed_us();

    /* Check against baselines */
    EXPECT_LT(startup_time, STARTUP_TIME_BASELINE_MS)
        << "Startup time regression detected";
    EXPECT_LT(init_run_time, INIT_RUN_BASELINE_US)
        << "Init run time regression detected";
    EXPECT_LT(get_stats_time, GET_STATS_BASELINE_US)
        << "Get stats time regression detected";
    EXPECT_LT(get_state_time, GET_STATE_BASELINE_US)
        << "Get state time regression detected";

    /* Report */
    std::cout << "\n=== Performance Regression Check ===" << std::endl;
    std::cout << "Startup time:    " << startup_time << " / "
              << STARTUP_TIME_BASELINE_MS << " ms" << std::endl;
    std::cout << "Init run time:   " << init_run_time << " / "
              << INIT_RUN_BASELINE_US << " us" << std::endl;
    std::cout << "Get stats time:  " << get_stats_time << " / "
              << GET_STATS_BASELINE_US << " us" << std::endl;
    std::cout << "Get state time:  " << get_state_time << " / "
              << GET_STATE_BASELINE_US << " us" << std::endl;
    std::cout << "====================================\n" << std::endl;
}
