/**
 * \file            common_diagnostics_test.cpp
 * \brief           Raw crash retention, formatting and health contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fault_record.h"
#include "nexus/components/health.h"
#include "nexus/components/log_format.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <string>

namespace {
TEST(CommonDiagnostics, FaultRoundTripPreservesNumericFactsAndResetBits) {
    nx_fault_record_t record{};
    nx_fault_capture_t facts{};
    facts.sequence = 7;
    facts.domain = NX_FAULT_PLATFORM;
    facts.reason = NX_ERROR_IO;
    facts.flags = NX_FAULT_CORE_VALID | NX_FAULT_STATUS_VALID;
    facts.exception = 3;
    facts.exception_return = 0xffffffedU;
    facts.msp = 0x20001000;
    facts.psp = 0x20002000;
    facts.core[6] = 0x08001234;
    facts.status[0] = 0x82;
    facts.build_id[3] = 0x47;
    ASSERT_EQ(nx_fault_record_capture(&record, &facts), NX_SUCCESS);
    nx_fault_capture_t copy{};
    ASSERT_EQ(nx_fault_record_read(&record, &copy), NX_SUCCESS);
    EXPECT_EQ(std::memcmp(&facts, &copy, sizeof(facts)), 0);
    nx_reset_record_t boot{};
    ASSERT_EQ(nx_reset_record_read(&record, 0x58, &boot), NX_SUCCESS);
    EXPECT_EQ(boot.reset_causes, 0x58U);
    EXPECT_TRUE(boot.has_fault);
    EXPECT_EQ(boot.fault.core[6], 0x08001234U);
}

TEST(CommonDiagnostics, FirstCrashWinsUntilExactAcknowledgement) {
    nx_fault_record_t record{};
    nx_fault_capture_t facts{};
    facts.sequence = 1;
    ASSERT_EQ(nx_fault_record_capture(&record, &facts), NX_SUCCESS);
    const auto before = record;
    facts.sequence = 2;
    EXPECT_EQ(nx_fault_record_capture(&record, &facts), NX_ERROR_BUSY);
    EXPECT_EQ(std::memcmp(&record, &before, sizeof(record)), 0);
    EXPECT_EQ(nx_fault_record_ack(&record, 2), NX_ERROR_STATE);
    ASSERT_EQ(nx_fault_record_ack(&record, 1), NX_SUCCESS);
    nx_fault_capture_t output{};
    EXPECT_EQ(nx_fault_record_read(&record, &output), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_fault_record_capture(&record, &facts), NX_SUCCESS);
}

TEST(CommonDiagnostics, CorruptionNeverBecomesAValidCrashOrEmptyBoot) {
    nx_fault_record_t record{};
    nx_fault_capture_t facts{};
    facts.sequence = 99;
    ASSERT_EQ(nx_fault_record_capture(&record, &facts), NX_SUCCESS);
    record.bytes[48] ^= 1;
    nx_fault_capture_t output{};
    output.sequence = 123;
    EXPECT_EQ(nx_fault_record_read(&record, &output), NX_ERROR_IO);
    EXPECT_EQ(output.sequence, 123U);
    nx_reset_record_t boot{};
    boot.reset_causes = 0x1234;
    EXPECT_EQ(nx_reset_record_read(&record, 2, &boot), NX_ERROR_IO);
    EXPECT_EQ(boot.reset_causes, 0x1234U);
}

TEST(CommonDiagnostics, AbsentCrashHasIndependentResetFacts) {
    nx_fault_record_t record{};
    nx_reset_record_t boot{};
    ASSERT_EQ(nx_reset_record_read(&record, 0x41, &boot), NX_SUCCESS);
    EXPECT_FALSE(boot.has_fault);
    EXPECT_EQ(boot.reset_causes, 0x41U);
    EXPECT_EQ(sizeof(record), NX_FAULT_RECORD_BYTES);
}

TEST(CommonDiagnostics, HealthPreservesUnknownAndDoesNotInferHealthy) {
    nx_health_facts_t facts{nullptr, nullptr, 100, 20};
    nx_health_snapshot_t output{};
    ASSERT_EQ(nx_health_snapshot(&facts, &output), NX_SUCCESS);
    EXPECT_EQ(output.known, 0U);
    EXPECT_EQ(output.io_unknown_facts, NX_IO_FACT_ALL);
    EXPECT_EQ(output.findings, 0U);
}

TEST(CommonDiagnostics, ProgressBudgetUsesActualSameDomainOccurrences) {
    nx_progress_t progress{};
    ASSERT_EQ(nx_progress_record(&progress, 50), NX_SUCCESS);
    ASSERT_EQ(nx_progress_record(&progress, 60), NX_SUCCESS);
    nx_io_diagnostic_t io{NX_IO_DIAG_QUARANTINED, NX_IO_FACT_SERVICE};
    nx_health_facts_t facts{&progress, &io, 81, 20};
    nx_health_snapshot_t output{};
    ASSERT_EQ(nx_health_snapshot(&facts, &output), NX_SUCCESS);
    EXPECT_EQ(output.known, NX_HEALTH_PROGRESS_KNOWN | NX_HEALTH_IO_KNOWN);
    EXPECT_EQ(output.findings,
              NX_HEALTH_PROGRESS_OVERDUE | NX_HEALTH_IO_FINDINGS);
    EXPECT_EQ(output.progress_age_us, 21U);
    EXPECT_EQ(output.progress_generation, 2U);
    EXPECT_EQ(output.io_unknown_facts, NX_IO_FACT_SERVICE);
    EXPECT_EQ(nx_progress_record(&progress, 59), NX_ERROR_STATE);
    EXPECT_EQ(progress.progressed_at, 60U);
    facts.observed_at = 59;
    const auto before = output;
    EXPECT_EQ(nx_health_snapshot(&facts, &output), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&output, &before, sizeof(output)), 0);
}

struct CapturedLog {
    std::string bytes;
    unsigned writes = 0;
    static nx_result_t write(void* context, nx_log_level_t, const void* data,
                             size_t size, nx_time_us_t) {
        auto& self = *static_cast<CapturedLog*>(context);
        ++self.writes;
        self.bytes.assign(static_cast<const char*>(data), size);
        return NX_SUCCESS;
    }
};

TEST(CommonDiagnostics, FormatterPublishesOnlyWholeAdmittedRecords) {
    CapturedLog capture;
    nx_log_t logger{};
    ASSERT_EQ(nx_log_init(&logger, {&capture, CapturedLog::write, nullptr},
                          NX_LOG_INFO),
              NX_SUCCESS);
    std::array<char, 64> buffer{};
    EXPECT_EQ(nx_log_printf(&logger, NX_LOG_INFO, buffer.data(), buffer.size(),
                            99, "value=%08x %.4s", 0x42, "hello"),
              NX_SUCCESS);
    EXPECT_EQ(capture.bytes, "value=00000042 hell");
    EXPECT_EQ(nx_log_printf(&logger, NX_LOG_INFO, buffer.data(), 4, 99,
                            "long=%u", 42U),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(capture.writes, 1U);
    EXPECT_EQ(nx_log_printf(&logger, NX_LOG_INFO, buffer.data(), buffer.size(),
                            99, "%s", "unbounded"),
              NX_ERROR_INVALID);
    int written = 77;
    EXPECT_EQ(nx_log_printf(&logger, NX_LOG_INFO, buffer.data(), buffer.size(),
                            99, "%n", &written),
              NX_ERROR_INVALID);
    EXPECT_EQ(written, 77);
    EXPECT_EQ(capture.writes, 1U);
}

TEST(CommonDiagnostics, FormatterIntegerBoundsMatchIndependentHostReference) {
    CapturedLog capture;
    nx_log_t logger{};
    ASSERT_EQ(nx_log_init(&logger, {&capture, CapturedLog::write, nullptr},
                          NX_LOG_DEBUG),
              NX_SUCCESS);
    std::array<char, 256> scratch{};
    std::array<char, 256> reference{};
    const auto minimum = static_cast<long long>(INT64_MIN);
    const auto maximum = static_cast<unsigned long long>(UINT64_MAX);
    const char* format = "%+024lld|%#020llx|%#.0o|%-.0u|%zu|%td|%.5s|%4c";
    std::snprintf(reference.data(), reference.size(), format, minimum, maximum,
                  0U, 0U, size_t{123}, ptrdiff_t{-7}, "abcdef", 'z');
    ASSERT_EQ(nx_log_printf(&logger, NX_LOG_INFO, scratch.data(),
                            scratch.size(), 100, format, minimum, maximum, 0U,
                            0U, size_t{123}, ptrdiff_t{-7}, "abcdef", 'z'),
              NX_SUCCESS);
    EXPECT_EQ(capture.bytes, reference.data());
    format = "%hhd|%hu|%ld|%jd|%#o|%#08X|%8.4d|%-8.3s|%%";
    std::snprintf(reference.data(), reference.size(), format, -128, 65535,
                  long{-1234}, intmax_t{-99}, 511U, 0xABU, -12, "abcdef");
    ASSERT_EQ(nx_log_printf(&logger, NX_LOG_INFO, scratch.data(),
                            scratch.size(), 100, format, -128, 65535,
                            long{-1234}, intmax_t{-99}, 511U, 0xABU, -12,
                            "abcdef"),
              NX_SUCCESS);
    EXPECT_EQ(capture.bytes, reference.data());
}

TEST(CommonDiagnostics, FormatterRejectsUnboundedOrExcessWorkBeforeSink) {
    CapturedLog capture;
    nx_log_t logger{};
    ASSERT_EQ(nx_log_init(&logger, {&capture, CapturedLog::write, nullptr},
                          NX_LOG_DEBUG),
              NX_SUCCESS);
    std::array<char, 16> scratch{};
    EXPECT_EQ(nx_log_printf(&logger, NX_LOG_INFO, scratch.data(),
                            scratch.size(), 100, "%1025u", 1U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_log_printf(&logger, NX_LOG_INFO, scratch.data(),
                            scratch.size(), 100, "%*u", 3, 1U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_log_printf(&logger, NX_LOG_INFO, scratch.data(),
                            scratch.size(), 100, "%f", 1.0),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_log_printf(&logger, NX_LOG_INFO, scratch.data(), 1, 100, "x"),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(capture.writes, 0U);
}
} /* namespace */
