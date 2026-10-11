/**
 * \file            gd32_arch_test.cpp
 * \brief           GD32 timebase uses the shared CPU contract without drift
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f4xx.h"
#include "nexus/arch/arch.h"
#include "private/system.h"
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
/** \brief CPU state and SDK effects are external to the real SoC algorithm. */
class GD32Boundary {
  public:
    MOCK_METHOD(nx_arch_irq_state_t, Save, ());
    MOCK_METHOD(void, Restore, (uint32_t));
    MOCK_METHOD(bool, InISR, ());
    MOCK_METHOD(bool, Masked, ());
    MOCK_METHOD(void, CompleteWrites, ());
    MOCK_METHOD(void, SynchronizeInstructions, ());
    MOCK_METHOD(int, ClockStart, ());
    MOCK_METHOD(int, ClockStop, ());
};
GD32Boundary* s_boundary;
} /* namespace */

/** \brief The Arch boundary owns the saved interrupt state. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return s_boundary->Save();
}

/** \brief Restore the same opaque state without consulting the vendor model. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    s_boundary->Restore(previous.value);
}

/** \brief Exception state is supplied by the selected Arch implementation. */
extern "C" bool nx_arch_in_isr(void) {
    return s_boundary->InISR();
}

/** \brief Includes the Arch priority and fault masks, beyond vendor PRIMASK. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return s_boundary->Masked();
}

/** \brief CPU completion ordering remains distinct from peripheral readback. */
extern "C" void nx_arch_dsb(void) {
    s_boundary->CompleteWrites();
}

/** \brief Instruction synchronization follows completed peripheral writes. */
extern "C" void nx_arch_isb(void) {
    s_boundary->SynchronizeInstructions();
}

/** \brief Clock acquisition is a separate SoC boundary. */
extern "C" int nx_gd32_clock_start(void) {
    return s_boundary->ClockStart();
}

/** \brief Clock release is a separate SoC boundary. */
extern "C" int nx_gd32_clock_stop(void) {
    return s_boundary->ClockStop();
}

/** \brief Model only the selected clock-enable register effect. */
extern "C" void rcu_periph_clock_enable(rcu_periph_enum peripheral) {
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) |=
        1U << (static_cast<uint32_t>(peripheral) & 31U);
}

/** \brief Model only the selected clock-disable register effect. */
extern "C" void rcu_periph_clock_disable(rcu_periph_enum peripheral) {
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) &=
        ~(1U << (static_cast<uint32_t>(peripheral) & 31U));
}

/** \brief Preserve the SDK's selected sleep-clock enable effect. */
extern "C" void
rcu_periph_clock_sleep_enable(rcu_periph_sleep_enum peripheral) {
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) |=
        1U << (static_cast<uint32_t>(peripheral) & 31U);
}

/** \brief Preserve the SDK's selected sleep-clock disable effect. */
extern "C" void
rcu_periph_clock_sleep_disable(rcu_periph_sleep_enum peripheral) {
    REG32(RCU + (static_cast<uint32_t>(peripheral) >> 6U)) &=
        ~(1U << (static_cast<uint32_t>(peripheral) & 31U));
}

/** \brief Reset the modeled timer registers without changing CPU state. */
extern "C" void timer_deinit(uint32_t timer) {
    std::memset(reinterpret_cast<void*>(timer), 0, 0x50U);
}

class GD32Arch : public ::testing::Test {
  protected:
    ::testing::StrictMock<GD32Boundary> boundary;
    void* mapping = MAP_FAILED;

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        std::memset(&g_gd32_model_scb, 0, sizeof(g_gd32_model_scb));
        std::memset(&g_gd32_model_systick, 0, sizeof(g_gd32_model_systick));
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        s_boundary = &boundary;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000), 0);
        }
        s_boundary = nullptr;
    }
};

TEST_F(GD32Arch, NestedSectionsRestoreIncomingArchTokensInReverseOrder) {
    ::testing::InSequence sequence;
    EXPECT_CALL(boundary, Save())
        .WillOnce(::testing::Return(nx_arch_irq_state_t{0U}));
    EXPECT_CALL(boundary, Save())
        .WillOnce(::testing::Return(nx_arch_irq_state_t{1U}));
    EXPECT_CALL(boundary, Restore(1U));
    EXPECT_CALL(boundary, Restore(0U));
    uint32_t outer = nx_gd32_critical_enter();
    uint32_t inner = nx_gd32_critical_enter();
    EXPECT_EQ(outer, 0U);
    EXPECT_EQ(inner, 1U);
    nx_gd32_critical_leave(inner);
    nx_gd32_critical_leave(outer);
    EXPECT_EQ(g_gd32_model_mask, 0U);
}

TEST_F(GD32Arch, IncomingArchMaskIsPreservedWithoutVendorStateAccess) {
    EXPECT_CALL(boundary, Save())
        .WillOnce(::testing::Return(nx_arch_irq_state_t{1U}));
    EXPECT_CALL(boundary, Restore(1U));
    uint32_t saved = nx_gd32_critical_enter();
    EXPECT_EQ(saved, 1U);
    nx_gd32_critical_leave(saved);
    EXPECT_EQ(g_gd32_model_mask, 0U);
}

TEST_F(GD32Arch, ExceptionContextUsesArchInsteadOfVendorIPSR) {
    EXPECT_CALL(boundary, InISR()).WillOnce(::testing::Return(true));
    EXPECT_TRUE(nx_gd32_in_isr());
    EXPECT_FALSE(g_gd32_model_isr);
}

TEST_F(GD32Arch, PriorityOrFaultMaskIsObservedBeyondVendorPRIMASK) {
    EXPECT_CALL(boundary, Masked()).WillOnce(::testing::Return(true));
    EXPECT_TRUE(nx_gd32_irq_masked());
    EXPECT_EQ(g_gd32_model_mask, 0U);
}

TEST_F(GD32Arch, PeripheralCompletionUsesArchOrderingInSequence) {
    ::testing::InSequence sequence;
    EXPECT_CALL(boundary, CompleteWrites());
    EXPECT_CALL(boundary, SynchronizeInstructions());
    nx_gd32_peripheral_barrier();
}

TEST_F(GD32Arch, StartRejectsArchExceptionBeforeClockOrTimerEffects) {
    EXPECT_CALL(boundary, InISR()).WillOnce(::testing::Return(true));
    EXPECT_EQ(nx_gd32_soc_start(), -1);
    EXPECT_EQ(RCU_APB1EN, 0U);
    EXPECT_EQ(TIMER_CTL0(TIMER1), 0U);
}

TEST_F(GD32Arch, StartRejectsArchMaskBeforeClockOrTimerEffects) {
    ::testing::InSequence sequence;
    EXPECT_CALL(boundary, InISR()).WillOnce(::testing::Return(false));
    EXPECT_CALL(boundary, Masked()).WillOnce(::testing::Return(true));
    EXPECT_EQ(nx_gd32_soc_start(), -1);
    EXPECT_EQ(RCU_APB1EN, 0U);
    EXPECT_EQ(TIMER_CTL0(TIMER1), 0U);
}

TEST_F(GD32Arch, StopRejectsArchMaskBeforeReleasingSoCResources) {
    ::testing::InSequence sequence;
    EXPECT_CALL(boundary, InISR()).WillOnce(::testing::Return(false));
    EXPECT_CALL(boundary, Masked()).WillOnce(::testing::Return(true));
    EXPECT_EQ(nx_gd32_soc_stop(), -1);
    EXPECT_EQ(RCU_APB1EN, 0U);
}

TEST_F(GD32Arch, PendingWrapSamplingPreservesFlagUntilTheOwnedHandler) {
    EXPECT_CALL(boundary, Save())
        .Times(5)
        .WillRepeatedly(::testing::Return(nx_arch_irq_state_t{1U}));
    EXPECT_CALL(boundary, Restore(1U)).Times(5);
    TIMER_CNT(TIMER1) = 7U;
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    const uint64_t expected = (UINT64_C(1) << 32U) + 7U;
    EXPECT_EQ(nx_gd32_now_us(), expected);
    EXPECT_EQ(nx_time_now_us(), expected);
    EXPECT_NE(TIMER_INTF(TIMER1) & TIMER_INTF_UPIF, 0U);
    TIMER1_IRQHandler();
    EXPECT_EQ(TIMER_INTF(TIMER1) & TIMER_INTF_UPIF, 0U);
    TIMER1_IRQHandler();
    EXPECT_EQ(nx_gd32_now_us(), expected);
    EXPECT_EQ(g_gd32_model_mask, 0U);
}
