/**
 * \file            test_log_helpers.h
 * \brief           Log Framework Test Helper Functions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef TEST_LOG_HELPERS_H
#define TEST_LOG_HELPERS_H

#include <chrono>
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "log/log.h"
#include "log/log_backend.h"
}

/*---------------------------------------------------------------------------*/
/* Global Backend Cache                                                      */
/*---------------------------------------------------------------------------*/

/* Cache for backend content - memory backend read is destructive */
static std::map<log_backend_t*, std::string> g_backend_cache;

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

inline void ClearBackendCache();

/*---------------------------------------------------------------------------*/
/* Test Helper Macros                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Assert log status is OK
 */
#define ASSERT_LOG_OK(expr)                                                    \
    do {                                                                       \
        log_status_t __status = (expr);                                        \
        ASSERT_EQ(LOG_OK, __status)                                            \
            << "Expression: " << #expr << " returned " << __status;            \
    } while (0)

/**
 * \brief           Expect log status is OK
 */
#define EXPECT_LOG_OK(expr)                                                    \
    do {                                                                       \
        log_status_t __status = (expr);                                        \
        EXPECT_EQ(LOG_OK, __status)                                            \
            << "Expression: " << #expr << " returned " << __status;            \
    } while (0)

/*---------------------------------------------------------------------------*/
/* Test Fixture Base Class                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Base test fixture for log tests
 */
class LogTestBase : public ::testing::Test {
  protected:
    void SetUp() override {
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
        /* Clear backend cache */
        ClearBackendCache();
    }

    /**
     * \brief       Initialize log with default config
     */
    void InitLog() {
        ASSERT_LOG_OK(log_init(NULL));
    }

    /**
     * \brief       Initialize log with custom config
     */
    void InitLog(const log_config_t* config) {
        ASSERT_LOG_OK(log_init(config));
    }

    /**
     * \brief       Create and register memory backend
     */
    log_backend_t* CreateMemoryBackend(size_t size = 4096) {
        log_backend_t* backend = log_backend_memory_create(size);
        EXPECT_NE(nullptr, backend);
        if (backend != nullptr) {
            EXPECT_LOG_OK(log_backend_register(backend));
        }
        return backend;
    }

    /**
     * \brief       Cleanup memory backend
     */
    void CleanupMemoryBackend(log_backend_t* backend) {
        if (backend != nullptr) {
            log_backend_unregister("memory");
            log_backend_memory_destroy(backend);
        }
    }

    /**
     * \brief       Read all content from memory backend
     */
    std::string ReadMemoryBackend(log_backend_t* backend) {
        char buf[4096];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        return std::string(buf, len);
    }

    /**
     * \brief       Check if string contains substring
     */
    bool Contains(const std::string& str, const std::string& substr) {
        return str.find(substr) != std::string::npos;
    }
};

/*---------------------------------------------------------------------------*/
/* Performance Measurement Helpers                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Performance measurement helper
 */
class PerformanceTimer {
  public:
    PerformanceTimer() : start_(std::chrono::high_resolution_clock::now()) {
    }

    /**
     * \brief       Get elapsed time in milliseconds
     */
    double ElapsedMs() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    /**
     * \brief       Get elapsed time in microseconds
     */
    double ElapsedUs() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start_).count();
    }

    /**
     * \brief       Reset timer
     */
    void Reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

  private:
    std::chrono::high_resolution_clock::time_point start_;
};

/*---------------------------------------------------------------------------*/
/* Memory Tracking Helpers                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Memory usage tracker
 */
class MemoryTracker {
  public:
    /**
     * \brief       Take memory snapshot
     */
    void Snapshot() {
        /* Platform-specific memory tracking */
        /* This is a placeholder - implement based on platform */
    }

    /**
     * \brief       Get memory delta since last snapshot
     */
    size_t GetDelta() const {
        /* Return memory delta */
        return 0; /* Placeholder */
    }
};

/*---------------------------------------------------------------------------*/
/* Test Data Generators                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Generate random string
 */
inline std::string GenerateRandomString(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "0123456789";
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[rand() % (sizeof(charset) - 1)];
    }
    return result;
}

/**
 * \brief           Generate random log level
 */
inline log_level_t GenerateRandomLevel() {
    return static_cast<log_level_t>(rand() % (LOG_LEVEL_NONE + 1));
}

/**
 * \brief           Generate random message level (excluding NONE)
 */
inline log_level_t GenerateRandomMessageLevel() {
    return static_cast<log_level_t>(rand() % LOG_LEVEL_NONE);
}

/*---------------------------------------------------------------------------*/
/* Assertion Helpers                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Assert backend contains message
 */
inline void AssertBackendContains(log_backend_t* backend,
                                  const std::string& expected) {
    /* Give time for messages to be processed */
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    /* Check if we need to read from backend */
    if (g_backend_cache.find(backend) == g_backend_cache.end()) {
        char buf[4096];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        g_backend_cache[backend] = std::string(buf, len);
    }

    const std::string& content = g_backend_cache[backend];
    EXPECT_NE(content.find(expected), std::string::npos)
        << "Expected to find: " << expected << "\nIn: " << content;
}

/**
 * \brief           Assert backend does not contain message
 */
inline void AssertBackendNotContains(log_backend_t* backend,
                                     const std::string& unexpected) {
    /* Give time for messages to be processed */
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    /* Check if we need to read from backend */
    if (g_backend_cache.find(backend) == g_backend_cache.end()) {
        char buf[4096];
        size_t len = log_backend_memory_read(backend, buf, sizeof(buf));
        g_backend_cache[backend] = std::string(buf, len);
    }

    const std::string& content = g_backend_cache[backend];
    EXPECT_EQ(content.find(unexpected), std::string::npos)
        << "Did not expect to find: " << unexpected << "\nIn: " << content;
}

/**
 * \brief           Clear backend cache (call between tests)
 */
inline void ClearBackendCache() {
    g_backend_cache.clear();
}

#endif /* TEST_LOG_HELPERS_H */
