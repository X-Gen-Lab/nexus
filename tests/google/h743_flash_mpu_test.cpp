/**
 * \file            h743_flash_mpu_test.cpp
 * \brief           Test-first complete RAM transaction and protection effects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_mpu_support.h"
#include "terminal_watchdog.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

using namespace nexus_test;
using h743_mpu::mpu;
using h743_mpu::Register;
using h743_ram::cpu;

namespace {
template <class T> auto Bytes(const T& object) {
    std::array<unsigned char, sizeof(T)> result{};
    std::memcpy(result.data(), &object, result.size());
    return result;
}
} /* namespace */

class H743FlashMpu : public testing::Test {
  protected:
    std::array<uint32_t, 524288U> memory{};
    nx_stm32h743_flash_state_t port{};
    alignas(1024) nx_stm32h743_flash_ram_workspace_t execution{};
    nx_stm32h743_flash_mpu_state_t guard{};
    testing::NiceMock<h743_ram::RamHardwareBoundary> hardware;
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
        g_h743_model.flash_poll_advance = 1U;
        g_h743_model.core->VTOR = h743_ram::ram_vector_address;
        /* BP is read-only, always one; cache-off never means BP-off. */
        g_h743_model.core->CCR = UINT32_C(1) << 18U;
        execution.port = &port;
        execution.system = &g_nx_stm32h743_system;
        execution.destination = memory.data() + 131072U / sizeof(uint32_t);
        for (unsigned i = 0U; i < 8U; ++i)
            execution.words[i] = 0x55aa0000U + i;
        execution.configuration = FLASH_CR_PG;
        execution.saved_control = FLASH_CR_PSIZE_1;
        execution.program = execution.prepared = execution.active = true;
        h743_ram::ResetRamCpu(&execution);
        cpu.observer = &hardware;
        cpu.primask = 1U;
        mpu.Reset();
        mpu.guard = &guard;
    }
    void TearDown() override {
        /* The parent owns no terminal case: each runs in a child. Do not clear
         * a busy bit or protection effect to manufacture safe reclamation. */
        const uint32_t pending =
            (g_h743_model.flash.SR1 | g_h743_model.flash.SR2) &
            h743_ram::busy_flags;
        EXPECT_EQ(0U, pending);
        EXPECT_EQ(0U, guard.remaining_effects);
        EXPECT_EQ(0U, execution.guard.remaining_effects);
        EXPECT_EQ(0U, mpu.ccr_writes);
        EXPECT_EQ(0U, cpu.flash_fault_observers);
        if (pending != 0U || guard.remaining_effects != 0U ||
            execution.guard.remaining_effects != 0U)
            std::abort();
        cpu.execution = nullptr;
        cpu.observer = nullptr;
    }
    nx_result_t Call(nx_time_us_t deadline = 10000U) {
        return nx_stm32h743_flash_ram_guarded_transaction(&execution, &guard,
                                                          deadline);
    }
    void CheckGuard(bool handler = false) {
        EXPECT_TRUE(h743_mpu::InsidePod());
        EXPECT_TRUE(h743_ram::PrivateStack());
        EXPECT_EQ(h743_ram::ram_vector_address, g_h743_model.core->VTOR);
        EXPECT_EQ(1U, cpu.primask);
        EXPECT_EQ(h743_mpu::guard_control, mpu.registers.CTRL);
        EXPECT_TRUE(h743_mpu::GuardHasArchitecturalAttributes(handler));
        EXPECT_FALSE(h743_mpu::CodeInstructionPermitted(handler));
        EXPECT_FALSE(h743_mpu::CodeSpeculativeReadPermitted(handler));
        EXPECT_TRUE(h743_mpu::ExplicitFlashWordStorePermitted(handler));
        EXPECT_NE(0U, guard.remaining_effects);
    }
    void CheckInternalReturn(uint32_t control, uint32_t selected,
                             const std::array<h743_mpu::Region, 16U>& regions) {
        EXPECT_EQ(0U, (g_h743_model.flash.SR1 | g_h743_model.flash.SR2) &
                          h743_ram::busy_flags);
        EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_PSIZE_1 | FLASH_CR_LOCK),
                  *h743_ram::Control());
        EXPECT_EQ(control, mpu.registers.CTRL);
        EXPECT_EQ(selected, mpu.registers.RNR);
        EXPECT_EQ(regions, mpu.regions);
        EXPECT_EQ(0U, guard.remaining_effects);
        EXPECT_EQ(0U, mpu.unsafe_triggers);
        EXPECT_GT(mpu.control_read, mpu.control_write);
        EXPECT_GT(mpu.base_read, mpu.base_write);
        EXPECT_GT(mpu.attributes_read, mpu.attributes_write);
        EXPECT_GT(mpu.last_isb, mpu.control_write);
        EXPECT_GT(mpu.last_isb, mpu.base_write);
        EXPECT_GT(mpu.last_isb, mpu.attributes_write);
        bool post_trigger_dsb = false;
        for (size_t i = mpu.writes_at_last_trigger; i < mpu.writes.size();
             ++i) {
            const auto& write = mpu.writes[i];
            if (write.reg == Register::Dsb)
                post_trigger_dsb = true;
            const bool withdraw =
                (write.reg == Register::Control && write.value != 7U) ||
                (write.reg == Register::Attributes &&
                 write.value != h743_mpu::guard_attributes) ||
                (write.reg == Register::Base &&
                 (write.value & ~UINT32_C(0x1f)) != 0U);
            if (withdraw) {
                EXPECT_TRUE(post_trigger_dsb);
                EXPECT_EQ(0U, write.pending);
                EXPECT_NE(0U, write.flash_control & FLASH_CR_LOCK);
            }
        }
        /* This internal return reaches RAM, not a normal Flash call frame.
         * The later real bridge must independently restore VTOR/CPU. */
        EXPECT_TRUE(h743_mpu::InsidePod());
        EXPECT_TRUE(h743_ram::PrivateStack());
        EXPECT_EQ(h743_ram::ram_vector_address, g_h743_model.core->VTOR);
    }
    void CheckNoTrigger() {
        EXPECT_EQ(0U, g_h743_model.flash_store_count);
        EXPECT_FALSE(cpu.triggered);
        EXPECT_TRUE(mpu.triggers.empty());
    }
    void ObserveGuardedTerminal(nx_stm32h743_flash_mpu_fault_t reason) {
        EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
            EXPECT_CALL(hardware, Terminal(testing::_))
                .WillOnce([&](nx_stm32h743_flash_ram_fault_t) {
                    nexus_test::RecordTerminalObservation(
                        h743_mpu::InsidePod() && h743_ram::PrivateStack() &&
                        g_h743_model.core->VTOR ==
                            h743_ram::ram_vector_address &&
                        cpu.primask == 1U && execution.active && port.active &&
                        guard.remaining_effects != 0U &&
                        guard.fault == reason &&
                        cpu.flash_fault_observers == 0U);
                });
            (void)Call();
        }));
    }
};

TEST_F(H743FlashMpu, HighestRegionProtectsAllEightExplicitProgramStores) {
    mpu.registers.CTRL = 4U;
    cpu.mpu_control = 4U;
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    EXPECT_CALL(hardware, Trigger(true, true, true))
        .Times(8)
        .WillRepeatedly([&](bool, bool, bool) { CheckGuard(); });
    ASSERT_EQ(NX_SUCCESS, Call());
    ASSERT_EQ(8U, mpu.triggers.size());
    for (unsigned i = 0U; i < 8U; ++i)
        EXPECT_EQ(execution.words[i], execution.destination[i]);
    for (unsigned reads : mpu.region_reads)
        EXPECT_GT(reads, 0U);
    EXPECT_EQ(15U, guard.region);
    EXPECT_EQ(UINT32_C(1) << 18U, g_h743_model.core->CCR);
    CheckInternalReturn(4U, selected, regions);
}
TEST_F(H743FlashMpu, EraseStartHasTheSameRamGuardAndNoProgramStore) {
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    execution.program = false;
    execution.destination = nullptr;
    execution.configuration = static_cast<uint32_t>(
        FLASH_CR_SER | FLASH_CR_PSIZE_1 | (1U << FLASH_CR_SNB_Pos));
    memory[131072U / sizeof(uint32_t)] = 0U;
    EXPECT_CALL(hardware, Trigger(true, true, true))
        .WillOnce([&](bool, bool, bool) { CheckGuard(); });
    ASSERT_EQ(NX_SUCCESS, Call());
    ASSERT_EQ(1U, mpu.triggers.size());
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_EQ(UINT32_MAX, memory[131072U / sizeof(uint32_t)]);
    CheckInternalReturn(0U, selected, regions);
}
TEST_F(H743FlashMpu, UnreviewedMpuTypeCannotSelectOrActivateAnyRegion) {
    for (uint32_t type : {0U, 8U << 8U, 17U << 8U, (16U << 8U) | 1U,
                          (16U << 8U) | (1U << 16U)}) {
        mpu.type = type;
        const auto regions = mpu.regions;
        const uint32_t selected = mpu.registers.RNR;
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, Call());
        EXPECT_EQ(regions, mpu.regions);
        EXPECT_EQ(selected, mpu.registers.RNR);
        EXPECT_TRUE(mpu.writes.empty());
        EXPECT_EQ(0U, mpu.registers.CTRL);
        EXPECT_EQ(0U, guard.remaining_effects);
        CheckNoTrigger();
    }
}
TEST_F(H743FlashMpu, DisabledControlDoesNotAuthorizeAnyEnabledOldDescriptor) {
    const auto original = mpu.regions;
    for (unsigned i = 0U; i < mpu.regions.size(); ++i) {
        mpu.regions[i].attributes |= 1U;
        const auto regions = mpu.regions;
        const uint32_t selected = mpu.registers.RNR;
        EXPECT_EQ(NX_ERROR_STATE, Call());
        EXPECT_EQ(regions, mpu.regions);
        EXPECT_EQ(selected, mpu.registers.RNR);
        EXPECT_EQ(0U, mpu.registers.CTRL);
        for (const auto& write : mpu.writes)
            EXPECT_TRUE(write.reg == Register::Selector ||
                        write.reg == Register::Dsb ||
                        write.reg == Register::Isb);
        EXPECT_EQ(0U, guard.remaining_effects);
        CheckNoTrigger();
        mpu.regions = original;
        mpu.writes.clear();
    }
}
TEST_F(H743FlashMpu, ActiveOrUnpredictableIncomingControlIsNotAdopted) {
    for (uint32_t control : {1U, 2U, 3U, 5U, 6U, 7U}) {
        mpu.registers.CTRL = cpu.mpu_control = control;
        const auto regions = mpu.regions;
        EXPECT_EQ(NX_ERROR_STATE, Call());
        EXPECT_EQ(control, mpu.registers.CTRL);
        EXPECT_EQ(regions, mpu.regions);
        EXPECT_TRUE(mpu.writes.empty());
        CheckNoTrigger();
    }
    mpu.registers.CTRL = cpu.mpu_control = 0U;
}
TEST_F(H743FlashMpu, RawContextAndCacheAreReobservedWithoutChangingPolicy) {
    struct Context {
        uint32_t ipsr;
        uint32_t control;
        uint32_t primask;
        uint32_t basepri;
        uint32_t faultmask;
    };
    const std::array<Context, 5U> bad{{{17U, 0U, 1U, 0U, 0U},
                                       {0U, 1U, 1U, 0U, 0U},
                                       {0U, 0U, 0U, 0U, 0U},
                                       {0U, 0U, 1U, 0x50U, 0U},
                                       {0U, 0U, 1U, 0U, 1U}}};
    g_h743_model.privileged = true;
    for (const auto& context : bad) {
        cpu.ipsr = context.ipsr;
        cpu.control = context.control;
        cpu.primask = context.primask;
        cpu.basepri = context.basepri;
        cpu.faultmask = context.faultmask;
        EXPECT_EQ(NX_ERROR_CONTEXT, Call());
        EXPECT_TRUE(mpu.writes.empty());
        CheckNoTrigger();
    }
    cpu.ipsr = cpu.control = cpu.basepri = cpu.faultmask = 0U;
    cpu.primask = 1U;
    for (uint32_t cache : {static_cast<uint32_t>(SCB_CCR_DC_Msk),
                           static_cast<uint32_t>(SCB_CCR_IC_Msk)}) {
        g_h743_model.core->CCR = (UINT32_C(1) << 18U) | cache;
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, Call());
        EXPECT_TRUE(mpu.writes.empty());
        EXPECT_EQ((UINT32_C(1) << 18U) | cache, g_h743_model.core->CCR);
        CheckNoTrigger();
    }
    g_h743_model.core->CCR = UINT32_C(1) << 18U;
}
TEST_F(H743FlashMpu, ActualPrivatePcMspAndVtorAreRequiredAtGuardEntry) {
    const uintptr_t original_pc = mpu.pc;
    for (uintptr_t pc :
         {uintptr_t{0x08000001U}, static_cast<uintptr_t>(cpu.code_start - 1U),
          static_cast<uintptr_t>(cpu.code_end)}) {
        mpu.pc = pc;
        EXPECT_EQ(NX_ERROR_STATE, Call());
        EXPECT_TRUE(mpu.writes.empty());
    }
    mpu.pc = original_pc;
    const uintptr_t original_msp = cpu.msp;
    cpu.msp = cpu.psp;
    EXPECT_EQ(NX_ERROR_STATE, Call());
    EXPECT_TRUE(mpu.writes.empty());
    cpu.msp = original_msp;
    cpu.control = 2U;
    EXPECT_EQ(NX_ERROR_STATE, Call());
    EXPECT_TRUE(mpu.writes.empty());
    cpu.control = 0U;
    g_h743_model.core->VTOR = cpu.original_vtor;
    EXPECT_EQ(NX_ERROR_STATE, Call());
    EXPECT_TRUE(mpu.writes.empty());
    g_h743_model.core->VTOR = h743_ram::ram_vector_address;
    CheckNoTrigger();
}
TEST_F(H743FlashMpu, RefusedInstallRollsBackOnlyWhileBothBanksAreStillIdle) {
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    for (const auto refusal :
         {h743_mpu::Refusal{Register::Attributes, h743_mpu::guard_attributes,
                            false},
          h743_mpu::Refusal{Register::Control, h743_mpu::guard_control,
                            false}}) {
        mpu.refusals = {refusal};
        EXPECT_EQ(NX_ERROR_IO, Call());
        EXPECT_EQ(regions, mpu.regions);
        EXPECT_EQ(selected, mpu.registers.RNR);
        EXPECT_EQ(0U, mpu.registers.CTRL);
        EXPECT_EQ(0U, guard.remaining_effects);
        for (const auto& write : mpu.writes)
            EXPECT_EQ(0U, write.pending);
        CheckNoTrigger();
        mpu.writes.clear();
    }
}
TEST_F(H743FlashMpu, UnprovedPreTriggerRollbackCannotReturnIntoFlash) {
    mpu.refusals = {{Register::Control, h743_mpu::guard_control, false},
                    {Register::Attributes, mpu.regions[15U].attributes, false}};
    ObserveGuardedTerminal(NX_H743_FLASH_MPU_FAULT_RESTORE);
    /* RNR may be restored before the pulse. Do not force a needless final
     * selector write; refuse its real cold-scan rollback instead. */
    mpu.refusals = {{Register::Selector, mpu.registers.RNR, false}};
    ObserveGuardedTerminal(NX_H743_FLASH_MPU_FAULT_RESTORE);
    CheckNoTrigger();
}
TEST_F(H743FlashMpu, AlreadyExpiredDeadlineDoesNotStartAnyPhysicalPulse) {
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    g_h743_model.timer.CNT = 20U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, Call(10U));
    CheckNoTrigger();
    EXPECT_EQ(regions, mpu.regions);
    EXPECT_EQ(selected, mpu.registers.RNR);
    EXPECT_EQ(0U, mpu.registers.CTRL);
    EXPECT_EQ(0U, guard.remaining_effects);
}
TEST_F(H743FlashMpu, ExpiryLatchesTimeoutAndDrainsNaturallyWithoutWithdrawing) {
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    cpu.clear_after = 12U;
    unsigned after_deadline_busy = 0U;
    EXPECT_CALL(hardware, Poll(testing::_, true, true))
        .Times(12)
        .WillRepeatedly([&](uint32_t pending, bool, bool) {
            CheckGuard();
            if (g_h743_model.timer.CNT >= 5U && pending != 0U)
                ++after_deadline_busy;
        });
    EXPECT_EQ(NX_ERROR_TIMEOUT, Call(5U));
    EXPECT_GE(after_deadline_busy, 7U);
    EXPECT_GE(cpu.polls, 12U);
    EXPECT_TRUE(guard.timed_out);
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    CheckInternalReturn(0U, selected, regions);
}
TEST_F(H743FlashMpu, QueueAndWriteBufferEachRemainProtectedPastTheDeadline) {
    for (uint32_t pending : {static_cast<uint32_t>(FLASH_SR_QW),
                             static_cast<uint32_t>(FLASH_SR_WBNE)}) {
        EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
            cpu.pending = pending;
            cpu.clear_after = 0U;
            bool recorded = false;
            EXPECT_CALL(hardware, Poll(pending, true, true))
                .WillRepeatedly([&](uint32_t, bool, bool) {
                    if (!recorded && cpu.polls >= 20U) {
                        recorded = true;
                        nexus_test::RecordTerminalObservation(
                            h743_mpu::GuardHasArchitecturalAttributes(false) &&
                            h743_mpu::InsidePod() && h743_ram::PrivateStack() &&
                            (g_h743_model.flash.SR1 & pending) != 0U &&
                            guard.remaining_effects != 0U &&
                            g_h743_model.core->VTOR ==
                                h743_ram::ram_vector_address &&
                            cpu.flash_fault_observers == 0U);
                    }
                });
            (void)Call(5U);
        }));
    }
}
TEST_F(H743FlashMpu, PermanentBsyNeverReturnsOrRunsAFlashTerminalObserver) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        cpu.pending = FLASH_SR_BSY;
        cpu.clear_after = 0U;
        bool recorded = false;
        EXPECT_CALL(hardware, Poll(FLASH_SR_BSY, true, true))
            .WillRepeatedly([&](uint32_t pending, bool, bool) {
                if (!recorded && cpu.polls >= 20U) {
                    recorded = true;
                    nexus_test::RecordTerminalObservation(
                        pending != 0U && guard.remaining_effects != 0U &&
                        h743_mpu::GuardHasArchitecturalAttributes(false) &&
                        h743_mpu::InsidePod() && h743_ram::PrivateStack() &&
                        cpu.primask == 1U && execution.active && port.active &&
                        cpu.flash_fault_observers == 0U);
                }
            });
        (void)Call(5U);
    }));
}
TEST_F(H743FlashMpu, LockReadbackFailureKeepsProtectionAndRamResponsibility) {
    cpu.reject_lock = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        EXPECT_CALL(hardware, Terminal(testing::_))
            .WillOnce([&](nx_stm32h743_flash_ram_fault_t) {
                nexus_test::RecordTerminalObservation(
                    guard.fault == NX_H743_FLASH_MPU_FAULT_LOCK &&
                    h743_mpu::GuardHasArchitecturalAttributes(false) &&
                    h743_mpu::InsidePod() && h743_ram::PrivateStack() &&
                    guard.remaining_effects != 0U && execution.active &&
                    port.active &&
                    (*h743_ram::Control() & FLASH_CR_LOCK) == 0U);
            });
        (void)Call();
    }));
}
TEST_F(H743FlashMpu, EveryUnprovedProtectionRestoreParksInsteadOfReturning) {
    const std::array<h743_mpu::Refusal, 3U> refusals{
        {{Register::Control, 0U, true},
         {Register::Attributes, mpu.regions[15U].attributes, true},
         {Register::Base, mpu.regions[15U].base, true, ~UINT32_C(0x1f)}}};
    for (const auto& refusal : refusals) {
        mpu.refusals = {refusal};
        ObserveGuardedTerminal(NX_H743_FLASH_MPU_FAULT_RESTORE);
    }
}
TEST_F(H743FlashMpu, NmiAndHardFaultTransitionsUseOnlyPrivateRamClosure) {
    struct Injection {
        Register reg;
        uint32_t value;
        bool after;
    };
    const std::array<Injection, 5U> stages{
        {{Register::Control, h743_mpu::guard_control, false},
         {Register::Control, h743_mpu::guard_control, true},
         {Register::Other, 2U, true},
         {Register::Control, 0U, false},
         {Register::Control, 0U, true}}};
    for (unsigned exception : {2U, 3U}) {
        for (const auto& stage : stages) {
            EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
                mpu.inject_exception = exception;
                mpu.inject_register = stage.reg;
                mpu.inject_value = stage.value;
                mpu.inject_after = stage.after;
                EXPECT_CALL(hardware,
                            Terminal(NX_H743_FLASH_RAM_FAULT_EXCEPTION))
                    .WillOnce([&](nx_stm32h743_flash_ram_fault_t) {
                        const bool busy =
                            ((g_h743_model.flash.SR1 | g_h743_model.flash.SR2) &
                             h743_ram::busy_flags) != 0U;
                        nexus_test::RecordTerminalObservation(
                            mpu.injected && cpu.ipsr == exception &&
                            h743_mpu::InsidePod() && h743_ram::PrivateStack() &&
                            g_h743_model.core->VTOR ==
                                h743_ram::ram_vector_address &&
                            execution.active && port.active &&
                            (!busy ||
                             (h743_mpu::GuardHasArchitecturalAttributes(true) &&
                              !h743_mpu::CodeSpeculativeReadPermitted(true))) &&
                            cpu.flash_fault_observers == 0U);
                    });
                (void)Call();
            }));
        }
    }
}
TEST_F(H743FlashMpu, CapturedErrorStillDrainsLocksAndRestoresExactOldPolicy) {
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    cpu.pulse_error = true;
    EXPECT_EQ(NX_ERROR_IO, Call());
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    CheckInternalReturn(0U, selected, regions);
}
TEST_F(H743FlashMpu, SidecarAliasesRejectBeforeAnyReadOrMutationOfThatControl) {
    const auto before_execution = Bytes(execution);
    const auto before_port = Bytes(port);
    const auto before_system = Bytes(g_nx_stm32h743_system);
    const auto before_guard = Bytes(guard);
    const auto before_regions = mpu.regions;
    const uintptr_t first = reinterpret_cast<uintptr_t>(&execution);
    const std::array<void*, 13U> aliases{
        {&execution, execution.stack, execution.words, &port,
         &g_nx_stm32h743_system, &g_h743_model.flash, cpu.code_run.data(),
         cpu.code_source.data(), cpu.original_vectors.data(), &mpu.registers,
         reinterpret_cast<void*>(first - sizeof(guard) + 4U),
         reinterpret_cast<void*>(first + sizeof(execution) - 4U),
         &g_h743_model.timer}};
    for (void* pointer : aliases) {
        EXPECT_EXIT(
            {
                const nx_result_t result =
                    nx_stm32h743_flash_ram_guarded_transaction(
                        &execution,
                        static_cast<nx_stm32h743_flash_mpu_state_t*>(pointer),
                        10000U);
                std::_Exit(result == NX_ERROR_INVALID &&
                                   before_execution == Bytes(execution) &&
                                   before_port == Bytes(port) &&
                                   before_system ==
                                       Bytes(g_nx_stm32h743_system) &&
                                   before_guard == Bytes(guard) &&
                                   before_regions == mpu.regions &&
                                   mpu.writes.empty() &&
                                   g_h743_model.flash_store_count == 0U
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
            const nx_result_t result =
                nx_stm32h743_flash_ram_guarded_transaction(
                    &execution,
                    static_cast<nx_stm32h743_flash_mpu_state_t*>(unreadable),
                    10000U);
            std::_Exit(result == NX_ERROR_INVALID && mpu.writes.empty() &&
                               before_execution == Bytes(execution) &&
                               before_port == Bytes(port) &&
                               before_system == Bytes(g_nx_stm32h743_system)
                           ? 0
                           : 1);
        },
        testing::ExitedWithCode(0), "");
    EXPECT_EQ(0, munmap(unreadable, 4096U));
    cpu.alias_pointer = cpu.alias_address = 0U;
}
TEST_F(H743FlashMpu, OtherBankQueueMustAlsoBeReallyIdleBeforeGuardWithdrawal) {
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    unsigned target_idle_peer_busy = 0U;
    EXPECT_CALL(hardware, Poll(testing::_, true, true))
        .Times(10)
        .WillRepeatedly([&](uint32_t target_pending, bool, bool) {
            CheckGuard();
            if (cpu.polls == 1U)
                g_h743_model.flash.SR2 = FLASH_SR_QW;
            if (cpu.polls >= 3U && cpu.polls < 10U) {
                EXPECT_EQ(0U, target_pending);
                EXPECT_NE(0U, g_h743_model.flash.SR2 & FLASH_SR_QW);
                ++target_idle_peer_busy;
            }
            if (cpu.polls == 10U)
                g_h743_model.flash.SR2 = 0U;
        });
    EXPECT_EQ(NX_ERROR_TIMEOUT, Call(5U));
    EXPECT_EQ(7U, target_idle_peer_busy);
    CheckInternalReturn(0U, selected, regions);
    for (const auto& write : mpu.writes) {
        if (write.reg == Register::Control && write.value != 7U) {
            EXPECT_EQ(0U, write.pending);
            EXPECT_NE(0U, write.flash_control & FLASH_CR_LOCK);
        }
    }
}

TEST_F(H743FlashMpu, ExactEmbeddedGuardOwnsOnlyItsWorkspaceTail) {
    mpu.guard = &execution.guard;
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    EXPECT_CALL(hardware, Trigger(true, true, true))
        .Times(8)
        .WillRepeatedly([&](bool, bool, bool) {
            EXPECT_TRUE(h743_mpu::InsidePod());
            EXPECT_TRUE(h743_ram::PrivateStack());
            EXPECT_TRUE(h743_mpu::GuardHasArchitecturalAttributes(false));
            EXPECT_NE(0U, execution.guard.remaining_effects);
            EXPECT_EQ(0U, guard.remaining_effects);
        });
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_guarded_transaction(
                              &execution, &execution.guard, 10000U));
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    EXPECT_EQ(15U, execution.guard.region);
    EXPECT_EQ(0U, execution.guard.remaining_effects);
    EXPECT_FALSE(execution.guard.timed_out);
    EXPECT_EQ(NX_H743_FLASH_MPU_FAULT_NONE, execution.guard.fault);
    CheckInternalReturn(0U, selected, regions);
}

TEST_F(H743FlashMpu, InsufficientOrUnalignedPrivateMspCannotInstallGuard) {
    const uintptr_t first = reinterpret_cast<uintptr_t>(execution.stack);
    for (uintptr_t offset :
         {uintptr_t{0U}, uintptr_t{8U}, uintptr_t{216U}, uintptr_t{225U}}) {
        cpu.msp = first + offset;
        ASSERT_EQ(NX_ERROR_STATE, Call());
        EXPECT_TRUE(mpu.writes.empty());
        EXPECT_EQ(0U, guard.remaining_effects);
        CheckNoTrigger();
    }
}

TEST_F(H743FlashMpu, DeadlineReachedDuringArmSuppressesPhysicalIssue) {
    const auto regions = mpu.regions;
    const uint32_t selected = mpu.registers.RNR;
    g_h743_model.timer.CNT = 1U;
    mpu.advance_time_on_arm = 4U;
    ASSERT_EQ(NX_ERROR_TIMEOUT, Call(5U));
    EXPECT_EQ(1U, mpu.arm_time_advances);
    EXPECT_GE(g_h743_model.timer.CNT, 5U);
    EXPECT_TRUE(guard.timed_out);
    CheckNoTrigger();
    CheckInternalReturn(0U, selected, regions);
}
