/**
 * \file            stm32_sdram_test.cpp
 * \brief           Actual controller commands, finite failures and CPU leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_sdram.h"
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

extern "C" {
nx_stm32_system_t g_nx_stm32_system{};
}

namespace {
class Hardware {
  public:
    MOCK_METHOD(void, Command, (uint32_t));
};
Hardware* hardware;
nx_time_us_t now;
nx_time_us_t command_done;
bool wedge;
bool privilege;
bool isr;
bool irq_masked;
bool refuse_gate_off;
bool refuse_sdcr;
bool wrong_load_mode;
uint32_t refused_command;
unsigned writes_while_busy;
uint32_t command_issue_delay_us;
unsigned barrier_count;
std::vector<std::pair<uint32_t, nx_time_us_t>> commands;
nx_memory_window_lease_t* watched_lease;
bool barrier_observed_live_lease;
uint32_t pending_mode;
uint32_t refresh_command;
uint32_t mode_command;
nx_time_us_t pall_complete_at;

nx_time_us_t advance_clock(void) {
    ++now;
    if (!wedge && command_done != 0U && now >= command_done) {
        FMC_Bank5_6->SDSR = pending_mode;
        command_done = 0U;
    }
    return now;
}

class SDRAM : public testing::Test {
  protected:
    std::array<void*, 2> mapping{MAP_FAILED, MAP_FAILED};
    testing::NiceMock<Hardware> io;
    nx_stm32_sdram_t owner{};
    nx_stm32_sdram_config_t config{};
    nx_external_bus_port_t bus{&nx_stm32_sdram_bus_ops, &owner};
    nx_memory_window_port_t window{&nx_stm32_sdram_window_ops, &owner};
    std::vector<nx_stm32_platform_pin_config_t> pins;
    std::array<uint16_t, 64> snapshots{};
    void SetUp() override {
        for (size_t i = 0; i < mapping.size(); ++i) {
            const uintptr_t address = i == 0 ? 0x40000000U : 0x60000000U;
            const size_t bytes = i == 0 ? 0x80000U : 0x1000U;
            mapping[i] = mmap(
                reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            ASSERT_EQ(mapping[i], reinterpret_cast<void*>(address));
        }
        now = 0U;
        command_done = 0U;
        wedge = false;
        privilege = true;
        isr = false;
        irq_masked = false;
        refuse_gate_off = false;
        refuse_sdcr = false;
        wrong_load_mode = false;
        refused_command = UINT32_MAX;
        writes_while_busy = 0U;
        command_issue_delay_us = 0U;
        barrier_count = 0U;
        watched_lease = nullptr;
        barrier_observed_live_lease = false;
        commands.clear();
        hardware = &io;
        pending_mode = 0U;
        refresh_command = 0U;
        mode_command = 0U;
        pall_complete_at = 0U;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
        RCC->PLLCFGR = 0x24003010U;
        RCC->CFGR = 0U;
        FLASH->ACR = 0U;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.flash = FLASH;
        g_nx_stm32_system.power = PWR;
        g_nx_stm32_system.timer = TIM2;
        g_nx_stm32_system.core_hz = 16000000U;
        g_nx_stm32_system.time_epoch = 1U;
        g_nx_stm32_system.timer_error = NX_SUCCESS;
        g_nx_stm32_system.started = true;
        FMC_Bank5_6->SDCR[0] = 0x2d0U;
        FMC_Bank5_6->SDCR[1] = 0x2d0U;
        FMC_Bank5_6->SDTR[0] = 0x0fffffffU;
        FMC_Bank5_6->SDTR[1] = 0x0fffffffU;
        const std::array<uint16_t, 8> masks{0U,      0U,      0xdU,    0xc703U,
                                            0xff83U, 0xf83fU, 0x8133U, 0U};
        for (size_t port = 0; port < masks.size(); ++port) {
            if (masks[port] == 0U)
                continue;
            RCC->AHB1ENR |= UINT32_C(1) << port;
            for (uint8_t pin = 0; pin < 16U; ++pin) {
                if ((masks[port] & (UINT32_C(1) << pin)) == 0U)
                    continue;
                auto* gpio =
                    reinterpret_cast<GPIO_TypeDef*>(GPIOA_BASE + port * 0x400U);
                snapshots[pins.size()] = 1U;
                pins.push_back({gpio, &snapshots[pins.size()],
                                static_cast<uint8_t>(port), pin});
            }
        }
        config.pins = pins.data();
        config.pin_count = pins.size();
        config.system = &g_nx_stm32_system;
        config.timing = {{0U, 2U}, {70U, 1U}, {42U, 1U}, {63U, 1U},
                         {0U, 2U}, {18U, 1U}, {18U, 1U}};
        config.hclk_hz = 16000000U;
        config.bytes = 0x800000U;
        config.refresh_window_us = 64000U;
        config.powerup_us = 100U;
        config.prepare_budget_us = 1000U;
        config.stop_budget_us = 1000U;
        config.row_bits = 12U;
        config.column_bits = 8U;
        config.internal_banks = 4U;
        config.width_bits = 16U;
        config.cas = 3U;
        config.sdclk_div = 2U;
        config.read_pipe = 1U;
        config.initial_refreshes = 8U;
    }
    void TearDown() override {
        hardware = nullptr;
        for (size_t i = 0; i < mapping.size(); ++i)
            if (mapping[i] != MAP_FAILED)
                munmap(mapping[i], i == 0 ? 0x80000U : 0x1000U);
    }
    void Open() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    }
};

TEST_F(SDRAM, OfficialCommandsObservePowerupThenPublishExactCpuWindow) {
    testing::InSequence ordered;
    EXPECT_CALL(io, Command(FMC_SDRAM_CMD_CLK_ENABLE));
    EXPECT_CALL(io, Command(FMC_SDRAM_CMD_PALL));
    EXPECT_CALL(io, Command(FMC_SDRAM_CMD_AUTOREFRESH_MODE));
    EXPECT_CALL(io, Command(FMC_SDRAM_CMD_LOAD_MODE));
    Open();
    ASSERT_EQ(4U, commands.size());
    EXPECT_GE(commands[1].second - commands[0].second, config.powerup_us);
    EXPECT_EQ(105U,
              (FMC_Bank5_6->SDRTR & FMC_SDRTR_COUNT) >> FMC_SDRTR_COUNT_Pos);
    EXPECT_EQ(7U, (refresh_command & FMC_SDCMR_NRFS) >> FMC_SDCMR_NRFS_Pos);
    EXPECT_EQ(3U,
              ((mode_command & FMC_SDCMR_MRD) >> FMC_SDCMR_MRD_Pos) >> 4U & 7U);
    EXPECT_EQ(0U, ((mode_command & FMC_SDCMR_MRD) >> FMC_SDCMR_MRD_Pos) & 7U);
    EXPECT_EQ(NX_EXTERNAL_BUS_PREPARED, nx_external_bus_port_state(&bus));
    nx_memory_window_geometry_t geometry{};
    ASSERT_EQ(NX_SUCCESS, nx_memory_window_port_geometry(&window, &geometry));
    EXPECT_EQ(0xc0000000U, geometry.address);
    EXPECT_EQ(0x800000U, geometry.bytes);
    EXPECT_FALSE(geometry.startup_backing);
}
TEST_F(SDRAM, InvalidGeometryAndTimingHaveNoClockCommandOrRetainedOwner) {
    EXPECT_CALL(io, Command).Times(0);
    for (unsigned fault = 0; fault < 4U; ++fault) {
        auto selected = config;
        if (fault == 0U)
            selected.bytes -= 2U;
        if (fault == 1U)
            selected.timing.mrd.cycles = 17U;
        if (fault == 2U)
            selected.refresh_window_us = 100U;
        if (fault == 3U)
            selected.sdclk_div = 4U;
        const auto before = owner;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_sdram_prepare(&owner, &selected, 1000U));
        EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
        EXPECT_EQ(0U, RCC->AHB3ENR);
    }
}
TEST_F(SDRAM, ChangedClockAndForeignControllerRejectBeforeEffects) {
    RCC->CFGR = RCC_CFGR_HPRE_DIV2;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_sdram_prepare(&owner, &config, 1000U));
    EXPECT_TRUE(commands.empty());
    EXPECT_EQ(nullptr, owner.config);
    RCC->CFGR = 0U;
    RCC->AHB3ENR = RCC_AHB3ENR_FMCEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    EXPECT_TRUE(commands.empty());
    EXPECT_EQ(nullptr, owner.config);
}
TEST_F(SDRAM,
       BusyDeadlineRetainsColdReceiptAndStopFinishesOnlyAfterHardwareExit) {
    wedge = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32_sdram_prepare(&owner, &config, 20U));
    EXPECT_EQ(&config, owner.config);
    EXPECT_TRUE(owner.gate_added);
    EXPECT_FALSE(owner.prepared);
    EXPECT_NE(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(&config, owner.config);
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_FMCEN);
    wedge = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_FMCEN);
}
TEST_F(SDRAM, LiveOriginalLeaseAndEpochBlockStopWithoutRevokingCpuUsers) {
    Open();
    nx_memory_window_lease_t lease{};
    ASSERT_EQ(NX_SUCCESS, nx_memory_window_port_acquire(&window, &lease));
    auto copied = lease;
    const auto command_count = commands.size();
    EXPECT_EQ(NX_ERROR_BUSY, nx_external_bus_port_stop(&bus));
    EXPECT_EQ(command_count, commands.size());
    EXPECT_EQ(&owner, lease.owner);
    EXPECT_NE(NX_SUCCESS,
              nx_memory_window_port_release(&window, &copied, copied.epoch));
    EXPECT_NE(NX_SUCCESS,
              nx_memory_window_port_release(&window, &lease, lease.epoch + 1U));
    watched_lease = &lease;
    ASSERT_EQ(NX_SUCCESS,
              nx_memory_window_port_release(&window, &lease, lease.epoch));
    EXPECT_TRUE(barrier_observed_live_lease);
    EXPECT_EQ(nullptr, lease.owner);
    EXPECT_EQ(NX_SUCCESS, nx_external_bus_port_stop(&bus));
    EXPECT_EQ(FMC_SDRAM_CMD_SELFREFRESH_MODE, commands.back().first);
    EXPECT_EQ(0U, FMC_Bank5_6->SDCR[0] & FMC_SDCR1_SDCLK);
}
TEST_F(SDRAM,
       FailedReadbackAndRefusedGateReleaseRetainExactCleanupResponsibility) {
    refuse_sdcr = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    EXPECT_EQ(&config, owner.config);
    EXPECT_FALSE(owner.prepared);
    refuse_sdcr = false;
    refuse_gate_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(&config, owner.config);
    EXPECT_TRUE(owner.gate_added);
    refuse_gate_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(nullptr, owner.config);
}
TEST_F(SDRAM, ContextAndExhaustedFiniteDeadlineRejectWithoutEffects) {
    privilege = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    privilege = true;
    isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    isr = false;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_sdram_prepare(&owner, &config, NX_DEADLINE_NEVER));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_TRUE(commands.empty());
}
TEST_F(SDRAM, CompletedColdStopCanRestartOnlyItsOwnSelfRefreshedBank) {
    Open();
    nx_memory_window_lease_t first{};
    ASSERT_EQ(NX_SUCCESS, nx_memory_window_port_acquire(&window, &first));
    const uint64_t epoch = first.epoch;
    ASSERT_EQ(NX_SUCCESS,
              nx_memory_window_port_release(&window, &first, epoch));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    const size_t prior = commands.size();
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdram_prepare(&owner, &config, now + 1000U));
    ASSERT_LT(prior, commands.size());
    EXPECT_EQ(FMC_SDRAM_CMD_NORMAL_MODE, commands[prior].first);
    nx_memory_window_lease_t next{};
    ASSERT_EQ(NX_SUCCESS, nx_memory_window_port_acquire(&window, &next));
    EXPECT_GT(next.epoch, epoch);
    ASSERT_EQ(NX_SUCCESS,
              nx_memory_window_port_release(&window, &next, next.epoch));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
}
TEST_F(SDRAM, ModeReadbackMustBeNormalBeforeAnyCpuLoanCanBePublished) {
    wrong_load_mode = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    EXPECT_FALSE(owner.prepared);
    EXPECT_EQ(&config, owner.config);
    nx_memory_window_lease_t lease{};
    EXPECT_NE(NX_SUCCESS, nx_memory_window_port_acquire(&window, &lease));
    EXPECT_EQ(nullptr, lease.owner);
    wrong_load_mode = false;
    FMC_Bank5_6->SDSR = FMC_SDRAM_NORMAL_MODE;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
}
TEST_F(SDRAM,
       RejectedReadableCommandPayloadCannotAdvanceItsUncertainColdReceipt) {
    refused_command = FMC_SDRAM_CMD_LOAD_MODE;
    ASSERT_EQ(NX_ERROR_IO, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    ASSERT_EQ(3U, commands.size());
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(3U, commands.size());
    EXPECT_EQ(&config, owner.config);
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_FMCEN);
    refused_command = UINT32_MAX;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(3U, commands.size());
    EXPECT_EQ(&config, owner.config);
}
TEST_F(
    SDRAM,
    TimingRoundsUpAndRefreshAvoidsTheForbiddenCountWithoutExpandingGeometry) {
    config.timing.mrd = {126U, 1U};
    config.timing.wr.cycles = 16U;
    config.timing.rp.cycles = 8U;
    config.timing.rc.cycles = 9U;
    config.timing.rcd.cycles = 5U;
    config.refresh_window_us = 32000U;
    Open();
    EXPECT_EQ(1U, FMC_Bank5_6->SDTR[0] & FMC_SDTR1_TMRD);
    EXPECT_EQ(41U,
              (FMC_Bank5_6->SDRTR & FMC_SDRTR_COUNT) >> FMC_SDRTR_COUNT_Pos);
    EXPECT_EQ(FMC_SDTR2_TWR, FMC_Bank5_6->SDTR[1] & FMC_SDTR2_TWR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    config.timing.wr.cycles = 15U;
    config.refresh_window_us = 31232U;
    const size_t issued = commands.size();
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_sdram_prepare(&owner, &config, now + 1000U));
    EXPECT_EQ(issued, commands.size());
    EXPECT_EQ(nullptr, owner.config);
}
TEST_F(SDRAM, UnobservableMappingDoesNotAssumeResetWhenSyscfgClockIsOff) {
    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_SYSCFGEN;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR);
    EXPECT_TRUE(commands.empty());
}
TEST_F(SDRAM, SwappedMappingNeverClaimsTheDefaultCpuAddress) {
    SYSCFG->MEMRMP = SYSCFG_MEMRMP_SWP_FMC_0;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_sdram_prepare(&owner, &config, 1000U));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR);
    EXPECT_TRUE(commands.empty());
}
TEST_F(SDRAM,
       MappingChangePreventsLoanAndLeavesTheControllerForExplicitRecovery) {
    Open();
    SYSCFG->MEMRMP = SYSCFG_MEMRMP_SWP_FMC_0;
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(NX_ERROR_IO, nx_memory_window_port_acquire(&window, &lease));
    EXPECT_EQ(nullptr, lease.owner);
    EXPECT_EQ(&config, owner.config);
    SYSCFG->MEMRMP = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
}
TEST_F(SDRAM, StopNeverWritesANewCommandOverABusyController) {
    Open();
    const size_t issued = commands.size();
    FMC_Bank5_6->SDSR = FMC_SDSR_BUSY;
    command_done = 0U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(issued, commands.size());
    EXPECT_EQ(0U, writes_while_busy);
    EXPECT_EQ(&config, owner.config);
    FMC_Bank5_6->SDSR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
}
TEST_F(SDRAM, ForeignBusyDiscoveryRetainsGateUntilActualQuiescence) {
    FMC_Bank5_6->SDSR = FMC_SDSR_BUSY;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    ASSERT_EQ(&config, owner.config);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(&config, owner.config);
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_FMCEN);
    EXPECT_TRUE(commands.empty());
    FMC_Bank5_6->SDSR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR & RCC_AHB3ENR_FMCEN);
}
TEST_F(SDRAM, PowerupDelayStartsAfterTheActualDelayedClockCommand) {
    command_issue_delay_us = 80U;
    Open();
    ASSERT_EQ(4U, commands.size());
    EXPECT_GE(commands[1].second - commands[0].second, config.powerup_us);
}
TEST_F(SDRAM,
       NewSystemEpochWithIdenticalClockRegistersCannotBorrowOrResumeOldOwner) {
    Open();
    ++g_nx_stm32_system.time_epoch;
    now += 1000U;
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(NX_ERROR_STATE, nx_memory_window_port_acquire(&window, &lease));
    EXPECT_EQ(nullptr, lease.owner);
    const size_t issued = commands.size();
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(issued, commands.size());
    EXPECT_EQ(&config, owner.config);
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_FMCEN);
}
TEST_F(SDRAM, StoppedSystemWithUnchangedHzCannotCreateAnotherCpuLoan) {
    Open();
    g_nx_stm32_system.started = false;
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(NX_ERROR_STATE, nx_memory_window_port_acquire(&window, &lease));
    EXPECT_EQ(nullptr, lease.owner);
    EXPECT_EQ(&config, owner.config);
}
TEST_F(SDRAM, GeometryOutputCannotOverwriteTheInteriorOfTheLiveSystem) {
    Open();
    const auto before = g_nx_stm32_system;
    auto* result = reinterpret_cast<nx_memory_window_geometry_t*>(
        &g_nx_stm32_system.overflow_us);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_memory_window_port_geometry(&window, result));
    EXPECT_EQ(0, std::memcmp(&before, &g_nx_stm32_system, sizeof(before)));
}
TEST_F(SDRAM, PinSnapshotsCannotBorrowAnyInteriorSystemFieldAsMutableStorage) {
    pins[0].snapshot =
        reinterpret_cast<uint16_t*>(&g_nx_stm32_system.saved_power);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sdram_prepare(&owner, &config, 1000U));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_TRUE(commands.empty());
    EXPECT_EQ(0U, RCC->AHB3ENR);
}
TEST_F(SDRAM, GeometryOutputCannotDestroyTheOriginalLiveLease) {
    Open();
    nx_memory_window_lease_t lease{};
    ASSERT_EQ(NX_SUCCESS, nx_memory_window_port_acquire(&window, &lease));
    const auto before = lease;
    auto* result = reinterpret_cast<nx_memory_window_geometry_t*>(&lease);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_memory_window_port_geometry(&window, result));
    EXPECT_EQ(0, std::memcmp(&before, &lease, sizeof(lease)));
    EXPECT_EQ(NX_SUCCESS,
              nx_memory_window_port_release(&window, &lease, before.epoch));
}
TEST_F(SDRAM, PreviousCommandCompletingAtDeadlineCannotPublishTheNextCommand) {
    pall_complete_at = 200U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_stm32_sdram_prepare(&owner, &config, pall_complete_at));
    ASSERT_EQ(2U, commands.size());
    EXPECT_EQ(FMC_SDRAM_CMD_PALL, commands.back().first);
    EXPECT_EQ(&config, owner.config);
    EXPECT_FALSE(owner.prepared);
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_FMCEN);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(nullptr, owner.config);
}
TEST_F(SDRAM, MetadataAdmissionBeforeSystemAndGatesIsReadonly) {
    g_nx_stm32_system.started = false;
    RCC->AHB1ENR = 0U;
    RCC->APB2ENR = 0U;
    const auto before = owner;
    const auto system_before = g_nx_stm32_system;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_admit(&owner, &config));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_sdram_start(&owner, &config));
    EXPECT_EQ(0, std::memcmp(&before, &owner, sizeof(owner)));
    EXPECT_EQ(0, std::memcmp(&system_before, &g_nx_stm32_system,
                             sizeof(system_before)));
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(0U, RCC->AHB3ENR);
    EXPECT_TRUE(commands.empty());
}
TEST_F(SDRAM, StaticAdmissionRejectsBudgetAndForeignOwnerBeforeSystemEffects) {
    g_nx_stm32_system.started = false;
    for (const auto invalid : {0U, 1000001U}) {
        config.prepare_budget_us = invalid;
        EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sdram_admit(&owner, &config));
    }
    config.prepare_budget_us = 1000U;
    RCC->AHB3ENR = RCC_AHB3ENR_FMCEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdram_admit(&owner, &config));
    EXPECT_EQ(RCC_AHB3ENR_FMCEN, RCC->AHB3ENR);
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_TRUE(commands.empty());
}
TEST_F(SDRAM, HandwrittenStartNeverPromotesFiniteBudgetOverflowToInfiniteWait) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdram_start(&owner, &config));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    commands.clear();
    now = NX_DEADLINE_NEVER - 5U;
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_stm32_sdram_start(&owner, &config));
    EXPECT_TRUE(commands.empty());
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR);
}
TEST_F(SDRAM, DisabledSharedWriteFifoRejectsBeforeAnySdramCommandOrPinEffect) {
    FMC_Bank1->BTCR[0] = 0x30d1U | FMC_BCR1_WFDIS;
    const uint32_t before = FMC_Bank1->BTCR[0];
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_admit(&owner, &config));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32_sdram_prepare(&owner, &config, 1000U));
    EXPECT_EQ(&config, owner.config);
    EXPECT_FALSE(owner.prepared);
    EXPECT_TRUE(commands.empty());
    for (const auto& pin : pins)
        EXPECT_EQ(0U, pin.registers->MODER);
    EXPECT_EQ(before, FMC_Bank1->BTCR[0]);
    /* No command or CPU loan exists: only our observed gate needs release. */
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(nullptr, owner.config);
    EXPECT_EQ(0U, RCC->AHB3ENR);
    EXPECT_EQ(before, FMC_Bank1->BTCR[0]);
}
TEST_F(SDRAM, ChangedWriteFifoCannotGrantLoanOrFakeAnInitializedColdExit) {
    Open();
    const size_t issued = commands.size();
    FMC_Bank1->BTCR[0] |= FMC_BCR1_WFDIS;
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_memory_window_port_acquire(&window, &lease));
    EXPECT_EQ(nullptr, lease.owner);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_sdram_stop(&owner));
    EXPECT_EQ(issued, commands.size());
    EXPECT_EQ(&config, owner.config);
    EXPECT_NE(0U, RCC->AHB3ENR & RCC_AHB3ENR_FMCEN);
    EXPECT_NE(0U, FMC_Bank1->BTCR[0] & FMC_BCR1_WFDIS);
    FMC_Bank1->BTCR[0] &= ~static_cast<uint32_t>(FMC_BCR1_WFDIS);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdram_stop(&owner));
}
} /* namespace */

extern "C" uint32_t nx_sdram_model_read(const volatile uint32_t* reg) {
    if (reg == &FMC_Bank1->BTCR[0] && (RCC->AHB3ENR & RCC_AHB3ENR_FMCEN) == 0U)
        ADD_FAILURE() << "Unpowered FMC cannot prove shared WFDIS state";
    if (reg == &SYSCFG->MEMRMP && (RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U)
        ADD_FAILURE()
            << "Unpowered SYSCFG cannot prove a default SDRAM mapping";
    return *reg;
}
extern "C" void nx_sdram_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->AHB3ENR && refuse_gate_off &&
        (value & RCC_AHB3ENR_FMCEN) == 0U)
        value |= RCC_AHB3ENR_FMCEN;
    if (reg == &FMC_Bank5_6->SDCR[0] && refuse_sdcr)
        return;
    if (reg == &FMC_Bank5_6->SDCMR &&
        (FMC_Bank5_6->SDSR & FMC_SDSR_BUSY) != 0U) {
        ++writes_while_busy;
        return;
    }
    if (reg == &FMC_Bank5_6->SDCMR &&
        (value & FMC_SDCMR_MODE) == refused_command)
        return;
    if (reg == &FMC_Bank5_6->SDCMR &&
        ((value & FMC_SDCMR_MODE) == FMC_SDRAM_CMD_CLK_ENABLE ||
         (value & FMC_SDCMR_MODE) == FMC_SDRAM_CMD_NORMAL_MODE))
        now += command_issue_delay_us;
    /* RM0386 p365: MODE/CTB are write-only; only MRD/NRFS read back. */
    *reg = reg == &FMC_Bank5_6->SDCMR ? value & (FMC_SDCMR_MRD | FMC_SDCMR_NRFS)
                                      : value;
    if (reg == &FMC_Bank5_6->SDCMR) {
        const uint32_t mode = value & FMC_SDCMR_MODE;
        commands.emplace_back(mode, now);
        if (mode == FMC_SDRAM_CMD_AUTOREFRESH_MODE)
            refresh_command = value;
        if (mode == FMC_SDRAM_CMD_LOAD_MODE)
            mode_command = value;
        if (hardware != nullptr)
            hardware->Command(mode);
        pending_mode =
            mode == FMC_SDRAM_CMD_SELFREFRESH_MODE ? FMC_SDSR_MODES1_0
            : mode == FMC_SDRAM_CMD_POWERDOWN_MODE ? FMC_SDSR_MODES1_1
                                                   : 0U;
        FMC_Bank5_6->SDSR = FMC_SDSR_BUSY;
        if (wrong_load_mode && mode == FMC_SDRAM_CMD_LOAD_MODE)
            pending_mode = FMC_SDSR_MODES1_0;
        command_done = mode == FMC_SDRAM_CMD_PALL && pall_complete_at != 0U
                           ? pall_complete_at
                           : now + 1U;
    }
}
extern "C" bool nx_sdram_privileged(void) {
    return privilege;
}
extern "C" bool nx_sdram_in_isr(void) {
    return isr;
}
extern "C" bool nx_sdram_masked(void) {
    return irq_masked;
}
extern "C" void nx_sdram_dsb(void) {
    ++barrier_count;
    if (watched_lease != nullptr && watched_lease->owner != nullptr)
        barrier_observed_live_lease = true;
}
extern "C" uint32_t HAL_GetTick(void) {
    return 0U;
}
extern "C" uint64_t nx_stm32_system_clock_read(void* selected) {
    if (selected != &g_nx_stm32_system || !g_nx_stm32_system.started ||
        g_nx_stm32_system.timer_error != NX_SUCCESS)
        return NX_DEADLINE_NEVER;
    return advance_clock();
}
