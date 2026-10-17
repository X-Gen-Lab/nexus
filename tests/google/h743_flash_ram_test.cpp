/**
 * \file            h743_flash_ram_test.cpp
 * \brief           Draft actual RAM pulse leaf against external CPU and MMIO
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_ram_model.h"
#include "nexus/core/fault.h"
#include "terminal_watchdog.h"
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
constexpr uint32_t busy_flags =
    static_cast<uint32_t>(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE);
constexpr uint32_t ram_vector_address = 0x24004000U;
class RamHardwareBoundary {
  public:
    MOCK_METHOD(void, Trigger,
                (bool masked, bool private_vector, bool private_stack));
    MOCK_METHOD(void, Poll,
                (uint32_t pending, bool execution_retained,
                 bool controller_retained));
    MOCK_METHOD(void, Terminal, (nx_stm32h743_flash_ram_fault_t reason));
};
struct RamCpuAndHardware {
    nx_stm32h743_flash_ram_workspace_t* execution;
    RamHardwareBoundary* observer;
    uint32_t primask;
    uint32_t control;
    uint32_t ipsr;
    uint32_t basepri;
    uint32_t faultmask;
    uintptr_t sp;
    uint32_t pending;
    unsigned polls;
    unsigned clear_after;
    unsigned flash_fault_observers;
    bool reject_configuration;
    bool reject_lock;
    bool pulse_error;
    bool overflow_on_first_poll;
    bool stuck_timer_ack;
    bool triggered;
};
RamCpuAndHardware cpu;
volatile uint32_t* Control() {
    return cpu.execution->bank == 0U ? &g_h743_model.flash.CR1
                                     : &g_h743_model.flash.CR2;
}
volatile uint32_t* Status() {
    return cpu.execution->bank == 0U ? &g_h743_model.flash.SR1
                                     : &g_h743_model.flash.SR2;
}
bool PrivateStack() {
    const uintptr_t first = reinterpret_cast<uintptr_t>(cpu.execution->stack);
    const uintptr_t end = first + sizeof(cpu.execution->stack);
    return first <= cpu.sp && cpu.sp < end;
}
void ObserveTrigger() {
    if (cpu.observer != nullptr) {
        cpu.observer->Trigger(cpu.primask == 1U,
                              g_h743_model.core->VTOR == ram_vector_address,
                              PrivateStack());
    }
}
} /* namespace */

extern "C" uint32_t nx_h743_flash_ram_model_primask(void) {
    return cpu.primask;
}
extern "C" uint32_t nx_h743_flash_ram_model_control(void) {
    return cpu.control;
}
extern "C" uint32_t nx_h743_flash_ram_model_ipsr(void) {
    return cpu.ipsr;
}
extern "C" uint32_t nx_h743_flash_ram_model_basepri(void) {
    return cpu.basepri;
}
extern "C" uint32_t nx_h743_flash_ram_model_faultmask(void) {
    return cpu.faultmask;
}
extern "C" uintptr_t nx_h743_flash_ram_model_sp(void) {
    return cpu.sp;
}
extern "C" uint32_t nx_h743_flash_ram_model_vtor(
    const nx_stm32h743_flash_ram_workspace_t* execution) {
    EXPECT_EQ(cpu.execution, execution);
    return ram_vector_address;
}
extern "C" void nx_h743_flash_ram_model_barrier(void) {
    /* CPU ordering port, not a replacement for the pulse implementation.
     * Actual ARM barriers and closure are checked in the linked ELF. */
}
extern "C" uint32_t nx_h743_flash_ram_model_read(volatile uint32_t* reg) {
    return nx_h743_model_read(reg);
}
extern "C" void nx_h743_flash_ram_model_write(volatile uint32_t* reg,
                                              uint32_t value) {
    if (cpu.execution != nullptr && reg == Control()) {
        if ((value & FLASH_CR_START) != 0U) {
            ObserveTrigger();
        }
        const bool refused_config =
            cpu.reject_configuration && value == cpu.execution->configuration;
        if (refused_config ||
            (cpu.reject_lock && (value & FLASH_CR_LOCK) != 0U)) {
            ++g_h743_model.writes;
            return;
        }
    }
    if (cpu.stuck_timer_ack && reg == &g_h743_model.timer.SR) {
        ++g_h743_model.writes;
        return;
    }
    nx_h743_model_write(reg, value);
    if (cpu.execution != nullptr && reg == Control() &&
        (value & FLASH_CR_START) != 0U && (*Control() & FLASH_CR_START) != 0U) {
        cpu.triggered = true;
        *Status() = cpu.pending;
    }
}
extern "C" void nx_h743_flash_ram_model_store(volatile uint32_t* address,
                                              uint32_t value) {
    ObserveTrigger();
    nx_h743_model_flash_write32(address, value);
    cpu.triggered = true;
    *Status() = cpu.pending;
}
extern "C" void
nx_h743_flash_ram_model_poll(nx_stm32h743_flash_ram_workspace_t* execution) {
    EXPECT_EQ(cpu.execution, execution);
    ++cpu.polls;
    if (cpu.overflow_on_first_poll && cpu.polls == 1U) {
        g_h743_model.timer.CNT = 4U;
        g_h743_model.timer.SR |= TIM_SR_UIF;
    }
    if (cpu.triggered && cpu.clear_after != 0U &&
        cpu.polls >= cpu.clear_after) {
        *Status() = static_cast<uint32_t>(cpu.pulse_error ? FLASH_SR_OPERR
                                                          : FLASH_SR_EOP);
    }
    if (cpu.observer != nullptr) {
        cpu.observer->Poll(*Status() & busy_flags, execution->active,
                           execution->port->active);
    }
}
extern "C" void nx_h743_flash_ram_model_terminal_observe(
    nx_stm32h743_flash_ram_workspace_t* execution) {
    ASSERT_NE(nullptr, execution);
    if (cpu.observer != nullptr) {
        cpu.observer->Terminal(execution->fault);
    }
}
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    ++cpu.flash_fault_observers;
    ADD_FAILURE() << "A busy-bank terminal path called the Flash observer";
}

class H743FlashRamPulse : public testing::Test {
  protected:
    std::array<uint32_t, 524288U> memory{};
    nx_stm32h743_flash_state_t port{};
    nx_stm32h743_flash_ram_workspace_t execution{};
    testing::NiceMock<RamHardwareBoundary> hardware;
    void SetUp() override {
        nx_h743_model_reset();
        memory.fill(UINT32_MAX);
        port.registers = &g_h743_model.flash;
        port.memory = reinterpret_cast<uint8_t*>(memory.data());
        port.poll_limit = 8U;
        port.active = true;
        g_h743_model.physical_flash = &port;
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.remaining_effects = NX_STM32H743_EFFECT_TIMER;
        g_h743_model.timer.CR1 = TIM_CR1_CEN;
        g_h743_model.timer.DIER = TIM_DIER_UIE;
        g_h743_model.core->VTOR = ram_vector_address;
        execution.port = &port;
        execution.system = &g_nx_stm32h743_system;
        execution.destination = memory.data() + 131072U / sizeof(uint32_t);
        for (unsigned i = 0U; i < 8U; ++i) {
            execution.words[i] = 0x55aa0000U + i;
        }
        execution.configuration = FLASH_CR_PG;
        execution.saved_control = FLASH_CR_PSIZE_1;
        execution.program = true;
        execution.prepared = true;
        execution.active = true;
        cpu = {};
        cpu.execution = &execution;
        cpu.observer = &hardware;
        cpu.primask = 1U;
        cpu.sp = reinterpret_cast<uintptr_t>(execution.stack) +
                 sizeof(execution.stack) - 64U;
        cpu.pending = busy_flags;
        cpu.clear_after = 3U;
    }
    void TearDown() override {
        EXPECT_EQ(0U, cpu.flash_fault_observers);
        cpu.execution = nullptr;
        cpu.observer = nullptr;
    }
    void ExpectIdleLockedAndStillOwned() {
        EXPECT_EQ(0U, *Status() & busy_flags);
        EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_PSIZE_1 | FLASH_CR_LOCK),
                  *Control());
        EXPECT_TRUE(execution.active);
        EXPECT_TRUE(port.active);
        EXPECT_EQ(1U, cpu.primask);
        EXPECT_EQ(ram_vector_address, g_h743_model.core->VTOR);
        EXPECT_TRUE(PrivateStack());
    }
    void ExpectTerminal(nx_stm32h743_flash_ram_fault_t reason,
                        bool expected_time_valid) {
        EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
            EXPECT_CALL(hardware, Terminal(reason)).WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    execution.active && port.active && cpu.primask == 1U &&
                    PrivateStack() &&
                    g_h743_model.core->VTOR == ram_vector_address &&
                    execution.fault == reason &&
                    g_nx_stm32h743_system.started == expected_time_valid &&
                    cpu.flash_fault_observers == 0U);
            });
            (void)nx_stm32h743_flash_ram_pulse(&execution);
        }));
    }
};
TEST_F(H743FlashRamPulse, AllEightStoresUseTheMaskedRamVectorAndStack) {
    EXPECT_CALL(hardware, Trigger(true, true, true)).Times(8);
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    for (unsigned i = 0U; i < 8U; ++i) {
        EXPECT_EQ(execution.words[i], execution.destination[i]);
    }
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, EraseStartIsInsideTheSameRamResponsibility) {
    execution.program = false;
    execution.destination = nullptr;
    execution.configuration = static_cast<uint32_t>(
        FLASH_CR_SER | FLASH_CR_PSIZE_1 | (1U << FLASH_CR_SNB_Pos));
    memory[131072U / sizeof(uint32_t)] = 0U;
    EXPECT_CALL(hardware, Trigger(true, true, true)).Times(1);
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(UINT32_MAX, memory[131072U / sizeof(uint32_t)]);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, BankTwoUsesOnlyTheSecondPhysicalControls) {
    execution.bank = 1U;
    execution.destination = memory.data() + 1048576U / sizeof(uint32_t);
    g_h743_model.flash.CR1 = FLASH_CR_LOCK;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(FLASH_CR_LOCK, g_h743_model.flash.CR1);
    EXPECT_EQ(0x55aa0000U, memory[1048576U / sizeof(uint32_t)]);
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, BsyClearDoesNotReleaseQueueWritePending) {
    cpu.pending = FLASH_SR_QW;
    EXPECT_CALL(hardware, Poll(FLASH_SR_QW, true, true)).Times(2);
    EXPECT_CALL(hardware, Poll(0U, true, true)).Times(1);
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, BsyClearDoesNotReleaseWriteBufferPending) {
    cpu.pending = FLASH_SR_WBNE;
    EXPECT_CALL(hardware, Poll(FLASH_SR_WBNE, true, true)).Times(2);
    EXPECT_CALL(hardware, Poll(0U, true, true)).Times(1);
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, ErrorStatusStillDrainsAndRelocksBeforeReturn) {
    cpu.pulse_error = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_flash_ram_pulse(&execution));
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, RefusedPgConfigurationDoesNotIssueAnyStore) {
    cpu.reject_configuration = true;
    EXPECT_CALL(hardware, Trigger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_PSIZE_1 | FLASH_CR_LOCK),
              *Control());
}
TEST_F(H743FlashRamPulse, AnUnobservedEraseStartCannotManufactureCompletion) {
    execution.program = false;
    execution.destination = nullptr;
    execution.configuration = static_cast<uint32_t>(
        FLASH_CR_SER | FLASH_CR_PSIZE_1 | (1U << FLASH_CR_SNB_Pos));
    g_h743_model.hold_flash_start = true;
    memory[131072U / sizeof(uint32_t)] = 0U;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, memory[131072U / sizeof(uint32_t)]);
    EXPECT_FALSE(cpu.triggered);
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, StaleEopCannotCompleteARefusedEraseStart) {
    execution.program = false;
    execution.destination = nullptr;
    execution.configuration = static_cast<uint32_t>(
        FLASH_CR_SER | FLASH_CR_PSIZE_1 | (1U << FLASH_CR_SNB_Pos));
    g_h743_model.hold_flash_start = true;
    g_h743_model.flash.SR1 = FLASH_SR_EOP;
    memory[131072U / sizeof(uint32_t)] = 0U;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, memory[131072U / sizeof(uint32_t)]);
    EXPECT_FALSE(cpu.triggered);
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, RefusedStatusClearCannotIssueAProgrammingStore) {
    g_h743_model.flash.SR1 = FLASH_SR_EOP | FLASH_SR_OPERR;
    g_h743_model.ignore_write = &g_h743_model.flash.CCR1;
    EXPECT_CALL(hardware, Trigger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_PSIZE_1 | FLASH_CR_LOCK),
              *Control());
    EXPECT_TRUE(execution.active);
    EXPECT_TRUE(port.active);
}
TEST_F(H743FlashRamPulse, OnePendingWrapJoinsTheExistingSystemEpochOnce) {
    cpu.overflow_on_first_poll = true;
    g_nx_stm32h743_system.overflow_us = UINT64_C(1) << 32U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(UINT64_C(2) << 32U, g_nx_stm32h743_system.overflow_us);
    EXPECT_EQ(0U, g_h743_model.timer.SR & TIM_SR_UIF);
    EXPECT_EQ(4U, g_h743_model.timer.CNT);
    EXPECT_EQ(TIM_CR1_CEN, g_h743_model.timer.CR1);
    EXPECT_EQ(TIM_DIER_UIE, g_h743_model.timer.DIER);
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, APreexistingWrapIsAcknowledgedBeforeAnyTrigger) {
    g_h743_model.timer.SR = TIM_SR_UIF;
    EXPECT_CALL(hardware, Trigger(true, true, true))
        .Times(8)
        .WillRepeatedly([](bool, bool, bool) {
            EXPECT_EQ(UINT64_C(1) << 32U, g_nx_stm32h743_system.overflow_us);
            EXPECT_EQ(0U, g_h743_model.timer.SR & TIM_SR_UIF);
        });
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(UINT64_C(1) << 32U, g_nx_stm32h743_system.overflow_us);
    ExpectIdleLockedAndStillOwned();
}
TEST_F(H743FlashRamPulse, NoWrapDoesNotManufactureAnotherTimerEpoch) {
    g_nx_stm32h743_system.overflow_us = UINT64_C(3) << 32U;
    g_h743_model.timer.CNT = 9U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(UINT64_C(3) << 32U, g_nx_stm32h743_system.overflow_us);
    EXPECT_EQ(9U, g_h743_model.timer.CNT);
}
TEST_F(H743FlashRamPulse, UnpreparedEntryHasNoHardwareEffects) {
    execution.prepared = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashRamPulse, UnmaskedEntryCannotIssueAConfigurationOrTrigger) {
    cpu.primask = 0U;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashRamPulse, AForeignVectorTableCannotBeAcceptedAsRamClosure) {
    g_h743_model.core->VTOR = 0x08000000U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashRamPulse, AnArbitraryApplicationStackIsNotTheOwnedStack) {
    cpu.sp = reinterpret_cast<uintptr_t>(memory.data());
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashRamPulse, PollExhaustionParksInsideRamAndRetainsThePulse) {
    cpu.clear_after = 0U;
    ExpectTerminal(NX_H743_FLASH_RAM_FAULT_DRAIN, true);
}
TEST_F(H743FlashRamPulse, LockReadbackFailureCannotReturnToTheCaller) {
    cpu.reject_lock = true;
    ExpectTerminal(NX_H743_FLASH_RAM_FAULT_LOCK, true);
}
TEST_F(H743FlashRamPulse, AFailedWrapAckInvalidatesTimeWithoutCountingTwice) {
    cpu.overflow_on_first_poll = true;
    cpu.stuck_timer_ack = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_TIME))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    execution.active && port.active && cpu.primask == 1U &&
                    !g_nx_stm32h743_system.started &&
                    g_nx_stm32h743_system.timer_error == NX_ERROR_IO &&
                    (g_nx_stm32h743_system.remaining_effects &
                     NX_STM32H743_EFFECT_TIMER) != 0U &&
                    g_nx_stm32h743_system.overflow_us == 0U &&
                    (g_h743_model.timer.SR & TIM_SR_UIF) != 0U &&
                    cpu.flash_fault_observers == 0U);
            });
        (void)nx_stm32h743_flash_ram_pulse(&execution);
    }));
}
TEST_F(H743FlashRamPulse, APreTriggerWrapAckFailureNeverStartsFlash) {
    g_h743_model.timer.SR = TIM_SR_UIF;
    cpu.stuck_timer_ack = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        EXPECT_CALL(hardware, Trigger(testing::_, testing::_, testing::_))
            .Times(0);
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_TIME))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    execution.active && port.active && cpu.primask == 1U &&
                    !g_nx_stm32h743_system.started &&
                    g_nx_stm32h743_system.timer_error == NX_ERROR_IO &&
                    g_nx_stm32h743_system.overflow_us == 0U &&
                    g_h743_model.flash_store_count == 0U && !cpu.triggered &&
                    cpu.flash_fault_observers == 0U);
            });
        (void)nx_stm32h743_flash_ram_pulse(&execution);
    }));
}
