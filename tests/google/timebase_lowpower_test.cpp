/**
 * \file            timebase_lowpower_test.cpp
 * \brief           Actual phase/alarm implementation against hardware semantics
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "timebase_lowpower.h"
#include "timebase_lowpower_model.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
constexpr uint32_t pending_tick = 1U << 26U;
constexpr uint32_t count_flag = 1U << 16U;

/** \brief External timer/CPU register facts are the only modeled boundary. */
class TimebaseLowpower : public testing::Test {
  public:
    uint32_t control{7U}, reload{167999U}, value{126001U}, pending{};
    uint32_t status{1U}, interrupts{1U}, compare{};
    uint64_t now{1000U};
    unsigned writes{}, barriers{}, ticks{};
    unsigned access_cycles{};
    uint32_t timer_fraction{};
    uint64_t elapsed_cycles{};
    bool late_arm{}, pending_on_stop{}, never_reload{}, fail_disarm{};
    bool fail_rollback{}, fail_resume_interrupt{}, late_first{};
    testing::NiceMock<testing::MockFunction<uint64_t()>> clock;
    nx_timebase_registers_t registers{
        &control, &reload, &value, &pending, &status,    &interrupts, &compare,
        2U,       2U,      168U,   167999U,  read_clock, this};
    nx_timebase_lowpower_t context{&registers, 0U, 0U, false, false, false};
    static TimebaseLowpower* active;

    static uint64_t read_clock(void* p) {
        return static_cast<TimebaseLowpower*>(p)->clock.Call();
    }
    void SetUp() override {
        active = this;
        ON_CALL(clock, Call()).WillByDefault([this] { return now; });
    }
    void TearDown() override {
        active = nullptr;
    }
    void pause() {
        nx_freertos_tick_snapshot_t snapshot{};
        ASSERT_EQ(nx_timebase_pause_tick(&context, &snapshot), NX_SUCCESS);
    }
    void cycle() {
        ++elapsed_cycles;
        if (++timer_fraction == registers.cycles_per_us) {
            timer_fraction = 0U;
            ++now;
        }
        if ((control & 1U) == 0U)
            return;
        if (value == 0U) {
            if (!never_reload)
                value = reload;
        } else if (--value == 0U) {
            control |= count_flag;
            pending |= pending_tick;
        }
    }
};
TimebaseLowpower* TimebaseLowpower::active;

TEST_F(TimebaseLowpower, PendingTickRejectsWithoutChangingSource) {
    pending = pending_tick;
    nx_freertos_tick_snapshot_t out{77U, 78U};
    EXPECT_EQ(nx_timebase_pause_tick(&context, &out), NX_ERROR_BUSY);
    EXPECT_EQ(writes, 0U);
    EXPECT_EQ(control, 7U);
    EXPECT_EQ(out.time_us, 77U);
}
TEST_F(TimebaseLowpower, SnapshotCannotAliasItsRetainedControl) {
    auto* out = reinterpret_cast<nx_freertos_tick_snapshot_t*>(&context);
    EXPECT_EQ(nx_timebase_pause_tick(&context, out), NX_ERROR_INVALID);
    EXPECT_EQ(context.registers, &registers);
    EXPECT_FALSE(context.paused);
    EXPECT_EQ(writes, 0U);
}
TEST_F(TimebaseLowpower, SnapshotCannotOverwriteImmutableRegisterBinding) {
    auto* out = reinterpret_cast<nx_freertos_tick_snapshot_t*>(&registers);
    EXPECT_EQ(nx_timebase_pause_tick(&context, out), NX_ERROR_INVALID);
    EXPECT_EQ(registers.tick_control, &control);
    EXPECT_EQ(writes, 0U);
}
TEST_F(TimebaseLowpower, SnapshotCannotOverwriteAReviewedMMIOWord) {
    auto* out = reinterpret_cast<nx_freertos_tick_snapshot_t*>(&control);
    EXPECT_EQ(nx_timebase_pause_tick(&context, out), NX_ERROR_INVALID);
    EXPECT_EQ(control, 7U);
    EXPECT_EQ(writes, 0U);
}
TEST_F(TimebaseLowpower, CapturePreservesFractionAndHasADeclaredSampleBound) {
    nx_freertos_tick_snapshot_t out{};
    EXPECT_EQ(nx_timebase_pause_tick(&context, &out), NX_SUCCESS);
    EXPECT_EQ(out.time_us, now);
    EXPECT_EQ(out.phase_us, 249U);
    EXPECT_EQ(context.phase_cycles, 166U);
    EXPECT_EQ(control, 6U);
    EXPECT_TRUE(context.paused);
}
TEST_F(TimebaseLowpower, PendingArrivalDuringDisableRollsBackControl) {
    pending_on_stop = true;
    nx_freertos_tick_snapshot_t out{77U, 78U};
    EXPECT_EQ(nx_timebase_pause_tick(&context, &out), NX_ERROR_BUSY);
    EXPECT_EQ(control, 7U);
    EXPECT_EQ(pending, pending_tick);
    EXPECT_FALSE(context.paused);
    EXPECT_EQ(out.time_us, 77U);
}
TEST_F(TimebaseLowpower, ExcessiveOrReversedSampleWindowRejects) {
    EXPECT_CALL(clock, Call())
        .WillOnce(testing::Return(1000U))
        .WillOnce(testing::Return(1005U));
    nx_freertos_tick_snapshot_t out{77U, 78U};
    EXPECT_EQ(nx_timebase_pause_tick(&context, &out), NX_ERROR_BUSY);
    EXPECT_EQ(control, 7U);
    EXPECT_FALSE(context.paused);
    EXPECT_EQ(out.time_us, 77U);
}
TEST_F(TimebaseLowpower, ReversedSampleIsRejectedWithPeriodicSourceRestored) {
    EXPECT_CALL(clock, Call())
        .WillOnce(testing::Return(1000U))
        .WillOnce(testing::Return(999U));
    nx_freertos_tick_snapshot_t out{77U, 78U};
    EXPECT_EQ(nx_timebase_pause_tick(&context, &out), NX_ERROR_IO);
    EXPECT_EQ(control, 7U);
    EXPECT_FALSE(context.paused);
    EXPECT_EQ(out.time_us, 77U);
}
TEST_F(TimebaseLowpower, FailedRejectedPauseRollbackCannotForgetStoppedSource) {
    pending_on_stop = true;
    fail_rollback = true;
    nx_freertos_tick_snapshot_t out{77U, 78U};
    EXPECT_THROW(nx_timebase_pause_tick(&context, &out), std::runtime_error);
    EXPECT_TRUE(context.paused);
    EXPECT_EQ(control, 6U);
    EXPECT_EQ(out.time_us, 77U);
}
TEST_F(TimebaseLowpower, AlarmUsesAbsoluteLowWordAcrossCounterWrap) {
    pause();
    now = UINT64_C(0xffffffff) - 5U;
    EXPECT_EQ(nx_timebase_arm_deadline(&context, now + 10U), NX_SUCCESS);
    EXPECT_EQ(compare, 4U);
    EXPECT_EQ(interrupts, 3U);
    EXPECT_EQ(status & 1U, 1U);
    EXPECT_TRUE(context.armed);
}
TEST_F(TimebaseLowpower, ExpiredAndOverHorizonAlarmsHaveNoEffects) {
    pause();
    const unsigned prior = writes;
    EXPECT_EQ(nx_timebase_arm_deadline(&context, now), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_timebase_arm_deadline(&context, now + (UINT64_C(1) << 32U)),
              NX_ERROR_INVALID);
    EXPECT_EQ(writes, prior);
    EXPECT_FALSE(context.armed);
}
TEST_F(TimebaseLowpower, LateArmRejectsAndPreservesOverflow) {
    pause();
    late_arm = true;
    EXPECT_EQ(nx_timebase_arm_deadline(&context, now + 10U), NX_ERROR_TIMEOUT);
    EXPECT_FALSE(context.armed);
    EXPECT_EQ(interrupts, 1U);
    EXPECT_EQ(status & 1U, 1U);
}
TEST_F(TimebaseLowpower, DisarmNeverStealsSharedOverflowFlag) {
    pause();
    ASSERT_EQ(nx_timebase_arm_deadline(&context, now + 10U), NX_SUCCESS);
    status = 3U;
    nx_timebase_disarm(&context);
    EXPECT_EQ(status, 1U);
    EXPECT_EQ(interrupts, 1U);
    EXPECT_FALSE(context.armed);
}
TEST_F(TimebaseLowpower, FailedInterruptDisableCannotResumeAndForgetOwnership) {
    pause();
    ASSERT_EQ(nx_timebase_arm_deadline(&context, now + 10U), NX_SUCCESS);
    fail_disarm = true;
    EXPECT_THROW(nx_timebase_disarm(&context), std::runtime_error);
    EXPECT_TRUE(context.armed);
    EXPECT_TRUE(context.paused);
}
TEST_F(TimebaseLowpower, FirstCounterConsumesShortLoadBeforeFutureFullReload) {
    pause();
    nx_timebase_resume_tick(&context, now + 751U);
    EXPECT_EQ(value, 751U * 168U - 166U - 1U);
    EXPECT_EQ(reload, registers.full_reload);
    EXPECT_EQ(control, 7U);
    EXPECT_FALSE(context.paused);
    while (value != 0U)
        cycle();
    cycle();
    EXPECT_EQ(value, registers.full_reload);
    nx_timebase_tick_irq(&context);
    EXPECT_EQ(ticks, 1U);
    EXPECT_EQ(reload, registers.full_reload);
}
TEST_F(TimebaseLowpower, ASecondPauseCannotTreatTheShortCounterAsAFullPeriod) {
    pause();
    nx_timebase_resume_tick(&context, now + 100U);
    ASSERT_TRUE(context.first_short);
    const unsigned prior = writes;
    nx_freertos_tick_snapshot_t out{77U, 78U};
    EXPECT_EQ(nx_timebase_pause_tick(&context, &out), NX_ERROR_BUSY);
    EXPECT_EQ(writes, prior);
    EXPECT_EQ(out.time_us, 77U);
    EXPECT_EQ(control, 7U);
}
TEST_F(TimebaseLowpower,
       NearZeroRemainderQuantizationAddsAtMostOneMicrosecond) {
    pause();
    context.phase_cycles = 167U;
    nx_timebase_resume_tick(&context, now + 1U);
    EXPECT_EQ(value, 167U);
    EXPECT_EQ(reload, registers.full_reload);
    EXPECT_FALSE(context.paused);
}
TEST_F(TimebaseLowpower, UnprovenReloadIsAnExplicitRetainedFault) {
    pause();
    never_reload = true;
    EXPECT_THROW(nx_timebase_resume_tick(&context, now + 10U),
                 std::runtime_error);
    EXPECT_TRUE(context.paused);
    EXPECT_EQ(ticks, 0U);
}
TEST_F(TimebaseLowpower, FailedPeriodicIRQRestoreRemainsARetainedFault) {
    pause();
    fail_resume_interrupt = true;
    EXPECT_THROW(nx_timebase_resume_tick(&context, now + 10U),
                 std::runtime_error);
    EXPECT_TRUE(context.paused);
    EXPECT_EQ(ticks, 0U);
}
TEST_F(TimebaseLowpower, ShortUnderflowDuringFullLoadRestoreCannotBeAccepted) {
    pause();
    late_first = true;
    EXPECT_THROW(nx_timebase_resume_tick(&context, now + 1U),
                 std::runtime_error);
    EXPECT_TRUE(context.paused);
    EXPECT_EQ(ticks, 0U);
}
TEST_F(TimebaseLowpower, AlarmDispatchClearsOnlyTheReservedCompareSource) {
    pause();
    ASSERT_EQ(nx_timebase_arm_deadline(&context, now + 10U), NX_SUCCESS);
    status = 3U;
    nx_timebase_alarm_irq(&context);
    EXPECT_EQ(status, 1U);
    EXPECT_EQ(interrupts, 1U);
    EXPECT_FALSE(context.armed);
    EXPECT_EQ(ticks, 0U);
}
TEST_F(TimebaseLowpower, ContinuousTimerAccountsForRegisterLatencyAndPhase) {
    for (unsigned latency : {0U, 1U, 4U, 16U, 32U}) {
        for (uint32_t fraction : {0U, 41U, 167U}) {
            SCOPED_TRACE(testing::Message() << latency << ":" << fraction);
            control = 7U;
            reload = registers.full_reload;
            value = 126001U;
            pending = 0U;
            now = 1000U;
            timer_fraction = fraction;
            elapsed_cycles = 0U;
            access_cycles = latency;
            context = {&registers, 0U, 0U, false, false, false};
            nx_freertos_tick_snapshot_t snapshot{};
            ASSERT_EQ(nx_timebase_pause_tick(&context, &snapshot), NX_SUCCESS);
            nx_timebase_resume_tick(&context, snapshot.time_us + 1000U -
                                                  snapshot.phase_us);
            while ((pending & pending_tick) == 0U)
                cycle();
            const int64_t phase_error =
                static_cast<int64_t>(elapsed_cycles) - 126001;
            /* One timer unit plus two bounded sample windows; no exact phase.
             */
            EXPECT_GE(phase_error, -168);
            EXPECT_LT(phase_error, 11 * 168);
            cycle();
            EXPECT_EQ(value, registers.full_reload);
        }
    }
}
TEST_F(TimebaseLowpower, TickHandlerRestoresFullPeriodBeforeKernelDispatch) {
    context.first_short = true;
    reload = 50U;
    nx_timebase_tick_irq(&context);
    EXPECT_EQ(reload, registers.full_reload);
    EXPECT_EQ(ticks, 1U);
    EXPECT_FALSE(context.first_short);
}
} /* namespace */

extern "C" uint32_t nx_timebase_model_read(volatile uint32_t* reg) {
    auto* f = TimebaseLowpower::active;
    for (unsigned i = 0U; i < f->access_cycles; ++i)
        f->cycle();
    uint32_t observed = *reg;
    if (reg == &f->control)
        f->control &= ~count_flag;
    return observed;
}
extern "C" void nx_timebase_model_write(volatile uint32_t* reg, uint32_t v) {
    auto* f = TimebaseLowpower::active;
    for (unsigned i = 0U; i < f->access_cycles; ++i)
        f->cycle();
    ++f->writes;
    if (reg == &f->status) {
        f->status &= v;
        return;
    }
    if (reg == &f->value) {
        f->value = 0U;
        f->control &= ~count_flag;
        return;
    }
    if (reg == &f->interrupts && f->fail_disarm && (v & 2U) == 0U)
        return;
    if (reg == &f->control && (v & 1U) != 0U && f->fail_rollback)
        return;
    *reg = v;
    if (reg == &f->control && (v & 1U) != 0U && f->fail_resume_interrupt)
        f->control &= ~2U;
    if (reg == &f->reload && v == f->registers.full_reload && f->late_first) {
        while (f->value != 0U)
            f->cycle();
    }
    if (reg == &f->control && (v & 1U) == 0U && f->pending_on_stop)
        f->pending |= pending_tick;
    if (reg == &f->interrupts && (v & 2U) != 0U && f->late_arm)
        f->now += 10U;
}
extern "C" void nx_arch_dsb(void) {
    auto* f = TimebaseLowpower::active;
    ++f->barriers;
    f->cycle();
}
extern "C" void nx_freertos_kernel_tick_handler(void) {
    auto* f = TimebaseLowpower::active;
    EXPECT_EQ(f->reload, f->registers.full_reload);
    ++f->ticks;
}
extern "C" void nx_freertos_assert_failed(const char*, unsigned);
extern "C" void nx_freertos_assert_failed(const char*, unsigned) {
    throw std::runtime_error("retained timebase protocol fault");
}
