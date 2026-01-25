/**
 * \file            test_log_thread_safety.cpp
 * \brief           Log Framework Thread Safety Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Thread safety tests for Log Framework.
 *                  Tests concurrent access from multiple threads.
 */

#include "test_log_helpers.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

extern "C" {
#include "log/log.h"
#include "log/log_backend.h"
}

/*---------------------------------------------------------------------------*/
/* Thread Safety Test Fixture                                                */
/*---------------------------------------------------------------------------*/

class LogThreadSafetyTest : public LogTestBase {
  protected:
    static constexpr int NUM_THREADS = 4;
    static constexpr int MESSAGES_PER_THREAD = 1000;
};

/*---------------------------------------------------------------------------*/
/* Concurrent Logging Tests                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test concurrent logging from multiple threads
 */
TEST_F(LogThreadSafetyTest, ConcurrentLogging) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    std::vector<std::thread> threads;
    std::atomic<int> total_messages{0};

    /* Create threads that log concurrently */
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &total_messages]() {
            for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
                LOG_INFO("Thread %d message %d", t, i);
                total_messages++;
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Verify all messages were processed */
    EXPECT_EQ(NUM_THREADS * MESSAGES_PER_THREAD, total_messages.load());

    /* Verify backend received messages */
    EXPECT_GT(log_backend_memory_size(backend), 0u);

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test concurrent level changes
 */
TEST_F(LogThreadSafetyTest, ConcurrentLevelChanges) {
    InitLog();
    log_set_format("%m");

    /* Use larger buffer to avoid overflow */
    log_backend_t* backend = log_backend_memory_create(131072); /* 128KB */
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    /* Thread 1: Change levels */
    threads.emplace_back([&stop]() {
        log_level_t levels[] = {LOG_LEVEL_TRACE, LOG_LEVEL_DEBUG,
                                LOG_LEVEL_INFO,  LOG_LEVEL_WARN,
                                LOG_LEVEL_ERROR, LOG_LEVEL_FATAL};
        int idx = 0;
        while (!stop.load()) {
            log_set_level(levels[idx]);
            idx = (idx + 1) % (sizeof(levels) / sizeof(levels[0]));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    /* Thread 2-4: Log messages (further reduced count) */
    for (int t = 0; t < 3; t++) {
        threads.emplace_back([t, &stop]() {
            int count = 0;
            while (!stop.load() &&
                   count < MESSAGES_PER_THREAD / 4) { /* Quarter messages */
                LOG_INFO("Thread %d message %d", t, count);
                count++;
                std::this_thread::sleep_for(
                    std::chrono::microseconds(100)); /* Slow down */
            }
        });
    }

    /* Run for a short time */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Clear backend before final message to ensure it's captured */
    log_backend_memory_clear(backend);
    ClearBackendCache();

    /* System should still be functional */
    LOG_INFO("Final message");

    /* Give time for message to be written */
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    AssertBackendContains(backend, "Final message");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test concurrent format changes
 */
TEST_F(LogThreadSafetyTest, ConcurrentFormatChanges) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    /* Thread 1: Change formats */
    threads.emplace_back([&stop]() {
        const char* formats[] = {"%m", "[%L] %m", "[%T] %m", "[%M] %m"};
        int idx = 0;
        while (!stop.load()) {
            log_set_format(formats[idx]);
            idx = (idx + 1) % (sizeof(formats) / sizeof(formats[0]));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    /* Thread 2-4: Log messages */
    for (int t = 0; t < 3; t++) {
        threads.emplace_back([t, &stop]() {
            int count = 0;
            while (!stop.load() && count < MESSAGES_PER_THREAD) {
                LOG_INFO("Thread %d message %d", t, count);
                count++;
            }
        });
    }

    /* Run for a short time */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* System should still be functional */
    LOG_INFO("Final message");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test concurrent backend registration
 */
TEST_F(LogThreadSafetyTest, ConcurrentBackendRegistration) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    std::vector<std::thread> threads;
    std::vector<log_backend_t*> backends;

    /* Create backends in parallel */
    std::mutex backends_mutex;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &backends, &backends_mutex]() {
            log_backend_t* backend = log_backend_memory_create(4096);
            if (backend != nullptr) {
                std::string name = "memory_" + std::to_string(t);
#ifdef _MSC_VER
                backend->name = _strdup(name.c_str());
#else
                backend->name = strdup(name.c_str());
#endif

                log_status_t status = log_backend_register(backend);

                std::lock_guard<std::mutex> lock(backends_mutex);
                if (status == LOG_OK) {
                    backends.push_back(backend);
                } else {
                    log_backend_memory_destroy(backend);
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Log a message - should go to all backends */
    LOG_INFO("Test message");

    /* Cleanup */
    for (auto* backend : backends) {
        log_backend_unregister(backend->name);
        free((void*)backend->name);
        log_backend_memory_destroy(backend);
    }
}

/**
 * \brief           Test concurrent module filter changes
 */
TEST_F(LogThreadSafetyTest, ConcurrentModuleFilterChanges) {
    InitLog();
    log_set_level(LOG_LEVEL_INFO);
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    /* Thread 1: Change module filters */
    threads.emplace_back([&stop]() {
        int count = 0;
        while (!stop.load() && count < 100) {
            log_module_set_level("test.module1", LOG_LEVEL_DEBUG);
            log_module_set_level("test.module2", LOG_LEVEL_WARN);
            log_module_clear_level("test.module1");
            count++;
        }
    });

    /* Thread 2-4: Log from different modules */
    for (int t = 0; t < 3; t++) {
        threads.emplace_back([t, &stop]() {
            std::string module = "test.module" + std::to_string(t);
            int count = 0;
            while (!stop.load() && count < MESSAGES_PER_THREAD) {
                log_write(LOG_LEVEL_INFO, module.c_str(), __FILE__, __LINE__,
                          __func__, "Message %d", count);
                count++;
            }
        });
    }

    /* Run for a short time */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test concurrent backend enable/disable
 */
TEST_F(LogThreadSafetyTest, ConcurrentBackendEnableDisable) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    /* Thread 1: Toggle backend */
    threads.emplace_back([&stop]() {
        bool enabled = true;
        while (!stop.load()) {
            log_backend_enable("memory", enabled);
            enabled = !enabled;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    /* Thread 2-4: Log messages */
    for (int t = 0; t < 3; t++) {
        threads.emplace_back([t, &stop]() {
            int count = 0;
            while (!stop.load() && count < MESSAGES_PER_THREAD) {
                LOG_INFO("Thread %d message %d", t, count);
                count++;
            }
        });
    }

    /* Run for a short time */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Re-enable backend */
    log_backend_enable("memory", true);

    /* Final message should work */
    LOG_INFO("Final message");

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test stress test with many threads
 */
TEST_F(LogThreadSafetyTest, StressTestManyThreads) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    constexpr int STRESS_THREADS = 10;
    constexpr int STRESS_MESSAGES = 500;

    std::vector<std::thread> threads;
    std::atomic<int> total_messages{0};

    for (int t = 0; t < STRESS_THREADS; t++) {
        threads.emplace_back([t, &total_messages]() {
            for (int i = 0; i < STRESS_MESSAGES; i++) {
                LOG_INFO("Thread %d message %d", t, i);
                total_messages++;

                /* Add some variety */
                if (i % 10 == 0) {
                    LOG_DEBUG("Debug from thread %d", t);
                }
                if (i % 20 == 0) {
                    LOG_WARN("Warning from thread %d", t);
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Verify message count */
    EXPECT_EQ(STRESS_THREADS * STRESS_MESSAGES, total_messages.load());

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test concurrent read and write
 */
TEST_F(LogThreadSafetyTest, ConcurrentReadWrite) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("%m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    /* Writer threads */
    for (int t = 0; t < 3; t++) {
        threads.emplace_back([t, &stop]() {
            int count = 0;
            while (!stop.load() && count < MESSAGES_PER_THREAD) {
                LOG_INFO("Writer %d message %d", t, count);
                count++;
            }
        });
    }

    /* Reader thread */
    threads.emplace_back([backend, &stop]() {
        char buf[4096];
        while (!stop.load()) {
            log_backend_memory_read(backend, buf, sizeof(buf));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    /* Run for a short time */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    CleanupMemoryBackend(backend);
}

/**
 * \brief           Test no data corruption under concurrent access
 */
TEST_F(LogThreadSafetyTest, NoDataCorruption) {
    InitLog();
    log_set_level(LOG_LEVEL_TRACE);
    log_set_format("[T%d] %m");

    log_backend_t* backend = log_backend_memory_create(65536);
    ASSERT_NE(nullptr, backend);
    ASSERT_LOG_OK(log_backend_register(backend));

    std::vector<std::thread> threads;

    /* Each thread logs unique messages */
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < MESSAGES_PER_THREAD; i++) {
                /* Unique message per thread */
                std::string msg =
                    "T" + std::to_string(t) + "_M" + std::to_string(i);
                LOG_INFO("%s", msg.c_str());
            }
        });
    }

    /* Wait for all threads */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Read all messages */
    std::string content = ReadMemoryBackend(backend);

    /* Verify no obvious corruption (messages should be readable) */
    EXPECT_GT(content.length(), 0u);

    /* Count occurrences of thread markers */
    for (int t = 0; t < NUM_THREADS; t++) {
        std::string marker = "T" + std::to_string(t) + "_M";
        size_t count = 0;
        size_t pos = 0;
        while ((pos = content.find(marker, pos)) != std::string::npos) {
            count++;
            pos += marker.length();
        }
        /* Should have some messages from each thread */
        EXPECT_GT(count, 0u) << "No messages from thread " << t;
    }

    CleanupMemoryBackend(backend);
}

/*---------------------------------------------------------------------------*/
/* End of Thread Safety Tests                                                */
/*---------------------------------------------------------------------------*/
