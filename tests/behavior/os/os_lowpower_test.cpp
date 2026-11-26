/**
 * \file            os_lowpower_test.cpp
 * \brief           Real tickless algorithm against mocked timer CPU and kernel
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "FreeRTOS.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/sleep.h"
#include "nexus/os/lowpower.h"
#include "task.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
namespace {
class Machine {
  public:
    MOCK_METHOD(bool, Privileged, ());
    MOCK_METHOD(uint32_t, Exception, ());
    MOCK_METHOD(nx_arch_irq_masks_t, Masks, ());
    MOCK_METHOD(nx_arch_irq_state_t, Save, ());
    MOCK_METHOD(void, Restore, (uint32_t));
    MOCK_METHOD(eSleepModeStatus, Confirm, ());
    MOCK_METHOD(nx_result_t, Pause, (nx_freertos_tick_snapshot_t*));
    MOCK_METHOD(nx_result_t, Arm, (uint64_t));
    MOCK_METHOD(void, Disarm, ());
    MOCK_METHOD(nx_arch_result_t, Sleep, ());
    MOCK_METHOD(uint64_t, Now, ());
    MOCK_METHOD(void, Step, (TickType_t));
    MOCK_METHOD(BaseType_t, Increment, ());
    MOCK_METHOD(void, Resume, (uint64_t));
};
Machine* machine;
const nx_freertos_lowpower_port_t* selected;
uint64_t Now(void*) {
    return machine->Now();
}
nx_result_t Pause(void*, nx_freertos_tick_snapshot_t* s) {
    return machine->Pause(s);
}
nx_result_t Arm(void*, uint64_t d) {
    return machine->Arm(d);
}
void Disarm(void*) {
    machine->Disarm();
}
void Resume(void*, uint64_t d) {
    machine->Resume(d);
}
class Lowpower : public ::testing::Test {
  protected:
    StrictMock<Machine> model;
    nx_freertos_lowpower_port_t port = {nullptr, Now,    Pause,
                                        Arm,     Disarm, Resume};
    void SetUp() override {
        machine = &model;
        selected = &port;
    }
    void TearDown() override {
        selected = nullptr;
        machine = nullptr;
    }
    void Enter(eSleepModeStatus mode = eStandardSleep) {
        EXPECT_CALL(model, Privileged()).WillOnce(Return(true));
        EXPECT_CALL(model, Exception()).WillOnce(Return(0));
        EXPECT_CALL(model, Masks())
            .WillOnce(Return(nx_arch_irq_masks_t{0, 0, 0}));
        EXPECT_CALL(model, Save()).WillOnce(Return(nx_arch_irq_state_t{0}));
        EXPECT_CALL(model, Confirm()).WillOnce(Return(mode));
    }
    void PauseAt(uint64_t now = 10000, uint32_t phase = 250) {
        EXPECT_CALL(model, Pause(::testing::_))
            .WillOnce([now, phase](nx_freertos_tick_snapshot_t* s) {
                s->time_us = now;
                s->phase_us = phase;
                return NX_SUCCESS;
            });
    }
};
TEST_F(Lowpower, NoConfiguredPortLeavesPeriodicTickAlone) {
    selected = nullptr;
    nx_freertos_suppress_ticks_and_sleep(20);
}
TEST_F(Lowpower, MissingMethodDoesNotTouchKernel) {
    port.pause_tick = nullptr;
    nx_freertos_suppress_ticks_and_sleep(20);
}
TEST_F(Lowpower, ShortIdleWindowDoesNotPauseTick) {
    nx_freertos_suppress_ticks_and_sleep(0);
    nx_freertos_suppress_ticks_and_sleep(1);
}
TEST_F(Lowpower, IncomingMaskRejectsBeforeKernel) {
    EXPECT_CALL(model, Privileged()).WillOnce(Return(true));
    EXPECT_CALL(model, Exception()).WillOnce(Return(0));
    EXPECT_CALL(model, Masks()).WillOnce(Return(nx_arch_irq_masks_t{1, 0, 0}));
    nx_freertos_suppress_ticks_and_sleep(20);
}
TEST_F(Lowpower, PendingKernelWorkAbortsBeforeTickPause) {
    InSequence order;
    Enter(eAbortSleep);
    EXPECT_CALL(model, Restore(0));
    nx_freertos_suppress_ticks_and_sleep(20);
}
TEST_F(Lowpower, PendingTickPauseRejectionLeavesNormalTicks) {
    InSequence order;
    Enter();
    EXPECT_CALL(model, Pause(::testing::_)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(model, Restore(0));
    nx_freertos_suppress_ticks_and_sleep(20);
}
TEST_F(Lowpower, EarlyIrqPreservesPhaseAndStepsWholeTicks) {
    InSequence order;
    Enter();
    PauseAt();
    EXPECT_CALL(model, Arm(19750)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_OK));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(12500));
    EXPECT_CALL(model, Step(2));
    EXPECT_CALL(model, Resume(12750));
    EXPECT_CALL(model, Restore(0));
    nx_freertos_suppress_ticks_and_sleep(10);
}
TEST_F(Lowpower, DeadlineWakePendsFinalTickToUnblockTasks) {
    InSequence order;
    Enter();
    PauseAt();
    EXPECT_CALL(model, Arm(19750)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_OK));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(19750));
    EXPECT_CALL(model, Step(9));
    EXPECT_CALL(model, Increment()).WillOnce(Return(pdFALSE));
    EXPECT_CALL(model, Resume(20750));
    EXPECT_CALL(model, Restore(0));
    nx_freertos_suppress_ticks_and_sleep(10);
}
TEST_F(Lowpower, LateWakeIsCaughtUpWithBoundedPendedTicks) {
    InSequence order;
    Enter();
    PauseAt();
    EXPECT_CALL(model, Arm(19750)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_OK));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(21750));
    EXPECT_CALL(model, Step(9));
    EXPECT_CALL(model, Increment()).Times(3).WillRepeatedly(Return(pdFALSE));
    EXPECT_CALL(model, Resume(22750));
    EXPECT_CALL(model, Restore(0));
    nx_freertos_suppress_ticks_and_sleep(10);
}
TEST_F(Lowpower, FailedAlarmRestoresPeriodicTickWithoutSleeping) {
    InSequence order;
    Enter();
    PauseAt();
    EXPECT_CALL(model, Arm(19750)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(model, Now()).WillOnce(Return(10250));
    EXPECT_CALL(model, Resume(10750));
    EXPECT_CALL(model, Restore(0));
    nx_freertos_suppress_ticks_and_sleep(10);
}
TEST_F(Lowpower, SubTickWakeDoesNotAdvanceKernel) {
    InSequence order;
    Enter();
    PauseAt();
    EXPECT_CALL(model, Arm(19750)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_OK));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(10100));
    EXPECT_CALL(model, Resume(10750));
    EXPECT_CALL(model, Restore(0));
    nx_freertos_suppress_ticks_and_sleep(10);
}
TEST_F(Lowpower, NoDeepSleepSupportStillRestoresTheTick) {
    InSequence order;
    Enter();
    PauseAt();
    EXPECT_CALL(model, Arm(19750)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_UNSUPPORTED));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(10100));
    EXPECT_CALL(model, Resume(10750));
    EXPECT_CALL(model, Restore(0));
    nx_freertos_suppress_ticks_and_sleep(10);
}
TEST_F(Lowpower, ReversedClockFailsStopBeforeKernelTimeMutation) {
    InSequence order;
    Enter();
    PauseAt();
    EXPECT_CALL(model, Arm(19750)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_OK));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(9999));
    EXPECT_THROW(nx_freertos_suppress_ticks_and_sleep(10), std::runtime_error);
}
TEST_F(Lowpower, ExcessiveAlarmLatenessFailsStopInsteadOfUnboundedCatchup) {
    InSequence order;
    Enter();
    PauseAt();
    EXPECT_CALL(model, Arm(19750)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_OK));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(28750));
    EXPECT_THROW(nx_freertos_suppress_ticks_and_sleep(10), std::runtime_error);
}
TEST_F(Lowpower, InvalidTickPhaseFailsStopBeforeProgrammingAlarm) {
    InSequence order;
    Enter();
    PauseAt(10000, 1000);
    EXPECT_THROW(nx_freertos_suppress_ticks_and_sleep(10), std::runtime_error);
}
TEST_F(Lowpower, AlarmDeadlineOverflowFailsStopBeforeProgrammingAlarm) {
    InSequence order;
    Enter();
    PauseAt(UINT64_MAX - 500, 250);
    EXPECT_THROW(nx_freertos_suppress_ticks_and_sleep(10), std::runtime_error);
}
TEST_F(Lowpower, UnprivilegedCallDoesNotPauseTick) {
    EXPECT_CALL(model, Privileged()).WillOnce(Return(false));
    nx_freertos_suppress_ticks_and_sleep(10);
}
TEST_F(Lowpower, HandlerCallDoesNotPauseTick) {
    EXPECT_CALL(model, Privileged()).WillOnce(Return(true));
    EXPECT_CALL(model, Exception()).WillOnce(Return(16));
    nx_freertos_suppress_ticks_and_sleep(10);
}
TEST_F(Lowpower, ElapsedClockAdditionOverflowFailsStopBeforeKernelMutation) {
    InSequence order;
    Enter();
    PauseAt(0, 250);
    EXPECT_CALL(model, Arm(9750)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_OK));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(UINT64_MAX));
    EXPECT_THROW(nx_freertos_suppress_ticks_and_sleep(10), std::runtime_error);
}
TEST_F(Lowpower, NextPeriodicExpiryOverflowFailsStopBeforeKernelMutation) {
    InSequence order;
    Enter();
    PauseAt(UINT64_MAX - 11000, 250);
    EXPECT_CALL(model, Arm(UINT64_MAX - 1250)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(model, Sleep()).WillOnce(Return(NX_ARCH_OK));
    EXPECT_CALL(model, Disarm());
    EXPECT_CALL(model, Now()).WillOnce(Return(UINT64_MAX));
    EXPECT_THROW(nx_freertos_suppress_ticks_and_sleep(10), std::runtime_error);
}
} /* namespace */
extern "C" const nx_freertos_lowpower_port_t* nx_freertos_lowpower_port(void) {
    return selected;
}
extern "C" bool nx_arch_is_privileged(void) {
    return machine->Privileged();
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return machine->Exception();
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return machine->Masks();
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return machine->Save();
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t s) {
    machine->Restore(s.value);
}
extern "C" nx_arch_result_t nx_arch_wait_for_interrupt(void) {
    return machine->Sleep();
}
extern "C" eSleepModeStatus eTaskConfirmSleepModeStatus(void) {
    return machine->Confirm();
}
extern "C" void vTaskStepTick(TickType_t n) {
    machine->Step(n);
}
extern "C" BaseType_t xTaskIncrementTick(void) {
    return machine->Increment();
}
extern "C" void nx_freertos_assert_failed(const char*, unsigned) {
    throw std::runtime_error("invalid tickless clock");
}
