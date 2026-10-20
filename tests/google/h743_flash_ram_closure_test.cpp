/**
 * \file            h743_flash_ram_closure_test.cpp
 * \brief           Draft actual RAM pulse leaf against external CPU and MMIO
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_mpu_support.h"
#include "h743_flash_ram_closure_support.h"
#include "nexus/core/fault.h"
#include "nexus/io/flash.h"
#include "terminal_watchdog.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <sys/mman.h>

using namespace nexus_test::h743_ram;

class H743FlashRamPulse : public testing::Test {
  protected:
    std::array<uint32_t, 524288U> memory{};
    nx_stm32h743_flash_state_t port{};
    alignas(1024) nx_stm32h743_flash_ram_workspace_t execution{};
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
        ResetRamCpu(&execution);
        cpu.observer = &hardware;
        cpu.primask = 1U;
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
TEST_F(H743FlashRamPulse, ReturnGateUsesTheCapturedVectorAndPreservesResult) {
    const uint32_t original = cpu.original_vtor;
    execution.saved_vtor = original + 1024U;
    g_h743_model.flash.CR1 = 0U;
    EXPECT_CALL(hardware, Trigger(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32h743_flash_ram_return(
                                    &execution, original, NX_ERROR_TIMEOUT));
    EXPECT_EQ(original, g_h743_model.core->VTOR);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_PSIZE_1 | FLASH_CR_LOCK),
              g_h743_model.flash.CR1);
    EXPECT_EQ(1U, cpu.vtor_writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_TRUE(execution.active);
    EXPECT_TRUE(port.active);
    EXPECT_TRUE(PrivateStack());
    EXPECT_EQ(1U, cpu.primask);
}
TEST_F(H743FlashRamPulse, ReturnGateRejectsTheOtherBanksPendingWrite) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        g_h743_model.flash.SR2 = FLASH_SR_WBNE;
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_DRAIN))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    execution.active && port.active && PrivateStack() &&
                    cpu.primask == 1U && cpu.vtor_writes == 0U &&
                    g_h743_model.core->VTOR == ram_vector_address &&
                    (g_h743_model.flash.SR2 & FLASH_SR_WBNE) != 0U);
            });
        (void)nx_stm32h743_flash_ram_return(&execution, cpu.original_vtor,
                                            NX_ERROR_IO);
    }));
}
TEST_F(H743FlashRamPulse, ReturnGateRetainsProtectionBeforeVectorWithdrawal) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        execution.guard.remaining_effects = NX_H743_FLASH_MPU_EFFECT_CONTROL;
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_EXCEPTION))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    execution.active && port.active && PrivateStack() &&
                    cpu.primask == 1U && cpu.vtor_writes == 0U &&
                    g_h743_model.core->VTOR == ram_vector_address &&
                    execution.guard.remaining_effects ==
                        NX_H743_FLASH_MPU_EFFECT_CONTROL);
            });
        (void)nx_stm32h743_flash_ram_return(&execution, cpu.original_vtor,
                                            NX_ERROR_IO);
    }));
}
TEST_F(H743FlashRamPulse, ReturnGateRefusedLockKeepsTheRamVector) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        g_h743_model.flash.CR1 = 0U;
        cpu.reject_lock = true;
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_LOCK))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    execution.active && port.active && PrivateStack() &&
                    cpu.primask == 1U && cpu.vtor_writes == 0U &&
                    g_h743_model.core->VTOR == ram_vector_address &&
                    (g_h743_model.flash.CR1 & FLASH_CR_LOCK) == 0U);
            });
        (void)nx_stm32h743_flash_ram_return(&execution, cpu.original_vtor,
                                            NX_ERROR_IO);
    }));
}
TEST_F(H743FlashRamPulse, ReturnGateRefusedVectorReadbackRetainsTheOwner) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        cpu.refuse_vtor_restore = true;
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_EXCEPTION))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    execution.active && port.active && PrivateStack() &&
                    cpu.primask == 1U && cpu.vtor_writes == 1U &&
                    cpu.refused_writes == 1U &&
                    g_h743_model.core->VTOR == ram_vector_address &&
                    g_h743_model.flash.CR1 ==
                        (FLASH_CR_PSIZE_1 | FLASH_CR_LOCK));
            });
        (void)nx_stm32h743_flash_ram_return(&execution, cpu.original_vtor,
                                            NX_ERROR_IO);
    }));
}
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
    cpu.msp = reinterpret_cast<uintptr_t>(memory.data());
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

/* These six cases observe raw CPU ports in the same model as the original
 * leaf tests. No host helper claims to execute an ARM stack switch. */
TEST_F(H743FlashRamPulse, HandlerModeCannotEnterTheRamPulse) {
    cpu.ipsr = 17U;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashRamPulse, UnprivilegedThreadCannotEnterTheRamPulse) {
    cpu.control = 1U;
    g_h743_model.privileged = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashRamPulse, PrivatePspDoesNotSubstituteForThePrivateMsp) {
    cpu.psp = cpu.msp;
    cpu.msp = reinterpret_cast<uintptr_t>(memory.data());
    cpu.control = 2U;
    EXPECT_NE(NX_SUCCESS, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashRamPulse, AnUnalignedPrivateMspCannotStartFlash) {
    cpu.msp = reinterpret_cast<uintptr_t>(execution.stack) + 4U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashRamPulse, AnInRangeStackWithoutFrameMarginCannotStartFlash) {
    cpu.msp = reinterpret_cast<uintptr_t>(execution.stack) + 8U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_ram_pulse(&execution));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashRamPulse, ForeignBasepriOrFaultmaskCannotBecomeTheLongMask) {
    for (bool faultmask : {false, true}) {
        cpu.basepri = faultmask ? 0U : 0x50U;
        cpu.faultmask = faultmask ? 1U : 0U;
        EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32h743_flash_ram_pulse(&execution));
        EXPECT_EQ(faultmask ? 0U : 0x50U, cpu.basepri);
        EXPECT_EQ(faultmask ? 1U : 0U, cpu.faultmask);
        EXPECT_EQ(0U, g_h743_model.writes);
    }
}
namespace {
template <typename T> auto RamObjectBytes(const T& object) {
    std::array<uint8_t, sizeof(T)> bytes{};
    std::memcpy(bytes.data(), &object, bytes.size());
    return bytes;
}
} /* namespace */
class H743FlashRamOwner : public H743FlashRamPulse {
  protected:
    nx_flash_port_t face{&nx_stm32h743_flash_ops, &port};
    void SetUp() override {
        H743FlashRamPulse::SetUp();
        execution = {};
        port.active = false;
        port.supply_mv = 3300U;
        cpu.owner_port = &port;
        cpu.primask = 0U;
        cpu.control = 0U;
        cpu.msp = 0x2407f000U;
        cpu.psp = 0x2407e000U;
        g_h743_model.core->VTOR = cpu.original_vtor;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_nx_stm32h743_system.remaining_effects |=
            NX_STM32H743_EFFECT_TIMER_GATE;
        g_h743_model.timer.PSC = 63U;
        g_h743_model.timer.ARR = UINT32_MAX;
        g_h743_model.rcc.APB1LENR = RCC_APB1LENR_TIM2EN;
        g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
        g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
        g_h743_model.flash.ACR = FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0;
        g_h743_model.flash.CR1 = g_h743_model.flash.CR2 = FLASH_CR_LOCK;
    }
    void TearDown() override {
        /* Terminal cases run in children. Never fabricate idle status to
         * reclaim a parent's live command, private stack or vector owner. */
        const uint32_t pending =
            (g_h743_model.flash.SR1 | g_h743_model.flash.SR2) & busy_flags;
        EXPECT_FALSE(execution.active);
        EXPECT_FALSE(port.active);
        EXPECT_EQ(0U, pending);
        if (execution.active || port.active || pending != 0U) {
            std::abort();
        }
        const nx_result_t stopped = nx_stm32h743_flash_ram_stop(&execution);
        EXPECT_EQ(NX_SUCCESS, stopped);
        if (stopped != NX_SUCCESS || execution.prepared || execution.active) {
            std::abort();
        }
        H743FlashRamPulse::TearDown();
    }
    void Prepare() {
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32h743_flash_ram_initialize(&port, &execution));
        ASSERT_TRUE(execution.prepared);
        ASSERT_EQ(&port, execution.port);
        ASSERT_EQ(&g_nx_stm32h743_system, execution.system);
    }
    void Program() {
        std::array<uint8_t, 32U> input{};
        input.fill(0x5aU);
        ASSERT_EQ(NX_SUCCESS,
                  nx_flash_port_program(&face, 131072U, input.data(),
                                        input.size(), 10000U));
    }
};
TEST_F(H743FlashRamOwner, EnabledFpuWithoutLiveContextDoesNotInventRejection) {
    cpu.fpccr = UINT32_C(0xc0000000);
    g_h743_model.core->CPACR = UINT32_C(0x00f00000);
    ASSERT_NO_FATAL_FAILURE(Prepare());
    EXPECT_EQ(UINT32_C(0xc0000000), cpu.fpccr);
    EXPECT_EQ(UINT32_C(0x00f00000), g_h743_model.core->CPACR);
    EXPECT_EQ(cpu.code_source, cpu.code_run);
    EXPECT_EQ(0x08000000U, g_h743_model.core->VTOR);
}
TEST_F(H743FlashRamOwner, ActiveLazyFpFrameRejectsBeforeStackOrVectorMutation) {
    cpu.fpccr |= 1U;
    const uintptr_t msp = cpu.msp;
    const uintptr_t psp = cpu.psp;
    EXPECT_NE(NX_SUCCESS, nx_stm32h743_flash_ram_initialize(&port, &execution));
    EXPECT_EQ(msp, cpu.msp);
    EXPECT_EQ(psp, cpu.psp);
    EXPECT_EQ(0U, cpu.control);
    EXPECT_EQ(0U, cpu.copy_writes);
    EXPECT_EQ(0U, cpu.vtor_writes);
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_FALSE(execution.prepared);
}
TEST_F(H743FlashRamOwner, ActiveFpStateIsPreservedInBothThreadStackModes) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    for (uint32_t control : {4U, 6U}) {
        cpu.control = control;
        const uintptr_t msp = cpu.msp;
        const uintptr_t psp = cpu.psp;
        const unsigned entries = cpu.bridge_entries;
        ASSERT_NO_FATAL_FAILURE(Program());
        EXPECT_EQ(entries + 1U, cpu.bridge_entries);
        EXPECT_EQ(control, cpu.control);
        EXPECT_EQ(msp, cpu.msp);
        EXPECT_EQ(psp, cpu.psp);
        EXPECT_EQ(0U, cpu.primask);
        EXPECT_EQ(cpu.original_vtor, g_h743_model.core->VTOR);
        memory[131072U / sizeof(uint32_t)] = UINT32_MAX;
        std::fill_n(memory.data() + 131072U / sizeof(uint32_t), 8U, UINT32_MAX);
    }
    /* CPU-port integration only. The actual bridge register roundtrip and
     * extended-frame reserve are separate actual-ELF checker cases. */
}
TEST_F(H743FlashRamOwner, CopyRangeComesFromActualLinkSymbolInputs) {
    const std::array<std::array<uint32_t, 4U>, 8U> bad{
        {{0x08006000U, 0x08006080U, 0x08004000U, 0x08004080U},
         {0x24008080U, 0x24008000U, 0x08004000U, 0x08004080U},
         {0x24004000U, 0x24004080U, 0x08004000U, 0x08004080U},
         {0x24008000U, 0x24008080U, 0x08004000U, 0x0800407fU},
         {0x24008000U, 0x24008080U, 0x08006000U, 0x08006080U},
         {0xfffffff0U, 0x00000070U, 0x08004000U, 0x08004080U},
         {0x2407ffc0U, 0x24080040U, 0x08004000U, 0x08004080U},
         {0x30000000U, 0x30000080U, 0x08004000U, 0x08004080U}}};
    for (const auto& ranges : bad) {
        cpu.code_start = ranges[0];
        cpu.code_end = ranges[1];
        cpu.code_load = ranges[2];
        cpu.code_load_end = ranges[3];
        EXPECT_NE(NX_SUCCESS,
                  nx_stm32h743_flash_ram_initialize(&port, &execution));
        EXPECT_EQ(0U, cpu.copy_writes);
        EXPECT_EQ(0U, cpu.vtor_writes);
        EXPECT_FALSE(execution.prepared);
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&execution));
    }
}
TEST_F(H743FlashRamOwner, CopyMismatchNeverPublishesPreparedExecution) {
    cpu.corrupt_copy = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32h743_flash_ram_initialize(&port, &execution));
    EXPECT_GT(cpu.copy_writes, 0U);
    EXPECT_FALSE(execution.prepared);
    EXPECT_EQ(0U, cpu.vtor_writes);
    EXPECT_EQ(cpu.original_vtor, g_h743_model.core->VTOR);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&execution));
    cpu.corrupt_copy = false;
    ASSERT_NO_FATAL_FAILURE(Prepare());
    EXPECT_EQ(cpu.code_source, cpu.code_run);
}
TEST_F(H743FlashRamOwner, EvenDefaultSymbolMatchesThumbVectorAliases) {
    /* The pinned startup leaves Default_Handler as an even NOTYPE symbol;
     * its .thumb_set NMI/HardFault aliases carry Thumb bit0 in the vectors. */
    cpu.default_handler &= ~UINT32_C(1);
    cpu.original_vectors[2] = cpu.default_handler | UINT32_C(1);
    cpu.original_vectors[3] = cpu.default_handler | UINT32_C(1);
    ASSERT_NO_FATAL_FAILURE(Prepare());
    EXPECT_EQ(cpu.code_source, cpu.code_run);
    EXPECT_EQ(cpu.original_vtor, g_h743_model.core->VTOR);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashRamOwner, MpuEnableRejectsWithoutChangingProtectionPolicy) {
    cpu.mpu_control = 1U;
    EXPECT_NE(NX_SUCCESS, nx_stm32h743_flash_ram_initialize(&port, &execution));
    EXPECT_EQ(1U, cpu.mpu_control);
    EXPECT_EQ(0U, cpu.copy_writes);
    EXPECT_EQ(0U, cpu.vtor_writes);
    EXPECT_EQ(0U, g_h743_model.writes);
    cpu.mpu_control = 0U;
}
TEST_F(H743FlashRamOwner, CacheChangedAfterPreparationCannotReachTheBridge) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    g_h743_model.core->CCR = SCB_CCR_DC_Msk | SCB_CCR_IC_Msk;
    std::array<uint8_t, 32U> input{};
    input.fill(0x5aU);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_program(&face, 131072U, input.data(), input.size(),
                                    10000U));
    EXPECT_EQ(0U, cpu.vtor_writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_TRUE(execution.prepared);
    g_h743_model.core->CCR = 0U;
}
TEST_F(H743FlashRamOwner, FinalRawCpuAndProtectionAdmissionPrecedesTheBridge) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    struct RawCase {
        uint32_t control;
        uint32_t ipsr;
        uint32_t primask;
        uint32_t fpccr;
        uint32_t mpu;
        uint32_t basepri;
        uint32_t faultmask;
        nx_result_t result;
    };
    const std::array<RawCase, 7U> cases{
        {{0U, 17U, 0U, 0xc0000000U, 0U, 0U, 0U, NX_ERROR_CONTEXT},
         {1U, 0U, 0U, 0xc0000000U, 0U, 0U, 0U, NX_ERROR_CONTEXT},
         {0U, 0U, 1U, 0xc0000000U, 0U, 0U, 0U, NX_ERROR_CONTEXT},
         {0U, 0U, 0U, 0xc0000001U, 0U, 0U, 0U, NX_ERROR_UNSUPPORTED},
         {0U, 0U, 0U, 0xc0000000U, 1U, 0U, 0U, NX_ERROR_UNSUPPORTED},
         {0U, 0U, 0U, 0xc0000000U, 0U, 0x50U, 0U, NX_ERROR_CONTEXT},
         {0U, 0U, 0U, 0xc0000000U, 0U, 0U, 1U, NX_ERROR_CONTEXT}}};
    const auto original = RamObjectBytes(execution);
    const auto port_before = RamObjectBytes(port);
    const unsigned copies = cpu.copy_writes;
    const uint32_t writes = g_h743_model.writes;
    std::array<uint8_t, 32U> input{};
    input.fill(0x5aU);
    /* The generic Native Arch helpers remain privileged task context. Only
     * the final raw CPU/protection observations change after preparation. */
    g_h743_model.privileged = true;
    for (const auto& item : cases) {
        cpu.control = item.control;
        cpu.ipsr = item.ipsr;
        cpu.primask = item.primask;
        cpu.fpccr = item.fpccr;
        cpu.mpu_control = item.mpu;
        cpu.basepri = item.basepri;
        cpu.faultmask = item.faultmask;
        const uintptr_t msp = cpu.msp;
        const uintptr_t psp = cpu.psp;
        EXPECT_EQ(item.result,
                  nx_flash_port_program(&face, 131072U, input.data(),
                                        input.size(), 10000U));
        EXPECT_EQ(original, RamObjectBytes(execution));
        EXPECT_EQ(port_before, RamObjectBytes(port));
        EXPECT_EQ(0U, cpu.bridge_entries);
        EXPECT_EQ(0U, cpu.vtor_writes);
        EXPECT_EQ(copies, cpu.copy_writes);
        EXPECT_EQ(writes, g_h743_model.writes);
        EXPECT_EQ(0U, g_h743_model.flash_store_count);
        EXPECT_EQ(msp, cpu.msp);
        EXPECT_EQ(psp, cpu.psp);
        EXPECT_EQ(item.control, cpu.control);
        EXPECT_EQ(item.primask, cpu.primask);
        EXPECT_EQ(item.basepri, cpu.basepri);
        EXPECT_EQ(item.faultmask, cpu.faultmask);
        EXPECT_EQ(item.fpccr, cpu.fpccr);
        EXPECT_EQ(item.mpu, cpu.mpu_control);
    }
    /* Explicitly release only the external changes introduced by this case;
     * neither command ownership nor a busy hardware flag is cleared here. */
    cpu.control = cpu.ipsr = cpu.primask = cpu.basepri = cpu.faultmask = 0U;
    cpu.fpccr = 0xc0000000U;
    cpu.mpu_control = 0U;
}
TEST_F(H743FlashRamOwner, RetainedGuardCannotStageOrBorrowAnotherCommand) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    std::array<uint8_t, 32U> input{};
    input.fill(0x5aU);
    for (uint32_t effect : {uint32_t{NX_H743_FLASH_MPU_EFFECT_SELECTOR},
                            uint32_t{NX_H743_FLASH_MPU_EFFECT_REGION},
                            uint32_t{NX_H743_FLASH_MPU_EFFECT_CONTROL}}) {
        EXPECT_EXIT(
            {
                execution.guard.remaining_effects = effect;
                const auto before = RamObjectBytes(execution);
                const auto controller = RamObjectBytes(port);
                const uint32_t writes = g_h743_model.writes;
                const nx_result_t result = nx_flash_port_program(
                    &face, 131072U, input.data(), input.size(), 10000U);
                std::_Exit(result == NX_ERROR_BUSY &&
                                   before == RamObjectBytes(execution) &&
                                   controller == RamObjectBytes(port) &&
                                   cpu.bridge_entries == 0U &&
                                   cpu.vtor_writes == 0U &&
                                   g_h743_model.writes == writes &&
                                   g_h743_model.flash_store_count == 0U
                               ? 0
                               : 1);
            },
            testing::ExitedWithCode(0), "");
    }
}
TEST_F(H743FlashRamOwner, RetainedProtectionCannotEscapeTheRamReturnBoundary) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        execution.destination = memory.data() + 131072U / sizeof(uint32_t);
        execution.configuration = FLASH_CR_PG;
        execution.program = true;
        execution.active = port.active = true;
        execution.deadline = 10000U;
        execution.guard.remaining_effects = NX_H743_FLASH_MPU_EFFECT_REGION;
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_EXCEPTION))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    PrivateStack() && cpu.primask == 1U &&
                    g_h743_model.core->VTOR == ram_vector_address &&
                    execution.guard.remaining_effects != 0U &&
                    execution.active && port.active &&
                    g_h743_model.flash_store_count == 0U);
            });
        (void)NX_H743_FLASH_RAM_ENTER(&execution);
    }));
}
TEST_F(H743FlashRamOwner, RejectedGuardLocksItsOwnedIdleTargetBeforeReturn) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    EXPECT_EXIT(
        {
            execution.destination = memory.data() + 131072U / sizeof(uint32_t);
            execution.configuration = FLASH_CR_PG;
            execution.program = true;
            execution.active = port.active = true;
            execution.deadline = 10000U;
            execution.saved_control = 0U;
            g_h743_model.flash.CR1 = 0U;
            /* A disabled MPU can still contain a foreign enabled descriptor.
             * The real guard rejects it without issuing a physical pulse. */
            nexus_test::h743_mpu::mpu.regions[3].attributes |= 1U;
            const nx_result_t result = NX_H743_FLASH_RAM_ENTER(&execution);
            std::_Exit(result == NX_ERROR_STATE &&
                               g_h743_model.flash.CR1 == FLASH_CR_LOCK &&
                               execution.guard.remaining_effects == 0U &&
                               cpu.primask == 0U &&
                               g_h743_model.core->VTOR == cpu.original_vtor &&
                               g_h743_model.flash_store_count == 0U
                           ? 0
                           : 1);
        },
        testing::ExitedWithCode(0), "");
}
TEST_F(H743FlashRamOwner, ConstructorRejectsWorkspaceControlAndImageAliases) {
    const auto system_before = RamObjectBytes(g_nx_stm32h743_system);
    const auto hardware_before = RamObjectBytes(g_h743_model);
    const auto core_before = RamObjectBytes(*g_h743_model.core);
    const auto port_before = RamObjectBytes(port);
    const auto workspace_before = RamObjectBytes(execution);
    const auto code_before = cpu.code_run;
    const auto load_before = cpu.code_source;
    const auto vectors_before = cpu.original_vectors;
    struct Alias {
        void* pointer;
        uintptr_t physical;
    };
    const uintptr_t system =
        reinterpret_cast<uintptr_t>(&g_nx_stm32h743_system);
    const uintptr_t state = reinterpret_cast<uintptr_t>(&port);
    const uintptr_t code = reinterpret_cast<uintptr_t>(cpu.code_run.data());
    const std::array<Alias, 16U> aliases{
        {{&g_nx_stm32h743_system, 0x24001000U},
         {&port, 0x24002000U},
         {&g_h743_model.rcc, RCC_BASE},
         {&g_h743_model.flash, FLASH_R_BASE},
         {&g_h743_model.power, PWR_BASE},
         {&g_h743_model.timer, TIM2_BASE},
         {g_h743_model.core, SCB_BASE},
         {cpu.code_run.data(), cpu.code_start},
         {cpu.code_source.data(), cpu.code_load},
         {cpu.original_vectors.data(), cpu.original_vtor},
         {reinterpret_cast<void*>(system - 1024U), 0x24000c00U},
         {reinterpret_cast<void*>(code - 1024U), cpu.code_start - 1024U},
         {reinterpret_cast<void*>(system - sizeof(execution) + 1U),
          0x24001000U - sizeof(execution) + 1U},
         {reinterpret_cast<void*>(system + sizeof(g_nx_stm32h743_system) - 1U),
          0x24001000U + sizeof(g_nx_stm32h743_system) - 1U},
         {reinterpret_cast<void*>(state - sizeof(execution) + 1U),
          0x24002000U - sizeof(execution) + 1U},
         {reinterpret_cast<void*>(code - sizeof(execution) + 1U),
          cpu.code_start - sizeof(execution) + 1U}}};
    for (const auto& item : aliases) {
        void* candidate = item.pointer;
        cpu.alias_pointer = reinterpret_cast<uintptr_t>(candidate);
        cpu.alias_address = item.physical;
        EXPECT_EXIT(
            {
                const nx_result_t result = nx_stm32h743_flash_ram_initialize(
                    &port, static_cast<nx_stm32h743_flash_ram_workspace_t*>(
                               candidate));
                const bool unchanged =
                    system_before == RamObjectBytes(g_nx_stm32h743_system) &&
                    hardware_before == RamObjectBytes(g_h743_model) &&
                    core_before == RamObjectBytes(*g_h743_model.core) &&
                    port_before == RamObjectBytes(port) &&
                    workspace_before == RamObjectBytes(execution) &&
                    code_before == cpu.code_run &&
                    load_before == cpu.code_source &&
                    vectors_before == cpu.original_vectors;
                std::_Exit(result == NX_ERROR_INVALID && unchanged &&
                                   cpu.copy_writes == 0U &&
                                   cpu.bridge_entries == 0U &&
                                   cpu.vtor_writes == 0U
                               ? 0
                               : 1);
            },
            testing::ExitedWithCode(0), "");
    }
    /* A valid host mapping with no read/write permission observes the
     * forbidden first dereference, not merely a later unchanged snapshot. */
    void* unreadable =
        mmap(nullptr, 4096U, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(MAP_FAILED, unreadable);
    cpu.alias_pointer = reinterpret_cast<uintptr_t>(unreadable);
    cpu.alias_address = 0x24001000U;
    EXPECT_EXIT(
        {
            const nx_result_t result = nx_stm32h743_flash_ram_initialize(
                &port,
                static_cast<nx_stm32h743_flash_ram_workspace_t*>(unreadable));
            std::_Exit(
                result == NX_ERROR_INVALID && cpu.copy_writes == 0U &&
                        cpu.bridge_entries == 0U && cpu.vtor_writes == 0U &&
                        hardware_before == RamObjectBytes(g_h743_model) &&
                        system_before ==
                            RamObjectBytes(g_nx_stm32h743_system) &&
                        port_before == RamObjectBytes(port) &&
                        workspace_before == RamObjectBytes(execution)
                    ? 0
                    : 1);
        },
        testing::ExitedWithCode(0), "");
    EXPECT_EQ(0, munmap(unreadable, 4096U));
    cpu.alias_pointer = cpu.alias_address = 0U;
}
TEST_F(H743FlashRamOwner, ConstructorRejectsPortInsideAnyOwnedControlSpan) {
    const auto system_before = RamObjectBytes(g_nx_stm32h743_system);
    const auto hardware_before = RamObjectBytes(g_h743_model);
    const auto core_before = RamObjectBytes(*g_h743_model.core);
    const auto port_before = RamObjectBytes(port);
    const auto workspace_before = RamObjectBytes(execution);
    const auto code_before = cpu.code_run;
    const auto load_before = cpu.code_source;
    const auto vectors_before = cpu.original_vectors;
    struct Alias {
        void* pointer;
        uintptr_t physical;
    };
    const uintptr_t owned = reinterpret_cast<uintptr_t>(&execution);
    const uintptr_t system =
        reinterpret_cast<uintptr_t>(&g_nx_stm32h743_system);
    const std::array<Alias, 18U> aliases{
        {{&execution, ram_vector_address},
         {execution.stack,
          ram_vector_address +
              offsetof(nx_stm32h743_flash_ram_workspace_t, stack)},
         {execution.words,
          ram_vector_address +
              offsetof(nx_stm32h743_flash_ram_workspace_t, words)},
         {&execution.configuration,
          ram_vector_address +
              offsetof(nx_stm32h743_flash_ram_workspace_t, configuration)},
         {&g_nx_stm32h743_system, 0x24001000U},
         {&g_h743_model.rcc, RCC_BASE},
         {&g_h743_model.flash, FLASH_R_BASE},
         {&g_h743_model.power, PWR_BASE},
         {&g_h743_model.timer, TIM2_BASE},
         {g_h743_model.core, SCB_BASE},
         {cpu.code_run.data(), cpu.code_start},
         {cpu.code_source.data(), cpu.code_load},
         {cpu.original_vectors.data(), cpu.original_vtor},
         {reinterpret_cast<void*>(owned - alignof(nx_stm32h743_flash_state_t)),
          ram_vector_address - alignof(nx_stm32h743_flash_state_t)},
         {reinterpret_cast<void*>(owned - sizeof(port) + 1U),
          ram_vector_address - sizeof(port) + 1U},
         {reinterpret_cast<void*>(owned + sizeof(execution) - 1U),
          ram_vector_address + sizeof(execution) - 1U},
         {reinterpret_cast<void*>(system - sizeof(port) + 1U),
          0x24001000U - sizeof(port) + 1U},
         {reinterpret_cast<void*>(system + sizeof(g_nx_stm32h743_system) - 1U),
          0x24001000U + sizeof(g_nx_stm32h743_system) - 1U}}};
    for (const auto& item : aliases) {
        void* candidate = item.pointer;
        cpu.alias_pointer = reinterpret_cast<uintptr_t>(candidate);
        cpu.alias_address = item.physical;
        EXPECT_EXIT(
            {
                const nx_result_t result = nx_stm32h743_flash_ram_initialize(
                    static_cast<nx_stm32h743_flash_state_t*>(candidate),
                    &execution);
                const bool unchanged =
                    system_before == RamObjectBytes(g_nx_stm32h743_system) &&
                    hardware_before == RamObjectBytes(g_h743_model) &&
                    core_before == RamObjectBytes(*g_h743_model.core) &&
                    port_before == RamObjectBytes(port) &&
                    workspace_before == RamObjectBytes(execution) &&
                    code_before == cpu.code_run &&
                    load_before == cpu.code_source &&
                    vectors_before == cpu.original_vectors;
                std::_Exit(result == NX_ERROR_INVALID && unchanged &&
                                   cpu.copy_writes == 0U &&
                                   cpu.bridge_entries == 0U &&
                                   cpu.vtor_writes == 0U
                               ? 0
                               : 1);
            },
            testing::ExitedWithCode(0), "");
    }
    void* unreadable =
        mmap(nullptr, 4096U, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(MAP_FAILED, unreadable);
    cpu.alias_pointer = reinterpret_cast<uintptr_t>(unreadable);
    cpu.alias_address = 0x24001000U;
    EXPECT_EXIT(
        {
            const nx_result_t result = nx_stm32h743_flash_ram_initialize(
                static_cast<nx_stm32h743_flash_state_t*>(unreadable),
                &execution);
            std::_Exit(
                result == NX_ERROR_INVALID && cpu.copy_writes == 0U &&
                        cpu.bridge_entries == 0U && cpu.vtor_writes == 0U &&
                        hardware_before == RamObjectBytes(g_h743_model) &&
                        system_before ==
                            RamObjectBytes(g_nx_stm32h743_system) &&
                        port_before == RamObjectBytes(port) &&
                        workspace_before == RamObjectBytes(execution)
                    ? 0
                    : 1);
        },
        testing::ExitedWithCode(0), "");
    EXPECT_EQ(0, munmap(unreadable, 4096U));
    cpu.alias_pointer = cpu.alias_address = 0U;
}
TEST_F(H743FlashRamOwner, PreparedCodeAndEveryWorkspacePartRejectOutputAlias) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    const auto before = RamObjectBytes(execution);
    const auto code_before = cpu.code_run;
    auto* raw = reinterpret_cast<uint8_t*>(&execution);
    const std::array<size_t, 13U> offsets{
        0U,
        sizeof(execution.vectors) - 1U,
        offsetof(nx_stm32h743_flash_ram_workspace_t, stack),
        offsetof(nx_stm32h743_flash_ram_workspace_t, stack) +
            sizeof(execution.stack) - 1U,
        offsetof(nx_stm32h743_flash_ram_workspace_t, port),
        offsetof(nx_stm32h743_flash_ram_workspace_t, system),
        offsetof(nx_stm32h743_flash_ram_workspace_t, destination),
        offsetof(nx_stm32h743_flash_ram_workspace_t, words),
        offsetof(nx_stm32h743_flash_ram_workspace_t, configuration),
        offsetof(nx_stm32h743_flash_ram_workspace_t, fault_status),
        offsetof(nx_stm32h743_flash_ram_workspace_t, fault_exception),
        offsetof(nx_stm32h743_flash_ram_workspace_t, prepared),
        sizeof(execution) - 1U};
    for (size_t offset : offsets) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_flash_port_read(&face, 131072U, raw + offset, 1U));
        EXPECT_EQ(0, std::memcmp(before.data(), &execution, before.size()));
        /* Keep an incorrect implementation from corrupting later test setup.
         * No pulse has been admitted in this output-only regression. */
        std::memcpy(&execution, before.data(), before.size());
    }
    for (size_t offset : {size_t{0U}, cpu.code_run.size() - 1U}) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_flash_port_read(&face, 131072U,
                                     cpu.code_run.data() + offset, 1U));
        EXPECT_EQ(code_before, cpu.code_run);
        cpu.code_run = code_before;
    }
}
TEST_F(H743FlashRamOwner, WholeInputOverlapRejectsBeforeTheFirstStagingWrite) {
    struct Boundary {
        alignas(1024) std::array<uint8_t, 1024U> before;
        nx_stm32h743_flash_ram_workspace_t owned;
        std::array<uint8_t, 64U> after;
    } boundary{};
    cpu.execution = &boundary.owned;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32h743_flash_ram_initialize(&port, &boundary.owned));
    const auto snapshot = RamObjectBytes(boundary.owned);
    auto* raw = reinterpret_cast<uint8_t*>(&boundary);
    constexpr size_t first = offsetof(Boundary, owned);
    constexpr size_t last = first + sizeof(boundary.owned);
    for (size_t offset : {first - 16U, first, last - 16U}) {
        EXPECT_EXIT(
            {
                const nx_result_t result = nx_flash_port_program(
                    &face, 131072U, raw + offset, 32U, 10000U);
                const bool unchanged =
                    std::memcmp(snapshot.data(), &boundary.owned,
                                snapshot.size()) == 0;
                std::_Exit(result == NX_ERROR_INVALID && unchanged &&
                                   g_h743_model.writes == 0U &&
                                   g_h743_model.flash_store_count == 0U
                               ? 0
                               : 1);
            },
            testing::ExitedWithCode(0), "");
    }
    /* Exact half-open neighbors exist in the enclosing object. A valid
     * implementation must not broaden workspace ownership into either byte. */
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_read(&face, 131072U, raw + first - 1U, 1U));
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_read(&face, 131072U, raw + last, 1U));
    EXPECT_EQ(snapshot, RamObjectBytes(boundary.owned));
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&boundary.owned));
    cpu.execution = &execution;
}
TEST_F(H743FlashRamOwner, ASecondStateCannotAdoptOrReleaseThePhysicalOwner) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    alignas(1024) nx_stm32h743_flash_ram_workspace_t other{};
    /* Establish every byte before admission; rejection may retain none of it.
     */
    std::memset(&other, 0, sizeof(other));
    const std::array<uint8_t, sizeof(other)> zero_workspace{};
    nx_stm32h743_flash_state_t other_port = port;
    cpu.secondary_execution = &other;
    cpu.secondary_port = &other_port;
    const auto workspace_before = RamObjectBytes(execution);
    const auto port_before = RamObjectBytes(port);
    const auto other_port_before = RamObjectBytes(other_port);
    const auto cpu_before = RamObjectBytes(cpu);
    const auto system_before = RamObjectBytes(g_nx_stm32h743_system);
    const auto core_before = RamObjectBytes(*g_h743_model.core);
    const auto flash_before = RamObjectBytes(g_h743_model.flash);
    const auto timer_before = RamObjectBytes(g_h743_model.timer);
    const auto clock_before = RamObjectBytes(g_h743_model.rcc);
    const auto power_before = RamObjectBytes(g_h743_model.power);
    auto& mpu = nexus_test::h743_mpu::mpu;
    const auto mpu_before = RamObjectBytes(mpu.registers);
    const auto regions_before = mpu.regions;
    const auto region_reads_before = mpu.region_reads;
    const auto mpu_writes_before = mpu.writes.size();
    const auto triggers_before = mpu.triggers.size();
    const unsigned dsbs_before = mpu.dsbs;
    const unsigned isbs_before = mpu.isbs;
    const uint32_t writes_before = g_h743_model.writes;
    const auto ExpectUnchanged = [&] {
        EXPECT_EQ(zero_workspace, RamObjectBytes(other));
        EXPECT_EQ(workspace_before, RamObjectBytes(execution));
        EXPECT_EQ(port_before, RamObjectBytes(port));
        EXPECT_EQ(other_port_before, RamObjectBytes(other_port));
        EXPECT_EQ(cpu_before, RamObjectBytes(cpu));
        EXPECT_EQ(system_before, RamObjectBytes(g_nx_stm32h743_system));
        EXPECT_EQ(core_before, RamObjectBytes(*g_h743_model.core));
        EXPECT_EQ(flash_before, RamObjectBytes(g_h743_model.flash));
        EXPECT_EQ(timer_before, RamObjectBytes(g_h743_model.timer));
        EXPECT_EQ(clock_before, RamObjectBytes(g_h743_model.rcc));
        EXPECT_EQ(power_before, RamObjectBytes(g_h743_model.power));
        EXPECT_EQ(mpu_before, RamObjectBytes(mpu.registers));
        EXPECT_EQ(regions_before, mpu.regions);
        EXPECT_EQ(region_reads_before, mpu.region_reads);
        EXPECT_EQ(mpu_writes_before, mpu.writes.size());
        EXPECT_EQ(triggers_before, mpu.triggers.size());
        EXPECT_EQ(dsbs_before, mpu.dsbs);
        EXPECT_EQ(isbs_before, mpu.isbs);
        EXPECT_EQ(writes_before, g_h743_model.writes);
        EXPECT_EQ(0U, g_h743_model.flash_store_count);
    };
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32h743_flash_ram_initialize(&other_port, &other));
    ExpectUnchanged();
    /* BUSY acquired nothing. Stopping this still-zero workspace is an
     * idempotent no-op and cannot release the independently prepared owner. */
    const uint32_t reads_before = g_h743_model.reads;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&other));
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&other));
    EXPECT_EQ(reads_before, g_h743_model.reads);
    ExpectUnchanged();
    EXPECT_TRUE(execution.prepared);
    EXPECT_EQ(&port, execution.port);
    EXPECT_EQ(&execution, port.execution);
    EXPECT_EQ(0U, cpu.vtor_writes);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&execution));
    EXPECT_FALSE(execution.prepared);
    EXPECT_EQ(nullptr, port.execution);
    EXPECT_EQ(zero_workspace, RamObjectBytes(other));
    EXPECT_EQ(other_port_before, RamObjectBytes(other_port));
    EXPECT_EQ(0U, cpu.vtor_writes);
}
TEST_F(H743FlashRamOwner, CustomOriginalNmiOrHardfaultIsNotSilentlyReplaced) {
    for (size_t slot : {size_t{2U}, size_t{3U}}) {
        cpu.original_vectors[slot] = 0x08002001U;
        EXPECT_NE(NX_SUCCESS,
                  nx_stm32h743_flash_ram_initialize(&port, &execution));
        EXPECT_FALSE(execution.prepared);
        EXPECT_EQ(0U, cpu.vtor_writes);
        EXPECT_EQ(0U, g_h743_model.flash_store_count);
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&execution));
        cpu.original_vectors[slot] = cpu.default_handler;
    }
}
TEST_F(H743FlashRamOwner, RefusedPrivateVtorPublicationNeverTriggersFlash) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    cpu.refuse_vtor_publish = true;
    g_h743_model.flash.CR1 = FLASH_CR_PSIZE_1;
    std::array<uint8_t, 32U> input{};
    input.fill(0x5aU);
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 131072U, input.data(),
                                                 input.size(), 10000U));
    EXPECT_GT(cpu.refused_writes, 0U);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_PSIZE_1 | FLASH_CR_LOCK),
              g_h743_model.flash.CR1);
    EXPECT_EQ(0U, execution.guard.remaining_effects);
    EXPECT_EQ(cpu.original_vtor, g_h743_model.core->VTOR);
    EXPECT_EQ(0U, cpu.primask);
    EXPECT_FALSE(execution.active);
    EXPECT_FALSE(port.active);
}
TEST_F(H743FlashRamOwner, NmiAtRamPublicationTriggerAndDrainRetainsExecution) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    for (unsigned stage : {1U, 8U}) {
        cpu.callback_seen = false;
        cpu.inject_exception = 2U;
        cpu.inject_stage = stage;
        ASSERT_NO_FATAL_FAILURE(Program());
        EXPECT_TRUE(cpu.callback_seen);
        EXPECT_EQ(cpu.original_vtor, g_h743_model.core->VTOR);
        EXPECT_EQ(0U, cpu.primask);
        EXPECT_FALSE(execution.active);
        /* Reset external backing only to isolate the next boundary case. */
        std::fill_n(memory.data() + 131072U / sizeof(uint32_t), 8U, UINT32_MAX);
    }
    cpu.callback_seen = false;
    for (unsigned stage : {2U, 3U, 4U, 5U, 6U, 7U}) {
        SCOPED_TRACE(stage);
        EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
            cpu.inject_exception = 2U;
            cpu.inject_stage = stage;
            EXPECT_CALL(hardware, Terminal(testing::_)).WillOnce([&](auto) {
                nexus_test::RecordTerminalObservation(
                    cpu.callback_seen && cpu.primask == 1U &&
                    g_h743_model.core->VTOR == ram_vector_address &&
                    PrivateStack() && execution.active && port.active &&
                    execution.fault_exception == 2U &&
                    cpu.flash_fault_observers == 0U);
            });
            Program();
        }));
    }
}
TEST_F(H743FlashRamOwner,
       HardfaultUsesTheRamStackAndExplicitExceptionIdentity) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    cpu.control = 4U;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        cpu.inject_exception = 3U;
        cpu.inject_stage = 5U;
        EXPECT_CALL(hardware, Terminal(testing::_)).WillOnce([&](auto) {
            nexus_test::RecordTerminalObservation(
                PrivateStack() && cpu.primask == 1U && execution.active &&
                port.active && execution.fault_exception == 3U &&
                cpu.flash_fault_observers == 0U);
        });
        Program();
    }));
    /* NMI preemption and basic/extended frame writes are actual ARM checker
     * obligations, not asserted from this host C stack or handler callback. */
}
TEST_F(H743FlashRamOwner, RefusedVtorRestorationCannotReturnOrReleaseTheOwner) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        cpu.refuse_vtor_restore = true;
        EXPECT_CALL(hardware, Terminal(testing::_)).WillOnce([&](auto) {
            nexus_test::RecordTerminalObservation(
                cpu.refused_writes != 0U && cpu.primask == 1U &&
                PrivateStack() &&
                g_h743_model.core->VTOR == ram_vector_address &&
                execution.active && port.active &&
                (g_h743_model.flash.CR1 & FLASH_CR_LOCK) != 0U &&
                (g_h743_model.flash.SR1 & busy_flags) == 0U &&
                g_nx_stm32h743_system.started &&
                cpu.flash_fault_observers == 0U);
        });
        Program();
    }));
}
TEST_F(H743FlashRamPulse, EpochExhaustionCannotWrapBackToAValidZeroTime) {
    g_nx_stm32h743_system.overflow_us = UINT64_C(0xffffffff00000000);
    g_h743_model.timer.SR = TIM_SR_UIF;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        EXPECT_CALL(hardware, Terminal(NX_H743_FLASH_RAM_FAULT_TIME))
            .WillOnce([&] {
                nexus_test::RecordTerminalObservation(
                    g_nx_stm32h743_system.overflow_us ==
                        UINT64_C(0xffffffff00000000) &&
                    !g_nx_stm32h743_system.started &&
                    g_nx_stm32h743_system.timer_error == NX_ERROR_EXHAUSTED &&
                    execution.active && port.active && cpu.primask == 1U &&
                    cpu.flash_fault_observers == 0U);
            });
        (void)nx_stm32h743_flash_ram_pulse(&execution);
    }));
}
TEST_F(H743FlashRamOwner,
       ExpiredPulseStillDrainsLocksAndRestoresBeforeTimeout) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    g_h743_model.flash_poll_advance = 250U;
    std::array<uint8_t, 32U> input{};
    input.fill(0x5aU);
    EXPECT_CALL(hardware, Poll(testing::Ne(0U), true, true)).Times(2);
    EXPECT_CALL(hardware, Poll(0U, true, true)).Times(1);
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_flash_port_program(&face, 131072U, input.data(), input.size(),
                                    500U));
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    EXPECT_EQ(0U, g_h743_model.flash.SR1 & busy_flags);
    EXPECT_NE(0U, g_h743_model.flash.CR1 & FLASH_CR_LOCK);
    EXPECT_EQ(cpu.original_vtor, g_h743_model.core->VTOR);
    EXPECT_EQ(0U, cpu.primask);
    EXPECT_FALSE(execution.active);
    EXPECT_FALSE(port.active);
}

TEST_F(H743FlashRamOwner,
       CorrectedPreflightReadCannotBecomeSilentAllFfSuccess) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    std::array<uint8_t, 32U> input{};
    for (uint32_t offset : {131072U, 1048576U + 131072U}) {
        SCOPED_TRACE(offset);
        EXPECT_EXIT(
            {
                input.fill(0xffU);
                cpu.inject_read_ecc = true;
                cpu.read_ecc_offset = offset;
                const uint32_t writes = g_h743_model.writes;
                const nx_result_t result = nx_flash_port_program(
                    &face, offset, input.data(), input.size(), 10000U);
                const uint32_t status = offset < 1048576U
                                            ? g_h743_model.flash.SR1
                                            : g_h743_model.flash.SR2;
                std::_Exit(result == NX_ERROR_IO && cpu.read_ecc_events == 1U &&
                                   (status & FLASH_SR_SNECCERR) != 0U &&
                                   g_h743_model.flash_store_count == 0U &&
                                   g_h743_model.writes == writes &&
                                   cpu.bridge_entries == 0U &&
                                   cpu.vtor_writes == 0U && !execution.active &&
                                   !port.active
                               ? 0
                               : 1);
            },
            testing::ExitedWithCode(0), "");
    }
}
TEST_F(H743FlashRamOwner, CorrectedVerifyReadStopsBeforeAnotherPhysicalPulse) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    std::array<uint8_t, 64U> input{};
    for (uint32_t offset : {131072U, 1048576U + 131072U}) {
        SCOPED_TRACE(offset);
        EXPECT_EXIT(
            {
                input.fill(0x5aU);
                cpu.inject_read_ecc = true;
                cpu.read_ecc_offset = offset;
                cpu.read_ecc_after_stores = 8U;
                const nx_result_t result = nx_flash_port_program(
                    &face, offset, input.data(), input.size(), 10000U);
                const uint32_t status = offset < 1048576U
                                            ? g_h743_model.flash.SR1
                                            : g_h743_model.flash.SR2;
                const bool unchanged_next = std::all_of(
                    reinterpret_cast<const uint8_t*>(memory.data()) + offset +
                        32U,
                    reinterpret_cast<const uint8_t*>(memory.data()) + offset +
                        64U,
                    [](uint8_t byte) { return byte == 0xffU; });
                std::_Exit(
                    result == NX_ERROR_IO && cpu.read_ecc_events == 1U &&
                            (status & FLASH_SR_SNECCERR) != 0U &&
                            g_h743_model.flash_store_count == 8U &&
                            unchanged_next &&
                            ((g_h743_model.flash.SR1 | g_h743_model.flash.SR2) &
                             busy_flags) == 0U &&
                            g_h743_model.flash.CR1 == FLASH_CR_LOCK &&
                            g_h743_model.flash.CR2 == FLASH_CR_LOCK &&
                            execution.guard.remaining_effects == 0U &&
                            cpu.bridge_entries == 1U && cpu.primask == 0U &&
                            g_h743_model.core->VTOR == cpu.original_vtor &&
                            !execution.active && !port.active
                        ? 0
                        : 1);
            },
            testing::ExitedWithCode(0), "");
    }
}
TEST_F(H743FlashRamOwner, CorrectedPublicReadRetainsTheDiagnosticFlag) {
    ASSERT_NO_FATAL_FAILURE(Prepare());
    std::array<uint8_t, 32U> output{};
    for (uint32_t offset : {131072U, 1048576U + 131072U}) {
        SCOPED_TRACE(offset);
        EXPECT_EXIT(
            {
                cpu.inject_read_ecc = true;
                cpu.read_ecc_offset = offset;
                const uint32_t writes = g_h743_model.writes;
                const nx_result_t result = nx_flash_port_read(
                    &face, offset, output.data(), output.size());
                const uint32_t status = offset < 1048576U
                                            ? g_h743_model.flash.SR1
                                            : g_h743_model.flash.SR2;
                std::_Exit(result == NX_ERROR_IO && cpu.read_ecc_events == 1U &&
                                   (status & FLASH_SR_SNECCERR) != 0U &&
                                   std::all_of(output.begin(), output.end(),
                                               [](uint8_t byte) {
                                                   return byte == 0xffU;
                                               }) &&
                                   g_h743_model.flash_store_count == 0U &&
                                   g_h743_model.writes == writes &&
                                   cpu.bridge_entries == 0U &&
                                   !execution.active && !port.active
                               ? 0
                               : 1);
            },
            testing::ExitedWithCode(0), "");
    }
}
