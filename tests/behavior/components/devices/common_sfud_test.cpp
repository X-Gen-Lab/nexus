/**
 * \file            common_sfud_test.cpp
 * \brief           Selected upstream NOR algorithms through whole-CS SPI
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/flashdb.h"
#include "nexus/components/sfud.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
namespace {
const nx_flashdb_operation_t* database_operation;

/** \brief Model external SPI replies and physical array effects only. */
class Spi {
  public:
    std::vector<uint8_t> bytes = std::vector<uint8_t>(16777216U, 0xffU);
    std::vector<uint8_t> commands;
    nx_time_us_t now{10U};
    nx_time_us_t clock_tick_us{0U};
    nx_time_us_t clock_quantum_us{1U};
    size_t clock_reads_per_tick{1U};
    nx_time_us_t transfer_time_us{0U};
    nx_time_us_t program_time_us{0U};
    nx_time_us_t erase_time_us{0U};
    nx_time_us_t busy_until{0U};
    size_t clock_reads{0U};
    nx_time_us_t after_read_time{NX_DEADLINE_NEVER};
    nx_time_us_t deadline{0U};
    uint8_t clock_command{0U};
    unsigned clock_occurrence{1U};
    unsigned matching_commands{0U};
    nx_time_us_t command_time{NX_DEADLINE_NEVER};
    bool defer_clock_until_after_check{false};
    bool clock_change_pending{false};
    uint8_t fail_command{0U};
    bool fail_before_effects{false};
    uint8_t status{0U};
    bool hold_busy{false};
    bool short_transfer{false};
    static nx_time_us_t time(void* context) {
        auto& self = *static_cast<Spi*>(context);
        ++self.clock_reads;
        nx_time_us_t observed = self.now - self.now % self.clock_quantum_us;
        if (self.clock_change_pending) {
            self.now = self.command_time;
            self.clock_change_pending = false;
        } else if (self.clock_reads % self.clock_reads_per_tick == 0U) {
            self.now += self.clock_tick_us;
        }
        return observed;
    }
    static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                                size_t bytes, nx_time_us_t deadline,
                                size_t* transferred) {
        auto& self = *static_cast<Spi*>(context);
        self.now += self.transfer_time_us;
        if (self.busy_until != 0U && self.now >= self.busy_until) {
            self.status &= static_cast<uint8_t>(~3U);
            self.busy_until = 0U;
        }
        self.commands.push_back(tx[0]);
        self.deadline = deadline;
        if (rx != nullptr) {
            std::fill_n(rx, bytes, 0U);
        }
        *transferred = self.short_transfer ? bytes - 1U : bytes;
        const bool fail = self.fail_command == tx[0];
        if (fail && self.fail_before_effects) {
            self.fail_command = 0U;
            *transferred = 0U;
            return NX_ERROR_IO;
        }
        switch (tx[0]) {
            case 0x9fU:
                if (rx != nullptr && bytes == 4U) {
                    rx[1] = 0xefU;
                    rx[2] = 0x40U;
                    rx[3] = 0x18U;
                }
                break;
            case 0x05U:
                if (rx != nullptr) {
                    rx[1] = self.status;
                }
                break;
            case 0x35U:
            case 0x15U:
                break;
            case 0x06U:
                if ((self.status & 1U) == 0U) {
                    self.status |= 2U;
                }
                break;
            case 0x04U:
                if ((self.status & 1U) == 0U) {
                    self.status &= static_cast<uint8_t>(~2U);
                }
                break;
            case 0x03U:
            case 0x02U:
            case 0x20U: {
                uint32_t address =
                    (uint32_t{tx[1]} << 16U) | (uint32_t{tx[2]} << 8U) | tx[3];
                if (tx[0] == 0x03U) {
                    if (rx != nullptr) {
                        std::memcpy(rx + 4U, self.bytes.data() + address,
                                    bytes - 4U);
                    }
                } else if (tx[0] == 0x02U) {
                    for (size_t index = 4U; index < bytes; ++index) {
                        self.bytes[address + index - 4U] &= tx[index];
                    }
                    self.status = self.hold_busy ? 1U : 0U;
                    if (self.program_time_us != 0U) {
                        self.status = 1U;
                        self.busy_until = self.now + self.program_time_us;
                    }
                } else {
                    std::fill_n(self.bytes.begin() + address, 4096U, 0xffU);
                    self.status = self.hold_busy ? 1U : 0U;
                    if (self.erase_time_us != 0U) {
                        self.status = 1U;
                        self.busy_until = self.now + self.erase_time_us;
                    }
                }
                break;
            }
            default:
                return NX_ERROR_UNSUPPORTED;
        }
        if (tx[0] == 0x03U && self.after_read_time != NX_DEADLINE_NEVER) {
            self.now = self.after_read_time;
        }
        if (tx[0] == self.clock_command &&
            ++self.matching_commands == self.clock_occurrence) {
            if (self.defer_clock_until_after_check) {
                self.clock_change_pending = true;
            } else {
                self.now = self.command_time;
            }
        }
        if (fail) {
            self.fail_command = 0U;
            return NX_ERROR_IO;
        }
        return NX_SUCCESS;
    }
    static const nx_spi_endpoint_ops_t ops;
    nx_spi_endpoint_t endpoint{&ops, this};
};
const nx_spi_endpoint_ops_t Spi::ops{Spi::transfer, nullptr, nullptr, nullptr,
                                     nullptr};
class SFUD : public testing::Test {
  protected:
    Spi bus;
    nx_sfud_t device{};
    std::vector<max_align_t> workspace;
    void SetUp() override {
        workspace.resize(
            (nx_sfud_workspace_bytes() + sizeof(max_align_t) - 1U) /
            sizeof(max_align_t));
    }
    void initialize() {
        ASSERT_EQ(nx_sfud_init(&device, &bus.endpoint, {Spi::time, &bus},
                               NX_SFUD_W25Q128JV, 0U, workspace.data(),
                               workspace.size() * sizeof(max_align_t), 1000U),
                  NX_SUCCESS);
    }
};
TEST_F(SFUD, ProbeNeverResetsOrChangesProtection) {
    initialize();
    EXPECT_THAT(bus.commands,
                testing::Each(testing::AnyOf(0x05U, 0x35U, 0x15U, 0x9fU)));
    const nx_flash_geometry_t* geometry =
        nx_flash_port_geometry(nx_sfud_flash(&device));
    ASSERT_NE(geometry, nullptr);
    EXPECT_EQ(geometry->size, 16777216U);
    EXPECT_EQ(geometry->program_unit, 1U);
    EXPECT_EQ(nx_sfud_stop(&device), NX_SUCCESS);
}
TEST_F(SFUD, RealUpstreamAlgorithmsReadProgramAndEraseWithinOneBudget) {
    initialize();
    const auto* port = nx_sfud_flash(&device);
    const std::array<uint8_t, 4> expected{1U, 2U, 3U, 4U};
    ASSERT_EQ(nx_flash_port_program(port, 256U, expected.data(),
                                    expected.size(), 1000U),
              NX_SUCCESS);
    std::array<uint8_t, 4> output{};
    ASSERT_EQ(nx_flash_port_read(port, 256U, output.data(), output.size()),
              NX_SUCCESS);
    EXPECT_EQ(output, expected);
    ASSERT_EQ(nx_flash_port_erase(port, 0U, 4096U, 1000U), NX_SUCCESS);
    EXPECT_EQ(bus.bytes[256U], 0xffU);
    EXPECT_EQ(bus.deadline, 1000U);
    EXPECT_EQ(nx_sfud_stop(&device), NX_SUCCESS);
}
TEST_F(SFUD, ShortProbeCannotPublishAWorkingFace) {
    bus.short_transfer = true;
    EXPECT_NE(nx_sfud_init(&device, &bus.endpoint, {Spi::time, &bus},
                           NX_SFUD_W25Q128JV, 0U, workspace.data(),
                           workspace.size() * sizeof(max_align_t), 1000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_sfud_flash(&device), nullptr);
}
TEST_F(SFUD, ArrayUncertaintyPreventsStopUntilBoundedCleanupProvesIdle) {
    initialize();
    bus.hold_busy = true;
    const auto* port = nx_sfud_flash(&device);
    const std::array<uint8_t, 4> value{1U, 2U, 3U, 4U};
    EXPECT_NE(
        nx_flash_port_program(port, 0U, value.data(), value.size(), 1000U),
        NX_SUCCESS);
    EXPECT_EQ(nx_sfud_stop(&device), NX_ERROR_BUSY);
    EXPECT_EQ(nx_sfud_service(&device, 2000U), NX_ERROR_BUSY);
    bus.status = 0U;
    EXPECT_EQ(nx_sfud_service(&device, 2000U), NX_SUCCESS);
    EXPECT_EQ(nx_sfud_stop(&device), NX_SUCCESS);
}

TEST_F(SFUD, ErasedInputUnitsNeverIssueAProgramPulse) {
    initialize();
    bus.commands.clear();
    const std::array<uint8_t, 4> erased{0xffU, 0xffU, 0xffU, 0xffU};
    ASSERT_EQ(nx_flash_port_program(nx_sfud_flash(&device), 0U, erased.data(),
                                    erased.size(), 1000U),
              NX_SUCCESS);
    EXPECT_THAT(bus.commands, testing::Not(testing::Contains(0x06U)));
    EXPECT_THAT(bus.commands, testing::Not(testing::Contains(0x02U)));
}
TEST_F(SFUD, ReadDeadlineIsExplicitAndNeverRenewedForEachExchange) {
    initialize();
    bus.now = 1500U;
    std::array<uint8_t, 4> output{};
    EXPECT_EQ(nx_flash_port_read(nx_sfud_flash(&device), 0U, output.data(),
                                 output.size()),
              NX_ERROR_TIMEOUT);
    ASSERT_EQ(nx_sfud_set_read_deadline(&device, 2000U), NX_SUCCESS);
    ASSERT_EQ(nx_flash_port_read(nx_sfud_flash(&device), 0U, output.data(),
                                 output.size()),
              NX_SUCCESS);
    EXPECT_EQ(bus.deadline, 2000U);
}

TEST_F(SFUD, ProviderRejectsInvalidPhysicalRangesBeforeAnySpiAccess) {
    initialize();
    bus.commands.clear();
    const auto* port = nx_sfud_flash(&device);
    std::array<uint8_t, 4> value{};
    EXPECT_EQ(nx_flash_port_read(port, UINT32_MAX, value.data(), value.size()),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_flash_port_program(port, 0U, value.data(), 0U, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_flash_port_erase(port, 1U, 4096U, 1000U), NX_ERROR_INVALID);
    EXPECT_TRUE(bus.commands.empty());
}

TEST_F(SFUD, WholeArrayEraseUsesOnlyTheSelectedSectorPrivilege) {
    initialize();
    bus.commands.clear();
    ASSERT_EQ(nx_flash_port_erase(nx_sfud_flash(&device), 0U, 16777216U, 1000U),
              NX_SUCCESS);
    EXPECT_THAT(bus.commands, testing::Not(testing::Contains(0xc7U)));
    EXPECT_EQ(std::count(bus.commands.begin(), bus.commands.end(), 0x20U),
              4096);
}

TEST_F(SFUD, LastReadCannotPublishAfterTheOperationDeadline) {
    initialize();
    bus.after_read_time = 1000U;
    std::array<uint8_t, 4> output{};
    EXPECT_EQ(nx_flash_port_read(nx_sfud_flash(&device), 0U, output.data(),
                                 output.size()),
              NX_ERROR_TIMEOUT);
}

TEST_F(SFUD, FinalProbeClockBoundaryCannotPublishAnInitializedFace) {
    for (bool after_check : {false, true}) {
        for (nx_time_us_t completion :
             {nx_time_us_t{1000U}, nx_time_us_t{9U}}) {
            Spi probe;
            nx_sfud_t candidate{};
            std::vector<max_align_t> state(workspace.size());
            probe.clock_command = 0x15U;
            probe.command_time = completion;
            probe.defer_clock_until_after_check = after_check;
            EXPECT_EQ(nx_sfud_init(&candidate, &probe.endpoint,
                                   {Spi::time, &probe}, NX_SFUD_W25Q128JV, 0U,
                                   state.data(),
                                   state.size() * sizeof(max_align_t), 1000U),
                      completion == 1000U ? NX_ERROR_TIMEOUT : NX_ERROR_STATE);
            EXPECT_EQ(nx_sfud_flash(&candidate), nullptr);
            EXPECT_FALSE(candidate.entered);
        }
    }
}

TEST_F(SFUD, FinalCleanupClockBoundaryPreservesUncertainOwnership) {
    for (bool after_check : {false, true}) {
        for (bool disarm : {false, true}) {
            for (nx_time_us_t completion :
                 {nx_time_us_t{2000U}, nx_time_us_t{9U}}) {
                Spi cleanup;
                nx_sfud_t candidate{};
                std::vector<max_align_t> state(workspace.size());
                ASSERT_EQ(nx_sfud_init(&candidate, &cleanup.endpoint,
                                       {Spi::time, &cleanup}, NX_SFUD_W25Q128JV,
                                       0U, state.data(),
                                       state.size() * sizeof(max_align_t),
                                       1000U),
                          NX_SUCCESS);
                cleanup.fail_command = 0x02U;
                cleanup.fail_before_effects = disarm;
                const std::array<uint8_t, 4> input{1U, 2U, 3U, 4U};
                ASSERT_EQ(nx_flash_port_program(nx_sfud_flash(&candidate), 0U,
                                                input.data(), input.size(),
                                                1000U),
                          NX_ERROR_IO);
                ASSERT_EQ(nx_sfud_stop(&candidate), NX_ERROR_BUSY);
                cleanup.clock_command = disarm ? 0x05U : 0x35U;
                cleanup.clock_occurrence = disarm ? 2U : 1U;
                cleanup.command_time = completion;
                cleanup.defer_clock_until_after_check = after_check;
                EXPECT_EQ(nx_sfud_service(&candidate, 2000U),
                          completion == 2000U ? NX_ERROR_TIMEOUT
                                              : NX_ERROR_STATE);
                EXPECT_TRUE(candidate.uncertain);
                EXPECT_TRUE(candidate.faulted);
                EXPECT_FALSE(candidate.entered);
                EXPECT_EQ(nx_sfud_stop(&candidate), NX_ERROR_BUSY);
            }
        }
    }
}

TEST_F(SFUD, ProgramWaitsForTimedWipWhenTheWholeDeadlineIsSufficient) {
    initialize();
    bus.clock_tick_us = 1U;
    bus.transfer_time_us = 1U;
    bus.program_time_us = 3000U;
    nx_time_us_t started = bus.now;
    const std::array<uint8_t, 4> input{1U, 2U, 3U, 4U};
    EXPECT_EQ(nx_flash_port_program(nx_sfud_flash(&device), 0U, input.data(),
                                    input.size(), 10000U),
              NX_SUCCESS);
    EXPECT_GE(bus.now - started, 3000U);
    EXPECT_LE(std::count(bus.commands.begin(), bus.commands.end(), 0x05U), 64);
}

TEST_F(SFUD, EraseWaitsForTypicalAndMaximumTimedWipWithoutFloodingSpi) {
    for (nx_time_us_t duration :
         {nx_time_us_t{45000U}, nx_time_us_t{400000U}}) {
        Spi medium;
        nx_sfud_t candidate{};
        std::vector<max_align_t> state(workspace.size());
        ASSERT_EQ(nx_sfud_init(&candidate, &medium.endpoint,
                               {Spi::time, &medium}, NX_SFUD_W25Q128JV, 0U,
                               state.data(), state.size() * sizeof(max_align_t),
                               1000U),
                  NX_SUCCESS);
        medium.clock_tick_us = 1U;
        medium.transfer_time_us = 1U;
        medium.erase_time_us = duration;
        nx_time_us_t started = medium.now;
        EXPECT_EQ(
            nx_flash_port_erase(nx_sfud_flash(&candidate), 0U, 4096U, 500000U),
            NX_SUCCESS);
        EXPECT_GE(medium.now - started, duration);
        EXPECT_LE(
            std::count(medium.commands.begin(), medium.commands.end(), 0x05U),
            450);
    }
}

TEST_F(SFUD, ShortDeadlineWaitsUntilExpiryAndKeepsTheBusyOwner) {
    initialize();
    bus.clock_tick_us = 1U;
    bus.transfer_time_us = 1U;
    bus.program_time_us = 3000U;
    const std::array<uint8_t, 4> input{1U, 2U, 3U, 4U};
    EXPECT_EQ(nx_flash_port_program(nx_sfud_flash(&device), 0U, input.data(),
                                    input.size(), 1000U),
              NX_ERROR_TIMEOUT);
    EXPECT_GE(bus.now, 1000U);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(nx_sfud_stop(&device), NX_ERROR_BUSY);
}

TEST_F(SFUD, StoppedClockCannotCreateAnUnboundedBusyWait) {
    initialize();
    bus.program_time_us = 3000U;
    const std::array<uint8_t, 4> input{1U, 2U, 3U, 4U};
    EXPECT_NE(nx_flash_port_program(nx_sfud_flash(&device), 0U, input.data(),
                                    input.size(), 1000000U),
              NX_SUCCESS);
    EXPECT_LT(bus.clock_reads, 2000000U);
    EXPECT_LT(bus.commands.size(), 1000U);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(nx_sfud_stop(&device), NX_ERROR_BUSY);
}

TEST_F(SFUD, TimedNorSupportsActualFalAndFlashDbCreateSetGet) {
    initialize();
    bus.clock_tick_us = 1U;
    bus.transfer_time_us = 1U;
    bus.program_time_us = 3000U;
    bus.erase_time_us = 400000U;
    constexpr nx_time_us_t deadline = 3000000U;
    ASSERT_EQ(nx_sfud_set_read_deadline(&device, deadline), NX_SUCCESS);
    nx_fal_t flash{};
    ASSERT_EQ(nx_fal_bind(&flash, {nx_sfud_flash(&device), 0U, 8192U, true},
                          {Spi::time, &bus}),
              NX_SUCCESS);
    nx_flashdb_t database{};
    std::vector<max_align_t> state(
        (nx_flashdb_workspace_bytes() + sizeof(max_align_t) - 1U) /
        sizeof(max_align_t));
    ASSERT_EQ(nx_flashdb_init(&database, &flash, NX_FLASHDB_KV, state.data(),
                              state.size() * sizeof(max_align_t), 0U),
              NX_SUCCESS);
    nx_flashdb_operation_t operation{&database, deadline};
    database_operation = &operation;
    nx_result_t opened =
        nx_flashdb_open(&operation, NX_FLASHDB_CREATE_IF_ERASED);
    EXPECT_EQ(opened, NX_SUCCESS);
    if (opened == NX_SUCCESS) {
        EXPECT_EQ(nx_flashdb_kv_set(&operation, "period", "20", 2U),
                  NX_SUCCESS);
        std::array<char, 4> output{};
        size_t bytes = output.size();
        EXPECT_EQ(
            nx_flashdb_kv_get(&operation, "period", output.data(), &bytes),
            NX_SUCCESS);
        EXPECT_EQ(bytes, 2U);
        EXPECT_EQ(std::memcmp(output.data(), "20", 2U), 0);
        EXPECT_GE(bus.now, 800000U);
    }
    EXPECT_EQ(nx_flashdb_close(&database), NX_SUCCESS);
    database_operation = nullptr;
}

TEST_F(SFUD, MillisecondQuantizedClockCanProgressAfterManyIdenticalSamples) {
    Spi medium;
    medium.now = 10000U;
    medium.clock_tick_us = 1U;
    medium.clock_quantum_us = 1000U;
    medium.clock_reads_per_tick = 100U;
    medium.transfer_time_us = 1U;
    medium.program_time_us = 3000U;
    nx_sfud_t candidate{};
    std::vector<max_align_t> state(workspace.size());
    ASSERT_EQ(nx_sfud_init(&candidate, &medium.endpoint, {Spi::time, &medium},
                           NX_SFUD_W25Q128JV, 0U, state.data(),
                           state.size() * sizeof(max_align_t), 100000U),
              NX_SUCCESS);
    const std::array<uint8_t, 4> input{1U, 2U, 3U, 4U};
    EXPECT_EQ(nx_flash_port_program(nx_sfud_flash(&candidate), 0U, input.data(),
                                    input.size(), 100000U),
              NX_SUCCESS);
    EXPECT_GE(medium.now, 13000U);
    EXPECT_LT(medium.clock_reads, 2000000U);
}
} /* namespace */

extern "C" const nx_flashdb_operation_t* nx_flashdb_operation_get(void) {
    return database_operation;
}
