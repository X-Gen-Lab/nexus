/**
 * \file            log_lifecycle_test.cpp
 * \brief           Explicit logger filtering, recursion and settled sink tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-26
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/log.h"
#include <cstring>
#include <gtest/gtest.h>

extern "C" nx_result_t
nx_test_log_init_level(nx_log_t* logger, nx_log_sink_port_t sink, int level);
extern "C" nx_result_t nx_test_log_write_level(nx_log_t* logger, int level,
                                               const void* bytes, size_t length,
                                               nx_time_us_t deadline);

namespace {
struct LogBoundary {
    nx_log_t* logger = nullptr;
    unsigned calls = 0U;
    bool stop = false;
    nx_result_t result = NX_SUCCESS;

    static nx_result_t write(void* context, nx_log_level_t level,
                             const void* bytes, size_t length,
                             nx_time_us_t deadline) {
        auto& self = *static_cast<LogBoundary*>(context);
        EXPECT_EQ(level, NX_LOG_INFO);
        EXPECT_EQ(deadline, 100U);
        EXPECT_EQ(length, 4U);
        EXPECT_NE(bytes, nullptr);
        if (bytes == nullptr || length != 4U) {
            return NX_ERROR_INVALID;
        }
        EXPECT_EQ(std::memcmp(bytes, "test", length), 0);
        ++self.calls;
        EXPECT_EQ(nx_log_write(self.logger, NX_LOG_INFO, "test", 4U, deadline),
                  NX_ERROR_BUSY);
        if (self.stop) {
            EXPECT_EQ(nx_log_stop(self.logger), NX_ERROR_BUSY);
        }
        return self.result;
    }
};

class LogLifecycle : public testing::Test {
  protected:
    nx_log_t logger{};
    LogBoundary boundary{&logger};
    nx_log_sink_port_t sink{&boundary, LogBoundary::write, nullptr};
};

TEST_F(LogLifecycle, InvalidCLevelsRejectBeforeCallingTheSink) {
    EXPECT_EQ(nx_test_log_init_level(&logger, sink, -1), NX_ERROR_INVALID);
    EXPECT_EQ(nx_test_log_init_level(&logger, sink, NX_LOG_ERROR + 1),
              NX_ERROR_INVALID);
    ASSERT_EQ(nx_log_init(&logger, sink, NX_LOG_INFO), NX_SUCCESS);
    EXPECT_EQ(nx_test_log_write_level(&logger, -1, "test", 4U, 100U),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_test_log_write_level(&logger, NX_LOG_ERROR + 1, "test", 4U, 100U),
        NX_ERROR_INVALID);
    EXPECT_EQ(boundary.calls, 0U);
}

TEST_F(LogLifecycle, FilteredRecordsHaveNoSinkEffectsAndErrorsReleaseBusy) {
    ASSERT_EQ(nx_log_init(&logger, sink, NX_LOG_INFO), NX_SUCCESS);
    EXPECT_EQ(nx_log_write(&logger, NX_LOG_DEBUG, "test", 4U, 100U),
              NX_SUCCESS);
    EXPECT_EQ(boundary.calls, 0U);
    boundary.result = NX_ERROR_IO;
    EXPECT_EQ(nx_log_write(&logger, NX_LOG_INFO, "test", 4U, 100U),
              NX_ERROR_IO);
    EXPECT_EQ(boundary.calls, 1U);
    EXPECT_EQ(logger.busy, 0U);
    EXPECT_EQ(nx_log_flush(&logger, 100U), NX_ERROR_UNSUPPORTED);
}

TEST_F(LogLifecycle, RecursiveWriteAndStopRetainTheActiveSinkUntilReturn) {
    ASSERT_EQ(nx_log_init(&logger, sink, NX_LOG_INFO), NX_SUCCESS);
    boundary.stop = true;
    ASSERT_EQ(nx_log_write(&logger, NX_LOG_INFO, "test", 4U, 100U), NX_SUCCESS);
    EXPECT_EQ(boundary.calls, 1U);
    ASSERT_EQ(nx_log_stop(&logger), NX_SUCCESS);
    EXPECT_EQ(nx_log_write(&logger, NX_LOG_INFO, "test", 4U, 100U),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_log_flush(&logger, 100U), NX_ERROR_STATE);
}
} /* namespace */
