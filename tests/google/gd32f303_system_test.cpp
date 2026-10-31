/**
 * \file            gd32f303_system_test.cpp
 * \brief           Official register layout and reserved timer contract tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_system.h"
#include "model.h"
#include "nexus/core/fault.h"
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
static nx_gd32f303_system_t* s_fault_system;
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    const bool retained = s_fault_system != nullptr &&
                          !s_fault_system->started &&
                          s_fault_system->timer_error == NX_ERROR_IO &&
                          s_fault_system->remaining_effects != 0U;
    const bool isolated = TIMER_CTL0(TIMER1) == 0U &&
                          TIMER_DMAINTEN(TIMER1) == 0U &&
                          !g_gd303_irq[TIMER1_IRQn];
    std::_Exit(domain == NX_FAULT_PLATFORM && reason == NX_ERROR_IO &&
                       file != nullptr && line != 0U && retained && isolated &&
                       nx_arch_irq_is_masked()
                   ? 77
                   : 91);
}
class GD303System : public testing::Test {
  protected:
    nx_gd32f303_system_t system{};
    void* mapping = MAP_FAILED;
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        std::memset(g_gd303_irq, 0, sizeof(g_gd303_irq));
        std::memset(g_gd303_pending, 0, sizeof(g_gd303_pending));
        g_gd303_ignore = nullptr;
        g_gd303_writes = 0;
        g_gd303_update_trgo = false;
        g_gd303_carry_on_low_read = false;
        g_gd303_density = (48U << 16U) | 256U;
        g_gd303_hold_update = g_gd303_hold_irq_off = false;
        g_gd303_pend_on_timer_withdraw = false;
        g_gd303_hold_late_irq_off = false;
        RCU_CTL =
            RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB | (16U << 3U) | (0xa5U << 8U);
    }
    void TearDown() override {
        if (mapping != MAP_FAILED)
            munmap(mapping, 0x30000U);
    }
};
class GD303AdjustedClock : public GD303System,
                           public testing::WithParamInterface<uint32_t> {};
TEST_P(GD303AdjustedClock, RejectsBeforeAnyClockTimerOrIrqOwnership) {
    RCU_CTL = (RCU_CTL & ~static_cast<uint32_t>(RCU_CTL_IRC8MADJ)) |
              (GetParam() << 3U);
    const uint32_t clock = RCU_CTL;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_system_preflight(&system));
    const auto start = nx_gd32f303_system_start(&system, 4U);
    EXPECT_EQ(NX_ERROR_BUSY, start.primary);
    EXPECT_EQ(NX_SUCCESS, start.cleanup);
    EXPECT_EQ(0U, start.remaining_effects);
    EXPECT_EQ(0U, g_gd303_writes);
    EXPECT_EQ(clock, RCU_CTL);
    EXPECT_EQ(0U, RCU_APB1EN);
    EXPECT_EQ(0U, system.remaining_effects);
    EXPECT_EQ(0U, system.core_hz);
    EXPECT_FALSE(system.started);
    EXPECT_FALSE(g_gd303_irq[TIMER1_IRQn]);
    EXPECT_FALSE(g_gd303_irq[TIMER4_IRQn]);
}
INSTANTIATE_TEST_SUITE_P(NonResetAdjustment, GD303AdjustedClock,
                         testing::Values(0U, 15U, 17U, 31U));
TEST_F(GD303System, ResetAdjustmentPreservesEveryFactoryCalibrationValue) {
    for (uint32_t calibration : {0U, 0x5aU, 0xffU}) {
        RCU_CTL = RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB | (16U << 3U) |
                  (calibration << 8U);
        const uint32_t clock = RCU_CTL;
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_preflight(&system));
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4U).primary);
        EXPECT_EQ(clock, RCU_CTL);
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_stop(&system));
        EXPECT_EQ(clock, RCU_CTL);
    }
}
TEST_F(GD303System, RestartCannotAdoptAChangedInternalClockAdjustment) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4U).primary);
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_stop(&system));
    RCU_CTL &= ~static_cast<uint32_t>(RCU_CTL_IRC8MADJ);
    const uint32_t clock = RCU_CTL;
    g_gd303_writes = 0U;
    const auto start = nx_gd32f303_system_start(&system, 4U);
    EXPECT_EQ(NX_ERROR_BUSY, start.primary);
    EXPECT_EQ(0U, g_gd303_writes);
    EXPECT_EQ(clock, RCU_CTL);
    EXPECT_FALSE(system.started);
    EXPECT_EQ(0U, system.remaining_effects);
}
TEST_F(GD303System, RejectForeignClockWithoutWrites) {
    RCU_CTL |= RCU_CTL_PLLEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_system_start(&system, 4).primary);
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303System, StartsExactSixteenBitOneMegahertzTimer) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    EXPECT_TRUE(system.started);
    EXPECT_EQ(7U, TIMER_PSC(TIMER1));
    EXPECT_EQ(65535U, TIMER_CAR(TIMER1));
    EXPECT_EQ(TIMER_CTL0_CEN, TIMER_CTL0(TIMER1));
    EXPECT_TRUE(g_gd303_irq[TIMER1_IRQn]);
}
TEST_F(GD303System, CascadeReservesTimer4AndCountsLowOverflowViaIti0) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4U).primary);
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER4EN);
    EXPECT_EQ(7U, TIMER_SMCFG(TIMER4));
    EXPECT_EQ(0U, TIMER_PSC(TIMER4));
    EXPECT_EQ(65535U, TIMER_CAR(TIMER4));
    EXPECT_EQ(2U << 4U, TIMER_CTL1(TIMER1));
    EXPECT_TRUE(g_gd303_irq[TIMER4_IRQn]);
}
TEST_F(GD303System, SixteenLowWrapsSurviveDelayedIrqAndLongFlashPulse) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4U).primary);
    TIMER_CNT(TIMER4) = 16U;
    TIMER_CNT(TIMER1) = 3456U;
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    EXPECT_EQ(16U * 65536U + 3456U, nx_gd32f303_system_now(&system));
    nx_gd32f303_system_timer_irq(&system);
    EXPECT_EQ(16U * 65536U + 3456U, nx_gd32f303_system_now(&system));
}
TEST_F(GD303System, HighWordPendingWrapIsCorrectedBeforeAndAfterItsIrq) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4U).primary);
    TIMER_CNT(TIMER4) = 65535U;
    TIMER_CNT(TIMER1) = 65535U;
    EXPECT_EQ(UINT64_C(4294967295), nx_gd32f303_system_now(&system));
    TIMER_CNT(TIMER4) = 0U;
    TIMER_CNT(TIMER1) = 7U;
    TIMER_INTF(TIMER4) = TIMER_INTF_UPIF;
    EXPECT_EQ(UINT64_C(4294967303), nx_gd32f303_system_now(&system));
    nx_gd32f303_system_cascade_irq(&system);
    EXPECT_EQ(UINT64_C(4294967303), nx_gd32f303_system_now(&system));
    nx_gd32f303_system_cascade_irq(&system);
    EXPECT_EQ(UINT64_C(4294967303), nx_gd32f303_system_now(&system));
}
TEST_F(GD303System, ForeignHighGateAndSlaveRejectWithoutResettingIt) {
    RCU_APB1EN = RCU_APB1EN_TIMER4EN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_system_start(&system, 4U).primary);
    EXPECT_EQ(0U, g_gd303_writes);
    RCU_APB1EN = 0U;
    TIMER_SMCFG(TIMER4) = 1U;
    auto start = nx_gd32f303_system_start(&system, 4U);
    EXPECT_EQ(NX_ERROR_BUSY, start.primary);
    EXPECT_EQ(NX_SUCCESS, start.cleanup);
    EXPECT_EQ(1U, TIMER_SMCFG(TIMER4));
}
TEST_F(GD303System, CarryBetweenWordReadsRereadsLowInNewHighEpoch) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4U).primary);
    TIMER_CNT(TIMER4) = 2U;
    TIMER_CNT(TIMER1) = 65535U;
    g_gd303_carry_on_low_read = true;
    EXPECT_EQ(3U * 65536U, nx_gd32f303_system_now(&system));
    EXPECT_FALSE(g_gd303_carry_on_low_read);
}
TEST_F(GD303System, HighSourceWithdrawalFailureRetainsBothClockLeases) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4U).primary);
    g_gd303_ignore = &TIMER_CTL0(TIMER4);
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_system_stop(&system));
    EXPECT_NE(0U, system.remaining_effects & 4U);
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER4EN);
    EXPECT_EQ(0U, TIMER_CTL1(TIMER1));
    EXPECT_FALSE(g_gd303_irq[TIMER4_IRQn]);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_system_stop(&system));
}

TEST_F(GD303System, RejectWrongPhysicalDensityWithoutWrites) {
    g_gd303_density = (64U << 16U) | 512U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_gd32f303_system_start(&system, 4).primary);
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303System, RejectPendingReservedVectorWithoutWrites) {
    g_gd303_pending[TIMER1_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_system_start(&system, 4).primary);
    EXPECT_EQ(0U, g_gd303_writes);
}
TEST_F(GD303System, UpdateFailureRollsBackEveryOwnedSource) {
    g_gd303_hold_update = true;
    auto result = nx_gd32f303_system_start(&system, 4);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(system.started);
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
    EXPECT_EQ(0U, TIMER_DMAINTEN(TIMER1));
}
TEST_F(GD303System, PoweredOffForeignTimerRemainsUntouched) {
    TIMER_CTL0(TIMER1) = TIMER_CTL0_CEN;
    auto result = nx_gd32f303_system_start(&system, 4);
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(TIMER_CTL0_CEN, TIMER_CTL0(TIMER1));
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
}
TEST_F(GD303System, TimerWrapIsCountedExactlyOnceBeforeAndAfterIrq) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    TIMER_CNT(TIMER1) = 0xfffe;
    EXPECT_EQ(65534U, nx_gd32f303_system_now(&system));
    TIMER_CNT(TIMER1) = 2;
    TIMER_CNT(TIMER4) = 1;
    TIMER_INTF(TIMER1) |= TIMER_INTF_UPIF;
    EXPECT_EQ(65538U, nx_gd32f303_system_now(&system));
    nx_gd32f303_system_timer_irq(&system);
    EXPECT_EQ(65538U, nx_gd32f303_system_now(&system));
    nx_gd32f303_system_timer_irq(&system);
    EXPECT_EQ(65538U, nx_gd32f303_system_now(&system));
}
TEST_F(GD303System, FailedGateWithdrawalRetainsCleanupOwnerForRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    g_gd303_ignore = &RCU_APB1EN;
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_system_stop(&system));
    EXPECT_NE(0U, system.remaining_effects);
    EXPECT_FALSE(system.started);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_system_stop(&system));
    EXPECT_EQ(0U, system.remaining_effects);
}
TEST_F(GD303System, FailedIrqWithdrawalStillWithdrawsTimerSources) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    g_gd303_hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_system_stop(&system));
    EXPECT_EQ(0U, TIMER_DMAINTEN(TIMER1));
    EXPECT_EQ(0U, TIMER_CTL0(TIMER1));
    EXPECT_NE(0U, system.remaining_effects);
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
}

TEST_F(GD303System, RetainedSlaveClockRejectsWithoutResettingForeignState) {
    TIMER_SMCFG(TIMER1) = 7U;
    auto result = nx_gd32f303_system_start(&system, 4);
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(7U, TIMER_SMCFG(TIMER1));
    EXPECT_EQ(0U, TIMER_SWEVG(TIMER1));
    EXPECT_EQ(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
}
TEST_F(GD303System, RetainedMasterOrChannelStateRejectsWithoutUpdateEvent) {
    for (volatile uint32_t* reg :
         {&TIMER_CTL1(TIMER1), &TIMER_CHCTL0(TIMER1), &TIMER_CHCTL1(TIMER1),
          &TIMER_CHCTL2(TIMER1)}) {
        *reg = 1U;
        auto result = nx_gd32f303_system_start(&system, 4);
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(1U, *reg);
        EXPECT_EQ(0U, TIMER_SWEVG(TIMER1));
        *reg = 0U;
    }
}

TEST_F(GD303System, ForeignCrystalBypassOrMonitorRejectsWithoutWrites) {
    for (uint32_t flag : {RCU_CTL_HXTALBPS, RCU_CTL_CKMEN}) {
        RCU_CTL = RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB | (16U << 3U) | flag;
        EXPECT_EQ(NX_ERROR_BUSY, nx_gd32f303_system_start(&system, 4).primary);
        EXPECT_EQ(0U, g_gd303_writes);
        EXPECT_NE(0U, RCU_CTL & flag);
        if (system.started) {
            ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_stop(&system));
        }
        g_gd303_writes = 0;
    }
}

TEST_F(GD303System, FailedWrapAckIsolatesSourcesAndNeverResumesConsumers) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    s_fault_system = &system;
    TIMER_INTF(TIMER1) = TIMER_INTF_UPIF;
    g_gd303_ignore = &TIMER_INTF(TIMER1);
    ASSERT_EXIT(nx_gd32f303_system_timer_irq(&system),
                testing::ExitedWithCode(77), "");
    s_fault_system = nullptr;
}
TEST_F(GD303System, SnapshotPreservesAnIncomingInterruptMask) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    TIMER_CNT(TIMER1) = 37;
    auto guard = nx_arch_irq_save();
    EXPECT_EQ(37U, nx_gd32f303_system_now(&system));
    EXPECT_TRUE(nx_arch_irq_is_masked());
    nx_arch_irq_restore(guard);
}

TEST_F(GD303System, PrescalerUpdateDoesNotPublishAResetModeTrgoPulse) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    EXPECT_FALSE(g_gd303_update_trgo);
    EXPECT_EQ(2U << 4U, TIMER_CTL1(TIMER1));
}

TEST_F(GD303System, LatePendingVectorIsWithdrawnAfterTimerSources) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    g_gd303_pend_on_timer_withdraw = true;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_system_stop(&system));
    EXPECT_FALSE(g_gd303_pending[TIMER1_IRQn]);
    EXPECT_FALSE(g_gd303_irq[TIMER1_IRQn]);
    EXPECT_EQ(0U, system.remaining_effects);
}
TEST_F(GD303System, RefusedLateVectorWithdrawalRetainsOwnerAndClockForRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_gd32f303_system_start(&system, 4).primary);
    g_gd303_pend_on_timer_withdraw = true;
    g_gd303_hold_late_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_gd32f303_system_stop(&system));
    EXPECT_FALSE(system.started);
    EXPECT_TRUE(g_gd303_pending[TIMER1_IRQn]);
    EXPECT_NE(0U, system.remaining_effects & 2U);
    EXPECT_NE(0U, RCU_APB1EN & RCU_APB1EN_TIMER1EN);
    g_gd303_hold_late_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_gd32f303_system_stop(&system));
    EXPECT_FALSE(g_gd303_pending[TIMER1_IRQn]);
    EXPECT_EQ(0U, system.remaining_effects);
}
