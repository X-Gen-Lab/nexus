/**
 * \file            freertos_stop_test.cpp
 * \brief           Actual selected Idle orchestration at owned port boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "FreeRTOS.h"
#include "nexus/arch/arch.h"
#include "nexus/arch/sleep.h"
#include "nexus/components/clock_domain_owner.h"
#include "nexus/os/lowpower.h"
#include "nexus/soc/stm32f4_freertos_stop.h"
#include "system/stm32f4_idle_power.h"
#include "task.h"
#include "timebase_lowpower.h"
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
nx_stm32_freertos_stop_t owner;
nx_stm32_freertos_stop_t* selected;
nx_stm32_freertos_stop_config_t config;
nx_clock_domain_t domain;
uint64_t now;
uint32_t mask;
unsigned pauses, handoffs, restores, powers, acks, steps, confirms;
uint32_t stepped;
uint32_t rtc_sample, rtc_delta, rtc_before, kernel_delay;
nx_result_t pause_result, handoff_result, power_result, sample_result;
nx_result_t after_sample_result, ack_result;
eSleepModeStatus sleep_status;
uint64_t resume_at;
uint32_t phase;
bool pending, privileged, changed_epoch_failure;
class FreeRtosStop : public testing::Test {
  protected:
    void SetUp() override {
        owner = {};
        selected = &owner;
        now = 10000U;
        mask = 0U;
        pauses = handoffs = restores = powers = acks = steps = confirms = 0U;
        stepped = rtc_sample = 0U;
        rtc_delta = 32U;
        rtc_before = 1000U;
        kernel_delay = 0U;
        after_sample_result = ack_result = NX_SUCCESS;
        phase = 250U;
        resume_at = 0U;
        pending = false;
        privileged = true;
        changed_epoch_failure = false;
        pause_result = handoff_result = power_result = sample_result =
            NX_SUCCESS;
        sleep_status = eStandardSleep;
        domain = {};
        ASSERT_EQ(nx_clock_domain_construct(&domain, 1U), NX_SUCCESS);
        config = {};
        ASSERT_EQ(nx_clock_domain_close(&domain, &config.barrier), NX_SUCCESS);
        static const nx_rtc_port_t rtc{};
        config.rtc = &rtc;
        config.poll_limit = 16U;
        config.max_elapsed_us = 200000U;
        config.drift_ppm = 100U;
    }
    void Arm() {
        ASSERT_EQ(nx_stm32_freertos_stop_arm(&owner, &config), NX_SUCCESS);
        rtc_sample = 0U; /* Arm admission is read-only. */
    }
};
TEST_F(FreeRtosStop, UnselectedAndShortIdleNeverBorrowATick) {
    selected = nullptr;
    EXPECT_FALSE(nx_freertos_try_stop_idle(1000U));
    selected = &owner;
    Arm();
    EXPECT_FALSE(nx_freertos_try_stop_idle(100U));
    EXPECT_TRUE(owner.armed);
    EXPECT_EQ(pauses, 0U);
    EXPECT_EQ(powers, 0U);
}
TEST_F(FreeRtosStop, RealIdleHandoffBoundsElapsedAndNeverCrossesNextUnblock) {
    Arm();
    nx_freertos_suppress_ticks_and_sleep(1000U);
    EXPECT_FALSE(owner.armed);
    EXPECT_TRUE(owner.attempted);
    EXPECT_EQ(owner.result, NX_SUCCESS);
    EXPECT_EQ(pauses, 1U);
    EXPECT_EQ(handoffs, 1U);
    EXPECT_EQ(powers, 1U);
    EXPECT_EQ(acks, 1U);
    EXPECT_EQ(restores, 1U);
    EXPECT_EQ(mask, 0U);
    EXPECT_GT(owner.elapsed_min_us, 121000U);
    EXPECT_LT(owner.elapsed_max_us, 130000U);
    EXPECT_EQ(stepped, owner.accounted_ticks);
    EXPECT_LT(stepped, 1000U);
    EXPECT_LT(owner.discarded_phase_us, 1000U);
    EXPECT_GE(owner.uncertainty_us,
              owner.elapsed_max_us - owner.elapsed_min_us);
    EXPECT_EQ(resume_at, now + 1000U);
    EXPECT_FALSE(nx_freertos_try_stop_idle(1000U));
    EXPECT_EQ(restores, 1U);
}
TEST_F(FreeRtosStop, PendingKernelOrTickCannotBeAcknowledgedByDeepIdle) {
    Arm();
    sleep_status = eAbortSleep;
    EXPECT_TRUE(nx_freertos_try_stop_idle(1000U));
    EXPECT_EQ(pauses, 0U);
    EXPECT_EQ(powers, 0U);
    EXPECT_EQ(mask, 0U);
    Arm();
    sleep_status = eStandardSleep;
    pause_result = NX_ERROR_BUSY;
    pending = true;
    EXPECT_TRUE(nx_freertos_try_stop_idle(1000U));
    EXPECT_EQ(handoffs, 0U);
    EXPECT_EQ(powers, 0U);
    EXPECT_TRUE(pending);
    EXPECT_EQ(restores, 0U);
}
TEST_F(FreeRtosStop, FailedHandoffRestoresOneTickAndCannotFallThrough) {
    Arm();
    handoff_result = NX_ERROR_IO;
    EXPECT_TRUE(nx_freertos_try_stop_idle(1000U));
    EXPECT_EQ(powers, 0U);
    EXPECT_EQ(restores, 1U);
    EXPECT_EQ(owner.result, NX_ERROR_IO);
    EXPECT_EQ(mask, 0U);
}
TEST_F(FreeRtosStop, RejectedPowerDoesNotInventANewEpochOrDeepElapsed) {
    Arm();
    power_result = NX_ERROR_BUSY;
    EXPECT_TRUE(nx_freertos_try_stop_idle(1000U));
    EXPECT_EQ(restores, 1U);
    EXPECT_EQ(owner.result, NX_ERROR_BUSY);
    EXPECT_EQ(owner.power.new_epoch, 0U);
    EXPECT_EQ(owner.elapsed_min_us, 0U);
    EXPECT_EQ(owner.elapsed_max_us, 0U);
    EXPECT_EQ(acks, 0U);
    EXPECT_EQ(stepped, 0U);
}
TEST_F(FreeRtosStop, PowerFailureCannotReuseAnObservedDifferentCoreEpoch) {
    Arm();
    power_result = NX_ERROR_IO;
    changed_epoch_failure = true;
    EXPECT_THROW(nx_freertos_try_stop_idle(1000U), std::runtime_error);
    EXPECT_EQ(owner.result, NX_ERROR_IO);
    EXPECT_EQ(mask, 1U);
    EXPECT_EQ(restores, 0U);
    EXPECT_EQ(steps, 0U);
}
TEST_F(FreeRtosStop, ObservedEnvelopeBeyondDeclaredBoundFailsWithMasksHeld) {
    Arm();
    rtc_delta = 64U;
    EXPECT_THROW(nx_freertos_try_stop_idle(1000U), std::runtime_error);
    EXPECT_EQ(mask, 1U);
    EXPECT_EQ(restores, 0U);
    EXPECT_EQ(owner.result, NX_ERROR_TIMEOUT);
}
TEST_F(FreeRtosStop, MidnightWrapUsesOneBoundedDayNotANegativeOrDayLongSleep) {
    rtc_before = 22118400U - 16U;
    Arm();
    EXPECT_TRUE(nx_freertos_try_stop_idle(1000U));
    EXPECT_EQ(owner.result, NX_SUCCESS);
    EXPECT_GT(owner.elapsed_min_us, 121000U);
    EXPECT_LT(owner.elapsed_max_us, 130000U);
}
TEST_F(FreeRtosStop, LostRtcAfterStopCannotInventElapsedOrRestoreATick) {
    Arm();
    after_sample_result = NX_ERROR_STATE;
    EXPECT_THROW(nx_freertos_try_stop_idle(1000U), std::runtime_error);
    EXPECT_EQ(mask, 1U);
    EXPECT_EQ(restores, 0U);
    EXPECT_EQ(owner.result, NX_ERROR_STATE);
    EXPECT_EQ(acks, 0U);
}
TEST_F(FreeRtosStop, StaleDomainBarrierRejectsBeforeBorrowingTick) {
    Arm();
    ASSERT_EQ(nx_clock_domain_reopen(config.barrier), NX_SUCCESS);
    EXPECT_TRUE(nx_freertos_try_stop_idle(1000U));
    EXPECT_FALSE(owner.armed);
    EXPECT_EQ(owner.result, NX_ERROR_STATE);
    EXPECT_EQ(pauses, 0U);
    EXPECT_EQ(powers, 0U);
}
TEST_F(FreeRtosStop,
       StepLatencyAndAckFailureRemainObservableWithoutSecondStep) {
    Arm();
    kernel_delay = 73U;
    ack_result = NX_ERROR_IO;
    EXPECT_TRUE(nx_freertos_try_stop_idle(1000U));
    EXPECT_EQ(owner.result, NX_ERROR_IO);
    EXPECT_EQ(steps, 1U);
    EXPECT_EQ(restores, 1U);
    EXPECT_EQ(mask, 0U);
    EXPECT_GT(owner.elapsed_max_us, 128980U);
    EXPECT_GE(owner.uncertainty_us, 73U);
    EXPECT_EQ(resume_at, now + 1000U);
}
} /* namespace */
extern "C" nx_stm32_freertos_stop_t* nx_stm32_freertos_stop_context(void) {
    return selected;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return 0U;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {mask, 0U, 0U};
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved{mask};
    mask = 1U;
    return saved;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    mask = saved.value;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" eSleepModeStatus eTaskConfirmSleepModeStatus(void) {
    ++confirms;
    return sleep_status;
}
extern "C" void vTaskStepTick(TickType_t ticks) {
    ++steps;
    stepped += static_cast<uint32_t>(ticks);
    now += kernel_delay;
    EXPECT_LT(ticks, 1000U);
}
extern "C" BaseType_t xTaskIncrementTick(void) {
    ADD_FAILURE() << "deep profile crossed a timeout";
    return pdFALSE;
}
extern "C" void nx_freertos_assert_failed(const char*, unsigned) {
    throw std::runtime_error("time envelope lost");
}
extern "C" nx_result_t nx_timebase_stop_handoff(void*) {
    ++handoffs;
    return handoff_result;
}
extern "C" void nx_timebase_stop_resume(void*, uint64_t next) {
    ++restores;
    resume_at = next;
}
extern "C" nx_result_t nx_stm32_rtc_idle_sample(const nx_rtc_port_t*,
                                                uint32_t* out) {
    if (sample_result != NX_SUCCESS)
        return sample_result;
    if (powers != 0U && after_sample_result != NX_SUCCESS)
        return after_sample_result;
    *out =
        rtc_sample++ == 0U ? rtc_before : (rtc_before + rtc_delta) % 22118400U;
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_rtc_idle_ack(const nx_rtc_port_t*) {
    ++acks;
    return ack_result;
}
extern "C" nx_result_t
nx_stm32_power_enter_idle_masked(nx_clock_barrier_t,
                                 const nx_stm32_power_options_t*,
                                 nx_stm32_stop_receipt_t* out) {
    ++powers;
    if (power_result != NX_SUCCESS) {
        if (changed_epoch_failure) {
            *out = {1U, 2U, now, 20000U, true};
            now = 20000U;
        }
        return power_result;
    }
    *out = {1U, 2U, now, 0U, true};
    now = 0U;
    return NX_SUCCESS;
}
namespace {
uint64_t Now(void*) {
    return now;
}
nx_result_t Pause(void*, nx_freertos_tick_snapshot_t* out) {
    ++pauses;
    if (pause_result == NX_SUCCESS)
        *out = {now, phase};
    return pause_result;
}
void Resume(void*, uint64_t next) {
    ++restores;
    resume_at = next;
}
nx_result_t ArmShallow(void*, uint64_t) {
    ADD_FAILURE() << "deep Idle fell through into a second alarm";
    return NX_ERROR_BUSY;
}
void Disarm(void*) {
    ADD_FAILURE() << "deep Idle disarmed a foreign alarm";
}
int tick_owner;
const nx_freertos_lowpower_port_t port{&tick_owner, Now,    Pause,
                                       ArmShallow,  Disarm, Resume};
} /* namespace */
extern "C" const nx_freertos_lowpower_port_t* nx_freertos_lowpower_port(void) {
    return &port;
}

extern "C" nx_arch_result_t nx_arch_wait_for_interrupt(void) {
    ADD_FAILURE() << "deep Idle entered a second sleep";
    return NX_ARCH_UNSUPPORTED;
}
