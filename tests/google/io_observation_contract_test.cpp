/**
 * \file            io_observation_contract_test.cpp
 * \brief           Timestamp provenance and checked service dispatch contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/i2c.h"
#include "nexus/io/observation.h"
#include "nexus/io/spi.h"
#include "nexus/io/uart.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
nx_io_timestamp_t event(uint64_t ticks, nx_io_timestamp_source_t source) {
    return {ticks, 1000000U, 1U, 1U, 0U, source, true};
}

TEST(IOTimestamp, HardwareAndSoftwareObservationRemainExplicit) {
    auto hardware = event(100U, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    auto irq = event(103U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    irq.uncertainty_ticks = 2U;
    nx_io_timestamp_delta_t result{};
    ASSERT_EQ(nx_io_timestamp_elapsed(&hardware, &irq, &result), NX_SUCCESS);
    EXPECT_EQ(result.ticks, 3U);
    EXPECT_EQ(result.ticks_per_second, 1000000U);
    EXPECT_EQ(result.uncertainty_ticks, 2U);
    EXPECT_TRUE(result.uncertainty_known);
    EXPECT_EQ(hardware.source, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    EXPECT_EQ(irq.source, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
}

TEST(IOTimestamp, UnavailableSourceIsNotAZeroTimeMeasurement) {
    auto first = event(0U, NX_IO_TIMESTAMP_UNKNOWN);
    auto last = event(0U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    nx_io_timestamp_delta_t result{777U, 888U, 999U, true};
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, &result),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(result.ticks, 777U);
    EXPECT_EQ(result.ticks_per_second, 888U);
    EXPECT_EQ(result.uncertainty_ticks, 999U);
}

TEST(IOTimestamp, UnknownUncertaintyRemainsExplicitWithoutAFakeZeroBound) {
    auto first = event(100U, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    auto last = event(103U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    last.uncertainty_known = false;
    last.uncertainty_ticks = UINT64_MAX;
    nx_io_timestamp_delta_t result{};
    ASSERT_EQ(nx_io_timestamp_elapsed(&first, &last, &result), NX_SUCCESS);
    EXPECT_EQ(result.ticks, 3U);
    EXPECT_FALSE(result.uncertainty_known);
    EXPECT_EQ(result.uncertainty_ticks, 0U);
}

TEST(IOTimestamp, RejectsDomainEpochFrequencyAndReverseTime) {
    auto first = event(10U, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    const auto original = event(20U, NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION);
    for (unsigned index = 0U; index != 4U; ++index) {
        auto last = original;
        if (index == 0U) {
            last.domain = 2U;
        } else if (index == 1U) {
            last.epoch = 2U;
        } else if (index == 2U) {
            last.ticks_per_second = 1000U;
        } else {
            last.ticks = 9U;
        }
        nx_io_timestamp_delta_t result{777U, 888U, 999U, true};
        EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, &result),
                  NX_ERROR_STATE);
        EXPECT_EQ(result.ticks, 777U);
    }
}

TEST(IOTimestamp, RejectsNullMalformedAndUncertaintyOverflow) {
    auto first = event(10U, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    auto last = event(20U, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    nx_io_timestamp_delta_t result{777U, 888U, 999U, true};
    EXPECT_EQ(nx_io_timestamp_elapsed(nullptr, &last, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, nullptr, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, nullptr),
              NX_ERROR_INVALID);
    first.domain = 0U;
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, &result),
              NX_ERROR_INVALID);
    first = event(10U, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    first.epoch = 0U;
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, &result),
              NX_ERROR_INVALID);
    first = event(10U, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    first.ticks_per_second = 0U;
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, &result),
              NX_ERROR_INVALID);
    first = event(10U, static_cast<nx_io_timestamp_source_t>(7));
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, &result),
              NX_ERROR_INVALID);
    first = event(10U, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    first.uncertainty_ticks = UINT64_MAX;
    last.uncertainty_ticks = 1U;
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, &result),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(result.ticks, 777U);
}

TEST(IOTimestamp, AliasedResultCannotRewriteAnInput) {
    auto first = event(10U, NX_IO_TIMESTAMP_HARDWARE_EVENT);
    auto last = event(20U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    auto* output = reinterpret_cast<nx_io_timestamp_delta_t*>(&first);
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, output), NX_ERROR_INVALID);
    EXPECT_EQ(first.ticks, 10U);
    output = reinterpret_cast<nx_io_timestamp_delta_t*>(&last);
    EXPECT_EQ(nx_io_timestamp_elapsed(&first, &last, output), NX_ERROR_INVALID);
    EXPECT_EQ(last.ticks, 20U);
}

TEST(IOTimestampBudget, OnlyTheCompleteErrorIntervalCanMeetTheLimit) {
    auto first = event(100U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    auto last = event(110U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    first.uncertainty_ticks = 2U;
    last.uncertainty_ticks = 3U;
    nx_io_timestamp_budget_result_t result{};
    ASSERT_EQ(nx_io_timestamp_budget(&first, &last, 15U, &result), NX_SUCCESS);
    EXPECT_EQ(result.verdict, NX_IO_TIMESTAMP_BUDGET_WITHIN);
    EXPECT_EQ(result.lower_ticks, 5U);
    EXPECT_EQ(result.upper_ticks, 15U);
    EXPECT_EQ(result.delta.ticks, 10U);
    EXPECT_EQ(result.delta.ticks_per_second, 1000000U);
}

TEST(IOTimestampBudget, ErrorOverlapIsUnknownRatherThanAPassedOrFailedBudget) {
    auto first = event(100U, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    auto last = event(110U, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    last.uncertainty_ticks = 4U;
    nx_io_timestamp_budget_result_t result{};
    ASSERT_EQ(nx_io_timestamp_budget(&first, &last, 6U, &result), NX_SUCCESS);
    EXPECT_EQ(result.verdict, NX_IO_TIMESTAMP_BUDGET_UNKNOWN);
    ASSERT_EQ(nx_io_timestamp_budget(&first, &last, 5U, &result), NX_SUCCESS);
    EXPECT_EQ(result.verdict, NX_IO_TIMESTAMP_BUDGET_EXCEEDED);
    ASSERT_EQ(nx_io_timestamp_budget(&first, &last, 14U, &result), NX_SUCCESS);
    EXPECT_EQ(result.verdict, NX_IO_TIMESTAMP_BUDGET_WITHIN);
}

TEST(IOTimestampBudget, MissingAnErrorBoundCannotFabricateAnExactMeasurement) {
    auto first = event(100U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    auto last = event(110U, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    last.uncertainty_known = false;
    last.uncertainty_ticks = UINT64_MAX;
    nx_io_timestamp_budget_result_t result{};
    ASSERT_EQ(nx_io_timestamp_budget(&first, &last, 1U, &result), NX_SUCCESS);
    EXPECT_EQ(result.verdict, NX_IO_TIMESTAMP_BUDGET_UNKNOWN);
    EXPECT_FALSE(result.delta.uncertainty_known);
    EXPECT_EQ(result.lower_ticks, 0U);
    EXPECT_EQ(result.upper_ticks, 0U);
}

TEST(IOTimestampBudget, ExtendedTicksKeepWrapsWithoutExtendingRawDwtSnapshots) {
    auto first = event(UINT32_MAX - 10U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    auto last = event(first.ticks + (UINT64_C(1) << 34U) + 30U,
                      NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    nx_io_timestamp_budget_result_t result{};
    ASSERT_EQ(nx_io_timestamp_budget(&first, &last, UINT64_MAX, &result),
              NX_SUCCESS);
    EXPECT_EQ(result.delta.ticks, (UINT64_C(1) << 34U) + 30U);
    EXPECT_EQ(result.lower_ticks, result.delta.ticks);
    EXPECT_EQ(result.upper_ticks, result.delta.ticks);
    EXPECT_EQ(result.verdict, NX_IO_TIMESTAMP_BUDGET_WITHIN);
}

TEST(IOTimestampBudget, APhysicalDurationLowerBoundNeverUnderflows) {
    auto first = event(10U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    auto last = event(11U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    first.uncertainty_ticks = 5U;
    nx_io_timestamp_budget_result_t result{};
    ASSERT_EQ(nx_io_timestamp_budget(&first, &last, 0U, &result), NX_SUCCESS);
    EXPECT_EQ(result.lower_ticks, 0U);
    EXPECT_EQ(result.upper_ticks, 6U);
    EXPECT_EQ(result.verdict, NX_IO_TIMESTAMP_BUDGET_UNKNOWN);
}

TEST(IOTimestampBudget, OverflowDoesNotBecomeASmallDurationOrOverwriteOutput) {
    auto first = event(0U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    auto last = event(UINT64_MAX, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    last.uncertainty_ticks = 1U;
    nx_io_timestamp_budget_result_t result{};
    result.lower_ticks = 777U;
    EXPECT_EQ(nx_io_timestamp_budget(&first, &last, UINT64_MAX, &result),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(result.lower_ticks, 777U);
    first.uncertainty_ticks = UINT64_MAX;
    EXPECT_EQ(nx_io_timestamp_budget(&first, &last, 0U, &result),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(result.lower_ticks, 777U);
}

TEST(IOTimestampBudget,
     ChangedDomainsEpochsRatesOrReverseTimeCannotBeCompared) {
    auto first = event(10U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    const auto original = event(20U, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    for (unsigned index = 0U; index != 4U; ++index) {
        auto last = original;
        if (index == 0U)
            last.domain = 2U;
        else if (index == 1U)
            last.epoch = 2U;
        else if (index == 2U)
            last.ticks_per_second = 1000U;
        else
            last.ticks = 9U;
        nx_io_timestamp_budget_result_t result{};
        result.lower_ticks = 777U;
        EXPECT_EQ(nx_io_timestamp_budget(&first, &last, 0U, &result),
                  NX_ERROR_STATE);
        EXPECT_EQ(result.lower_ticks, 777U);
    }
}

TEST(IOTimestampBudget, UnknownZeroRateAndAliasedStorageRetainNoResult) {
    auto first = event(10U, NX_IO_TIMESTAMP_UNKNOWN);
    auto last = event(20U, NX_IO_TIMESTAMP_IRQ_OBSERVATION);
    nx_io_timestamp_budget_result_t result{};
    result.lower_ticks = 777U;
    EXPECT_EQ(nx_io_timestamp_budget(&first, &last, 100U, &result),
              NX_ERROR_UNSUPPORTED);
    first.source = NX_IO_TIMESTAMP_HARDWARE_EVENT;
    first.ticks_per_second = 0U;
    EXPECT_EQ(nx_io_timestamp_budget(&first, &last, 100U, &result),
              NX_ERROR_INVALID);
    first.ticks_per_second = 1000000U;
    EXPECT_EQ(nx_io_timestamp_budget(nullptr, &last, 0U, &result),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_io_timestamp_budget(&first, &last, 0U, nullptr),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_io_timestamp_budget(
                  &first, &last, 0U,
                  reinterpret_cast<nx_io_timestamp_budget_result_t*>(&first)),
              NX_ERROR_INVALID);
    EXPECT_EQ(first.ticks, 10U);
    EXPECT_EQ(result.lower_ticks, 777U);
}

class ServicePort {
  public:
    MOCK_METHOD(void, Service, ());
    static void service(void* context) {
        static_cast<ServicePort*>(context)->Service();
    }
};

TEST(IOCheckedService, RejectsInvalidFacesBeforeProviderAccess) {
    nx_uart_port_t uart{};
    nx_spi_port_t spi{};
    nx_i2c_port_t i2c{};
    EXPECT_EQ(nx_uart_port_service_checked(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_spi_port_service_checked(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_port_service_checked(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_uart_port_service_checked(&uart), NX_ERROR_INVALID);
    EXPECT_EQ(nx_spi_port_service_checked(&spi), NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_port_service_checked(&i2c), NX_ERROR_INVALID);
}

TEST(IOCheckedService, DistinguishesUnsupportedFromDispatched) {
    testing::StrictMock<ServicePort> owner;
    nx_uart_ops_t uart_ops{};
    nx_spi_ops_t spi_ops{};
    nx_i2c_ops_t i2c_ops{};
    nx_uart_port_t uart{&uart_ops, &owner};
    nx_spi_port_t spi{&spi_ops, &owner};
    nx_i2c_port_t i2c{&i2c_ops, &owner};
    EXPECT_EQ(nx_uart_port_service_checked(&uart), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_spi_port_service_checked(&spi), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_i2c_port_service_checked(&i2c), NX_ERROR_UNSUPPORTED);
    uart_ops.service = ServicePort::service;
    spi_ops.service = ServicePort::service;
    i2c_ops.service = ServicePort::service;
    EXPECT_CALL(owner, Service()).Times(3);
    EXPECT_EQ(nx_uart_port_service_checked(&uart), NX_SUCCESS);
    EXPECT_EQ(nx_spi_port_service_checked(&spi), NX_SUCCESS);
    EXPECT_EQ(nx_i2c_port_service_checked(&i2c), NX_SUCCESS);
}

TEST(IOCheckedService, DispatchesExactIndependentContextOnce) {
    testing::StrictMock<ServicePort> first;
    testing::StrictMock<ServicePort> second;
    nx_uart_ops_t operations{};
    operations.service = ServicePort::service;
    nx_uart_port_t one{&operations, &first};
    nx_uart_port_t two{&operations, &second};
    EXPECT_CALL(first, Service()).Times(1);
    EXPECT_CALL(second, Service()).Times(1);
    EXPECT_EQ(nx_uart_port_service_checked(&one), NX_SUCCESS);
    EXPECT_EQ(nx_uart_port_service_checked(&two), NX_SUCCESS);
}
} /* namespace */
