/**
 * \file            w25_test.cpp
 * \brief           W25 command framing, protection, busy duty and clock tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/w25.h"
#include <array>
#include <cstring>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

namespace {
using testing::_;
using testing::Invoke;
using testing::StrictMock;

/** \brief Model only settled whole-CS SPI exchanges and the external clock. */
class Bus {
  public:
    MOCK_METHOD(nx_result_t, Transfer,
                (const uint8_t*, uint8_t*, size_t, nx_time_us_t, size_t*));
    testing::Sequence order;
    nx_time_us_t now{10000U};
    static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                                size_t bytes, nx_time_us_t deadline,
                                size_t* transferred) {
        return static_cast<Bus*>(context)->Transfer(tx, rx, bytes, deadline,
                                                    transferred);
    }
    static nx_time_us_t read_clock(void* context) {
        return static_cast<Bus*>(context)->now;
    }
    static const nx_spi_endpoint_ops_t ops;
    nx_spi_endpoint_t endpoint{&ops, this};
    nx_clock_t clock() {
        return {read_clock, this};
    }
    void command(std::vector<uint8_t> bytes, std::vector<uint8_t> response = {},
                 nx_result_t result = NX_SUCCESS,
                 std::optional<size_t> count = {},
                 std::function<void()> after = {}) {
        EXPECT_CALL(*this, Transfer(_, _, bytes.size(), _, _))
            .InSequence(order)
            .WillOnce(Invoke([=](const uint8_t* tx, uint8_t* rx, size_t length,
                                 nx_time_us_t deadline, size_t* transferred) {
                EXPECT_NE(tx, nullptr);
                EXPECT_NE(transferred, nullptr);
                EXPECT_NE(deadline, NX_DEADLINE_NEVER);
                EXPECT_EQ(std::memcmp(tx, bytes.data(), length), 0);
                if (response.empty()) {
                    EXPECT_EQ(rx, nullptr);
                } else {
                    EXPECT_NE(rx, nullptr);
                    EXPECT_EQ(response.size(), length);
                    if (rx != nullptr) {
                        std::memcpy(rx, response.data(), response.size());
                    }
                }
                *transferred =
                    count.value_or(result == NX_SUCCESS ? length : size_t{0U});
                if (after) {
                    after();
                }
                return result;
            }));
    }
    void status1(uint8_t status, nx_result_t result = NX_SUCCESS) {
        command({0x05U, 0xffU}, {0U, status}, result);
    }
    void status2(uint8_t status, nx_result_t result = NX_SUCCESS) {
        command({0x35U, 0xffU}, {0U, status}, result);
    }
    void status3(uint8_t status) {
        command({0x15U, 0xffU}, {0U, status});
    }
    void identity(uint32_t id = NX_W25_Q128JV_ID) {
        command({0x9fU, 0xffU, 0xffU, 0xffU},
                {0U, static_cast<uint8_t>(id >> 16U),
                 static_cast<uint8_t>(id >> 8U), static_cast<uint8_t>(id)});
    }
    void initialize() {
        status1(0U);
        status2(0U);
        identity();
        status3(0U);
    }
    void writable(uint8_t sr1 = 0U, uint8_t sr2 = 0U, uint8_t sr3 = 0U) {
        status1(sr1);
        status2(sr2);
        status3(sr3);
    }
    void arm() {
        command({0x06U});
        status1(2U);
    }
    void quiescent(uint8_t wel = 0U) {
        status1(wel);
        status2(0U);
        if (wel != 0U) {
            command({0x04U});
            status1(0U);
        }
    }
};

const nx_spi_endpoint_ops_t Bus::ops{Bus::transfer, nullptr, nullptr, nullptr,
                                     nullptr};

class W25 : public testing::Test {
  protected:
    StrictMock<Bus> bus;
    nx_w25_t device{};
    nx_w25_config_t config{
        NX_W25_PROFILE_Q128JV, 0x10000U, 0x20000U, 7U, 3U, 5000U};
    std::array<uint8_t, 3> data{{0x12U, 0x34U, 0x56U}};
    void initialize() {
        bus.initialize();
        ASSERT_EQ(
            nx_w25_init(&device, &bus.endpoint, bus.clock(), &config, 100000U),
            NX_SUCCESS);
    }
    void program(nx_time_us_t deadline = 100000U,
                 nx_result_t result = NX_SUCCESS,
                 std::function<void()> after = {}) {
        bus.writable();
        bus.arm();
        bus.command({0x02U, 0x01U, 0x00U, 0x10U, 0x12U, 0x34U, 0x56U}, {},
                    result, {}, after);
        EXPECT_EQ(nx_w25_program_start(&device, 0x10U, data.data(), data.size(),
                                       deadline),
                  result);
    }
    nx_w25_status_t status() {
        nx_w25_status_t result{};
        EXPECT_EQ(nx_w25_get_status(&device, &result), NX_SUCCESS);
        return result;
    }
};
} /* namespace */

TEST_F(W25, ProbeRequiresExplicitIdentityAndAnIdleNonSuspendedChip) {
    initialize();
    auto observed = status();
    EXPECT_TRUE(observed.initialized);
    EXPECT_FALSE(observed.pending);
    EXPECT_EQ(observed.operation, NX_W25_OPERATION_NONE);
    EXPECT_EQ(nx_w25_stop(&device), NX_SUCCESS);
}

TEST_F(W25, Q64AndOtherSameFamilyIdentitiesDoNotSelectADifferentProfile) {
    bus.status1(0U);
    bus.status2(0U);
    bus.identity(0xef4017U);
    EXPECT_EQ(
        nx_w25_init(&device, &bus.endpoint, bus.clock(), &config, 100000U),
        NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(device.initialized);
}

TEST_F(W25, InvalidModelRegionOrDeadlineProducesNoSpi) {
    for (unsigned which = 0U; which < 5U; ++which) {
        auto invalid = config;
        if (which == 0U) {
            invalid.profile = static_cast<nx_w25_profile_t>(0);
        } else if (which == 1U) {
            invalid.region_offset = 1U;
        } else if (which == 2U) {
            invalid.region_bytes = 0U;
        } else if (which == 3U) {
            invalid.region_offset = NX_W25_Q128JV_BYTES;
        } else {
            invalid.region_bytes = UINT32_MAX;
        }
        EXPECT_EQ(
            nx_w25_init(&device, &bus.endpoint, bus.clock(), &invalid, 100000U),
            NX_ERROR_INVALID);
    }
    EXPECT_EQ(nx_w25_init(&device, &bus.endpoint, bus.clock(), &config,
                          NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
}

TEST_F(W25, PowerReadyAtIsAnExplicitNoIoGate) {
    bus.now = 4000U;
    EXPECT_EQ(
        nx_w25_init(&device, &bus.endpoint, bus.clock(), &config, 100000U),
        NX_ERROR_BUSY);
    EXPECT_FALSE(device.initialized);
}

TEST_F(W25, BusyOrSuspendedProbeNeverIssuesWakeResetOrWriteEnable) {
    bus.status1(1U);
    EXPECT_EQ(
        nx_w25_init(&device, &bus.endpoint, bus.clock(), &config, 100000U),
        NX_ERROR_BUSY);
    bus.status1(0U);
    bus.status2(0x80U);
    EXPECT_EQ(
        nx_w25_init(&device, &bus.endpoint, bus.clock(), &config, 100000U),
        NX_ERROR_BUSY);
}

TEST_F(W25, MissingPollingMethodAndInvalidClockAreRejectedWithoutIo) {
    const nx_spi_endpoint_ops_t empty{};
    const nx_spi_endpoint_t absent{&empty, &bus};
    EXPECT_EQ(nx_w25_init(&device, &absent, bus.clock(), &config, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_init(&device, &bus.endpoint, {}, &config, 100000U),
              NX_ERROR_INVALID);
}

TEST_F(W25, ReadUses03AndOneWholeCsWithBigEndianPhysicalAddress) {
    initialize();
    bus.command({0x03U, 0x01U, 0x23U, 0x45U, 0xffU, 0xffU, 0xffU},
                {0U, 0U, 0U, 0U, 1U, 2U, 3U});
    std::array<uint8_t, 3> output{};
    EXPECT_EQ(
        nx_w25_read(&device, 0x2345U, output.data(), output.size(), 100000U),
        NX_SUCCESS);
    EXPECT_EQ(output, (std::array<uint8_t, 3>{{1U, 2U, 3U}}));
}

TEST_F(W25, FailedReadDoesNotPublishPartialOutput) {
    initialize();
    bus.command({0x03U, 0x01U, 0U, 0U, 0xffU, 0xffU, 0xffU},
                {0U, 0U, 0U, 0U, 1U, 2U, 3U}, NX_ERROR_IO, 5U);
    std::array<uint8_t, 3> output{{9U, 9U, 9U}};
    EXPECT_EQ(nx_w25_read(&device, 0U, output.data(), output.size(), 100000U),
              NX_ERROR_IO);
    EXPECT_EQ(output, (std::array<uint8_t, 3>{{9U, 9U, 9U}}));
}

TEST_F(W25, ShortSuccessfulReadIsAnIoFailureWithoutOutputPublication) {
    initialize();
    bus.command({0x03U, 0x01U, 0U, 0U, 0xffU}, {0U, 0U, 0U, 0U, 1U}, NX_SUCCESS,
                4U);
    uint8_t output = 9U;
    EXPECT_EQ(nx_w25_read(&device, 0U, &output, 1U, 100000U), NX_ERROR_IO);
    EXPECT_EQ(output, 9U);
}

TEST_F(W25, ReadRangeAndKnownAliasesAreRejectedBeforeSpi) {
    initialize();
    std::array<uint8_t, NX_W25_MAX_CHUNK_BYTES + 1U> output{};
    EXPECT_EQ(
        nx_w25_read(&device, config.region_bytes, output.data(), 1U, 100000U),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_read(&device, UINT32_MAX, output.data(), 1U, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_read(&device, 0U, output.data(), output.size(), 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_read(&device, 0U, reinterpret_cast<uint8_t*>(&device), 1U,
                          100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_read(&device, 0U,
                          reinterpret_cast<uint8_t*>(&bus.endpoint), 1U,
                          100000U),
              NX_ERROR_INVALID);
}

TEST_F(W25, ProgramRetainsDeviceDutyUntilBusyAndSuspendAreGone) {
    initialize();
    program();
    EXPECT_TRUE(status().pending);
    EXPECT_TRUE(status().may_have_modified);
    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
    bus.status1(1U);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_BUSY);
    bus.quiescent();
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_SUCCESS);
    EXPECT_FALSE(status().pending);
    EXPECT_EQ(status().operation, NX_W25_OPERATION_PROGRAM);
    EXPECT_EQ(nx_w25_stop(&device), NX_SUCCESS);
}

TEST_F(W25, MaximumProgramPayloadFitsExactlyOne256ByteTransaction) {
    initialize();
    std::array<uint8_t, NX_W25_MAX_CHUNK_BYTES> payload{};
    std::vector<uint8_t> command(4U + payload.size(), 0U);
    command[0] = 0x02U;
    command[1] = 0x01U;
    bus.writable();
    bus.arm();
    bus.command(command);
    EXPECT_EQ(nx_w25_program_start(&device, 0U, payload.data(), payload.size(),
                                   100000U),
              NX_SUCCESS);
    EXPECT_TRUE(status().pending);
}

TEST_F(W25, InvalidProgramSpansNeverWriteEnableOrWrapThePage) {
    initialize();
    std::array<uint8_t, 253> payload{};
    EXPECT_EQ(nx_w25_program_start(&device, 0U, payload.data(), payload.size(),
                                   100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_program_start(&device, 255U, data.data(), 2U, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_program_start(&device, config.region_bytes, data.data(),
                                   1U, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_program_start(&device, 0U,
                                   reinterpret_cast<const uint8_t*>(&device),
                                   1U, 100000U),
              NX_ERROR_INVALID);
}

TEST_F(W25, SectorEraseUses20AndOnlyTheDeclaredAlignedRegion) {
    initialize();
    EXPECT_EQ(nx_w25_erase_4k_start(&device, 1U, 100000U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_erase_4k_start(&device, config.region_bytes, 100000U),
              NX_ERROR_INVALID);
    bus.writable();
    bus.arm();
    bus.command({0x20U, 0x01U, 0x10U, 0U});
    EXPECT_EQ(nx_w25_erase_4k_start(&device, 0x1000U, 100000U), NX_SUCCESS);
    EXPECT_EQ(status().operation, NX_W25_OPERATION_ERASE_4K);
    EXPECT_EQ(status().bytes, NX_W25_SECTOR_BYTES);
}

TEST_F(W25, ExistingProtectionIsRejectedWithoutUnlockOrStatusWrites) {
    initialize();
    for (const auto& flags : {std::array<uint8_t, 3>{{4U, 0U, 0U}},
                              std::array<uint8_t, 3>{{0U, 0x40U, 0U}},
                              std::array<uint8_t, 3>{{0U, 0U, 4U}}}) {
        bus.writable(flags[0], flags[1], flags[2]);
        EXPECT_EQ(nx_w25_program_start(&device, 0U, data.data(), data.size(),
                                       100000U),
                  NX_ERROR_PERMISSION);
        EXPECT_FALSE(status().pending);
    }
}

TEST_F(W25, AWriteEnableThatDoesNotLatchNeverIssuesProgram) {
    initialize();
    bus.writable();
    bus.command({0x06U});
    bus.status1(0U);
    EXPECT_EQ(
        nx_w25_program_start(&device, 0U, data.data(), data.size(), 100000U),
        NX_ERROR_PERMISSION);
    EXPECT_FALSE(status().pending);
    EXPECT_FALSE(status().may_have_modified);
}

TEST_F(W25, UncertainWriteEnableRetainsStateUntilObservedDisarmed) {
    initialize();
    bus.writable();
    bus.command({0x06U}, {}, NX_ERROR_IO);
    EXPECT_EQ(
        nx_w25_program_start(&device, 0U, data.data(), data.size(), 100000U),
        NX_ERROR_IO);
    EXPECT_TRUE(status().pending);
    EXPECT_FALSE(status().may_have_modified);
    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
    bus.quiescent(2U);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
    EXPECT_FALSE(status().pending);
    EXPECT_EQ(nx_w25_stop(&device), NX_SUCCESS);
}

TEST_F(W25, FailedProgramMayHaveChangedFlashAndNeverReplaysBytes) {
    initialize();
    program(100000U, NX_ERROR_IO);
    EXPECT_TRUE(status().uncertain);
    EXPECT_TRUE(status().may_have_modified);
    bus.status1(1U);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_BUSY);
    bus.quiescent();
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
    EXPECT_FALSE(status().pending);
}

TEST_F(W25, FailedEraseRetainsTheSameSectorDutyUntilQuiescent) {
    initialize();
    bus.writable();
    bus.arm();
    bus.command({0x20U, 0x01U, 0U, 0U}, {}, NX_ERROR_IO);
    EXPECT_EQ(nx_w25_erase_4k_start(&device, 0U, 100000U), NX_ERROR_IO);
    EXPECT_TRUE(status().pending);
    EXPECT_EQ(status().bytes, NX_W25_SECTOR_BYTES);
    bus.quiescent();
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
}

TEST_F(W25, LaterCleanupBudgetDoesNotExtendTheOriginalOperationDeadline) {
    initialize();
    program(10020U);
    bus.now = 10030U;
    bus.status1(1U);
    EXPECT_EQ(nx_w25_service(&device, 20000U), NX_ERROR_BUSY);
    EXPECT_EQ(status().result, NX_ERROR_TIMEOUT);
    bus.quiescent();
    EXPECT_EQ(nx_w25_service(&device, 30000U), NX_ERROR_TIMEOUT);
    EXPECT_FALSE(status().pending);
    EXPECT_EQ(nx_w25_stop(&device), NX_SUCCESS);
}

TEST_F(W25, ExpiredStartRejectsBeforeAnySpiAccess) {
    initialize();
    EXPECT_EQ(
        nx_w25_program_start(&device, 0U, data.data(), data.size(), bus.now),
        NX_ERROR_TIMEOUT);
    EXPECT_FALSE(status().pending);
}

TEST_F(W25, ExpiredCleanupCannotRevokePendingDuty) {
    initialize();
    program();
    EXPECT_EQ(nx_w25_service(&device, bus.now), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(status().pending);
    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
    bus.quiescent();
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_SUCCESS);
}

TEST_F(W25, SuspendWithBusyClearIsNotSettlementOrAnAutomaticResume) {
    initialize();
    program();
    bus.status1(0U);
    bus.status2(0x80U);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_STATE);
    EXPECT_TRUE(status().pending);
    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
}

TEST_F(W25, IdleWelAfterAProgramIsDisarmedAndReportedAsFailed) {
    initialize();
    program();
    bus.quiescent(2U);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
    EXPECT_FALSE(status().pending);
}

TEST_F(W25, PendingDutyBlocksReadsAndAnotherMutatingCommandWithoutIo) {
    initialize();
    program();
    uint8_t output{};
    EXPECT_EQ(nx_w25_read(&device, 0U, &output, 1U, 100000U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_w25_erase_4k_start(&device, 0U, 100000U), NX_ERROR_BUSY);
    EXPECT_EQ(
        nx_w25_program_start(&device, 0U, data.data(), data.size(), 100000U),
        NX_ERROR_BUSY);
}

TEST_F(W25, ClockRegressionDuringProgramKeepsPendingStorageAndStopsNewIo) {
    initialize();
    bus.writable();
    bus.arm();
    bus.command({0x02U, 0x01U, 0U, 0U, 0x12U, 0x34U, 0x56U}, {}, NX_SUCCESS, {},
                [&] { bus.now = 9999U; });
    EXPECT_EQ(
        nx_w25_program_start(&device, 0U, data.data(), data.size(), 100000U),
        NX_ERROR_IO);
    EXPECT_TRUE(status().clock_fault);
    EXPECT_TRUE(status().pending);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
}

TEST_F(W25, AClockFaultDuringReadBlocksWorkButCannotInventArrayDuty) {
    initialize();
    bus.command({0x03U, 0x01U, 0U, 0U, 0xffU}, {0U, 0U, 0U, 0U, 1U}, NX_SUCCESS,
                {}, [&] { bus.now = 9999U; });
    uint8_t output = 9U;
    EXPECT_EQ(nx_w25_read(&device, 0U, &output, 1U, 100000U), NX_ERROR_IO);
    EXPECT_EQ(output, 9U);
    EXPECT_FALSE(status().pending);
    EXPECT_EQ(nx_w25_stop(&device), NX_SUCCESS);
}

TEST_F(W25, ReentrantTransportCallbacksCannotStartOrStopAnotherOperation) {
    initialize();
    bus.writable();
    bus.arm();
    bus.command({0x02U, 0x01U, 0U, 0U, 0x12U, 0x34U, 0x56U}, {}, NX_SUCCESS, {},
                [&] {
                    uint8_t output{};
                    EXPECT_EQ(nx_w25_read(&device, 0U, &output, 1U, 100000U),
                              NX_ERROR_BUSY);
                    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
                });
    EXPECT_EQ(
        nx_w25_program_start(&device, 0U, data.data(), data.size(), 100000U),
        NX_SUCCESS);
}

TEST_F(W25, ASecondBindingHasNoSharedPendingOrClockState) {
    initialize();
    program();
    StrictMock<Bus> second;
    nx_w25_t independent{};
    second.initialize();
    ASSERT_EQ(nx_w25_init(&independent, &second.endpoint, second.clock(),
                          &config, 100000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_w25_stop(&independent), NX_SUCCESS);
    EXPECT_TRUE(status().pending);
}

TEST_F(W25, ReinitializationCannotEraseAStillLiveOperation) {
    initialize();
    program();
    auto replacement = config;
    replacement.region_offset = 0U;
    EXPECT_EQ(
        nx_w25_init(&device, &bus.endpoint, bus.clock(), &replacement, 100000U),
        NX_ERROR_BUSY);
    EXPECT_TRUE(status().pending);
    EXPECT_EQ(device.config.region_offset, config.region_offset);
}

TEST_F(W25, CallerProgramPayloadIsNotBorrowedAfterTheSynchronousReturn) {
    initialize();
    {
        std::array<uint8_t, 1> temporary{{0x5aU}};
        bus.writable();
        bus.arm();
        bus.command({0x02U, 0x01U, 0U, 0U, 0x5aU});
        ASSERT_EQ(nx_w25_program_start(&device, 0U, temporary.data(),
                                       temporary.size(), 100000U),
                  NX_SUCCESS);
        temporary[0] = 0U;
    }
    bus.quiescent();
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_SUCCESS);
}

TEST_F(W25, StatusOutputCannotAliasOrCorruptItsControlState) {
    initialize();
    EXPECT_EQ(
        nx_w25_get_status(&device, reinterpret_cast<nx_w25_status_t*>(&device)),
        NX_ERROR_INVALID);
    EXPECT_TRUE(device.initialized);
}

TEST_F(W25, UnexpectedObservedBusyClosesAdmissionUntilQuiescent) {
    initialize();
    bus.status1(1U);
    EXPECT_EQ(
        nx_w25_program_start(&device, 0U, data.data(), data.size(), 100000U),
        NX_ERROR_BUSY);
    uint8_t output{};
    EXPECT_EQ(nx_w25_read(&device, 0U, &output, 1U, 100000U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
    bus.quiescent();
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_STATE);
    EXPECT_FALSE(status().pending);
}

TEST_F(W25, IdleServiceAndComponentStopDoNotStopOrResetTheSharedSpiBus) {
    initialize();
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_SUCCESS);
    EXPECT_EQ(nx_w25_stop(&device), NX_SUCCESS);
    EXPECT_EQ(nx_w25_stop(&device), NX_SUCCESS);
}

TEST_F(W25, ADeadlineAfterWriteEnableRetainsOnlyArmedNotPayloadDuty) {
    initialize();
    bus.writable();
    bus.command({0x06U}, {}, NX_SUCCESS, {}, [&] { bus.now = 100000U; });
    EXPECT_EQ(
        nx_w25_program_start(&device, 0U, data.data(), data.size(), 100000U),
        NX_ERROR_TIMEOUT);
    EXPECT_TRUE(status().pending);
    EXPECT_FALSE(status().may_have_modified);
    bus.quiescent(2U);
    EXPECT_EQ(nx_w25_service(&device, 200000U), NX_ERROR_TIMEOUT);
    EXPECT_FALSE(status().pending);
}

TEST_F(W25, FailedStatusProbeRetainsDutyWithoutInventingAFailedArrayCommand) {
    initialize();
    program();
    bus.status1(0U, NX_ERROR_IO);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
    EXPECT_TRUE(status().pending);
    EXPECT_TRUE(status().uncertain);
    EXPECT_EQ(status().result, NX_ERROR_BUSY);
    bus.quiescent();
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_SUCCESS);
}

TEST_F(W25, FailedWriteDisableDoesNotProveIdleRelease) {
    initialize();
    program();
    bus.status1(2U);
    bus.status2(0U);
    bus.command({0x04U}, {}, NX_ERROR_IO);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
    bus.quiescent(2U);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
    EXPECT_FALSE(status().pending);
}

TEST_F(W25, RefusedWriteDisableReadbackKeepsTheArmedBinding) {
    initialize();
    program();
    bus.status1(2U);
    bus.status2(0U);
    bus.command({0x04U});
    bus.status1(2U);
    EXPECT_EQ(nx_w25_service(&device, 100000U), NX_ERROR_IO);
    EXPECT_TRUE(status().pending);
    EXPECT_EQ(nx_w25_stop(&device), NX_ERROR_BUSY);
}

TEST_F(W25, Highest24BitByteIsAddressedWithoutTruncationOrWrap) {
    config.region_offset = NX_W25_Q128JV_BYTES - NX_W25_SECTOR_BYTES;
    config.region_bytes = NX_W25_SECTOR_BYTES;
    initialize();
    bus.command({0x03U, 0xffU, 0xffU, 0xffU, 0xffU}, {0U, 0U, 0U, 0U, 0x5aU});
    uint8_t output{};
    EXPECT_EQ(
        nx_w25_read(&device, NX_W25_SECTOR_BYTES - 1U, &output, 1U, 100000U),
        NX_SUCCESS);
    EXPECT_EQ(output, 0x5aU);
}

TEST_F(W25, InitialArmedOrExhaustedClockDoesNotPublishABinding) {
    bus.status1(2U);
    EXPECT_EQ(
        nx_w25_init(&device, &bus.endpoint, bus.clock(), &config, 100000U),
        NX_ERROR_BUSY);
    bus.now = NX_DEADLINE_NEVER;
    EXPECT_EQ(
        nx_w25_init(&device, &bus.endpoint, bus.clock(), &config, 100000U),
        NX_ERROR_IO);
    EXPECT_FALSE(device.initialized);
}

TEST_F(W25, MutationSourceCannotAliasTheImmutableDispatchTable) {
    initialize();
    const auto* bytes = reinterpret_cast<const uint8_t*>(&Bus::ops);
    EXPECT_EQ(nx_w25_program_start(&device, 0U, bytes, 1U, 100000U),
              NX_ERROR_INVALID);
}

TEST_F(W25, InitializationRejectsKnownControlAliasesWithoutChangingState) {
    EXPECT_EQ(nx_w25_init(&device, &bus.endpoint, bus.clock(),
                          reinterpret_cast<const nx_w25_config_t*>(&device),
                          100000U),
              NX_ERROR_INVALID);
    EXPECT_FALSE(device.initialized);
    EXPECT_FALSE(device.entered);
}

TEST_F(W25, ReadAndProgramRejectTheKnownEndpointContextAnchorBeforeSpi) {
    initialize();
    auto* anchor = reinterpret_cast<uint8_t*>(bus.endpoint.context);
    EXPECT_EQ(nx_w25_read(&device, 0U, anchor, 1U, 100000U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_program_start(&device, 0U, anchor, 1U, 100000U),
              NX_ERROR_INVALID);
    EXPECT_FALSE(device.pending);
}

TEST_F(W25, ReadAndProgramRejectTheSeparateClockContextAnchorBeforeSpi) {
    uint8_t clock_anchor{};
    nx_clock_t clock{+[](void*) { return nx_time_us_t{10000U}; },
                     &clock_anchor};
    bus.initialize();
    ASSERT_EQ(nx_w25_init(&device, &bus.endpoint, clock, &config, 100000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_w25_read(&device, 0U, &clock_anchor, 1U, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_w25_program_start(&device, 0U, &clock_anchor, 1U, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(clock_anchor, 0U);
    EXPECT_FALSE(device.pending);
}

TEST_F(W25, InitRejectsOwnerOverTheKnownBusContextBeforeCallingIt) {
    static unsigned transfers;
    transfers = 0U;
    const nx_spi_endpoint_ops_t ops{
        +[](void*, const uint8_t*, uint8_t*, size_t, nx_time_us_t, size_t*) {
            ++transfers;
            return NX_ERROR_IO;
        },
        nullptr, nullptr, nullptr, nullptr};
    const nx_spi_endpoint_t endpoint{&ops, &device};
    EXPECT_EQ(nx_w25_init(&device, &endpoint, bus.clock(), &config, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(transfers, 0U);
    EXPECT_FALSE(device.entered);
    EXPECT_FALSE(device.initialized);
}

TEST_F(W25, InitRejectsOwnerOverTheKnownClockContextBeforeCallingIt) {
    static unsigned reads;
    reads = 0U;
    nx_clock_t clock{+[](void*) {
                         ++reads;
                         return nx_time_us_t{10000U};
                     },
                     &device};
    EXPECT_EQ(nx_w25_init(&device, &bus.endpoint, clock, &config, 100000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(reads, 0U);
    EXPECT_FALSE(device.entered);
    EXPECT_FALSE(device.initialized);
}
