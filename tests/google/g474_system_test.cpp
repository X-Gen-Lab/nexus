/**
 * \file            g474_system_test.cpp
 * \brief           Actual G474 production clock,rollback and TIM2 contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_model.h"
#include "nexus/core/fault.h"
#include "stm32g474_system.h"
#include "terminal_watchdog.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
namespace {
class FaultPort {
  public:
    MOCK_METHOD(void, Observe,
                (nx_fault_domain_t, nx_result_t, const char*, unsigned));
};
struct ParkSignal {};
FaultPort* fault_port;
} /* namespace */
class G474System : public testing::Test {
  protected:
    testing::NiceMock<FaultPort> fault_mock;
    void SetUp() override {
        nx_g474_model_reset();
        fault_port = &fault_mock;
    }
    void TearDown() override {
        g_g474_model.before_irq_save = nullptr;
        /* Host-only cleanup after the observer interrupted a terminal path.
         * Production never restores a parked fault's retained interrupt mask.
         */
        if (nx_arch_irq_is_masked())
            nx_arch_irq_restore(nx_arch_irq_state_t{0U});
        fault_port = nullptr;
    }
};
class G474ForeignHsiTrim : public G474System,
                           public testing::WithParamInterface<uint32_t> {};
TEST_P(G474ForeignHsiTrim, RejectsBeforeClockWritesOrOwnership) {
    g_g474_model.rcc.ICSCR =
        (GetParam() << RCC_ICSCR_HSITRIM_Pos) | (0x5aU << RCC_ICSCR_HSICAL_Pos);
    const RCC_TypeDef before = g_g474_model.rcc;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32g474_system_preflight(&g_nx_stm32g474_system));
    const auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U);
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
    EXPECT_EQ(0U, g_nx_stm32g474_system.core_hz);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(0, std::memcmp(&before, &g_g474_model.rcc, sizeof(before)));
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8U));
    EXPECT_EQ(0U, g_g474_model.writes);
}
INSTANTIATE_TEST_SUITE_P(NonResetTrims, G474ForeignHsiTrim,
                         testing::Values(0U, 16U, 63U, 65U, 127U));
TEST_F(G474System, DefaultTrimAdmitsVariableFactoryCalibrationUnchanged) {
    EXPECT_EQ(64U, (g_g474_model.rcc.ICSCR & RCC_ICSCR_HSITRIM) >>
                       RCC_ICSCR_HSITRIM_Pos);
    for (uint32_t calibration : {0U, 1U, 0x7fU, 0xffU}) {
        SCOPED_TRACE(calibration);
        nx_g474_model_reset();
        const uint32_t internal = (64U << RCC_ICSCR_HSITRIM_Pos) |
                                  (calibration << RCC_ICSCR_HSICAL_Pos);
        g_g474_model.rcc.ICSCR = internal;
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32g474_system_preflight(&g_nx_stm32g474_system));
        EXPECT_EQ(0U, g_g474_model.writes);
        ASSERT_EQ(
            NX_SUCCESS,
            nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U).primary);
        EXPECT_EQ(170000000U, g_nx_stm32g474_system.core_hz);
        EXPECT_EQ(169U, g_g474_model.timer.PSC);
        EXPECT_EQ(internal, g_g474_model.rcc.ICSCR);
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8U));
        EXPECT_EQ(16000000U, g_nx_stm32g474_system.core_hz);
        EXPECT_EQ(internal, g_g474_model.rcc.ICSCR);
    }
}
TEST_F(G474System, Exact170MHzProfileUsesBoostFlashAndDelayedAhbSwitch) {
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_TRUE(g_g474_model.timer_update_observed);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_TRUE(g_g474_model.boost_delayed);
    EXPECT_FALSE(g_g474_model.unsafe_order);
    EXPECT_EQ(170000000U, g_nx_stm32g474_system.core_hz);
    EXPECT_EQ(5U, (g_g474_model.rcc.PLLCFGR & RCC_PLLCFGR_PLLM) >>
                      RCC_PLLCFGR_PLLM_Pos);
    EXPECT_EQ(85U, (g_g474_model.rcc.PLLCFGR & RCC_PLLCFGR_PLLN) >>
                       RCC_PLLCFGR_PLLN_Pos);
    EXPECT_EQ(0U, g_g474_model.rcc.CFGR &
                      (RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2));
    EXPECT_EQ(4U, g_g474_model.flash.ACR & FLASH_ACR_LATENCY);
    EXPECT_EQ(0U, g_g474_model.power.CR5 & PWR_CR5_R1MODE);
    EXPECT_EQ(169U, g_g474_model.timer.PSC);
    EXPECT_EQ(UINT32_MAX, g_g474_model.timer.ARR);
    EXPECT_TRUE(g_g474_model.irq_enabled[28]);
    EXPECT_TRUE(g_nx_stm32g474_system.started);
}
TEST_F(G474System, SoftwareUpdateIsClearedBeforePublishingAZeroEpoch) {
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U);
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_EQ(0U, g_g474_model.timer.SR & TIM_SR_UIF);
    EXPECT_EQ(0U, g_g474_model.timer.CNT);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    EXPECT_TRUE(g_nx_stm32g474_system.started);
}
TEST_F(G474System, RefusedInitialUpdateClearCannotPublishLiveTime) {
    g_g474_model.ignore_write = &g_g474_model.timer.SR;
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects & NX_STM32G474_EFFECT_TIMER);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    EXPECT_EQ(0U, g_g474_model.timer.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(0U, g_g474_model.timer.DIER);
    EXPECT_NE(0U, g_g474_model.rcc.APB1ENR1 & RCC_APB1ENR1_TIM2EN);
    g_g474_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_EQ(0U, g_g474_model.rcc.APB1ENR1 & RCC_APB1ENR1_TIM2EN);
}
TEST_F(G474System, RefusedSoftwareUpdateCannotReuseAnOldUpdateFlag) {
    g_g474_model.timer.SR = TIM_SR_UIF;
    g_g474_model.ignore_write = &g_g474_model.timer.EGR;
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3U);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
}
TEST_F(G474System, RefusedInitialCounterZeroCannotPublishLiveTime) {
    g_g474_model.hold_timer_count = true;
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    EXPECT_EQ(0U, g_g474_model.timer.CR1 & TIM_CR1_CEN);
}
TEST_F(G474System, CounterFailureRetainsObservedCleanupResponsibility) {
    g_g474_model.hold_timer_count = true;
    g_g474_model.hold_timer_gate_off = true;
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_NE(0U, g_g474_model.rcc.APB1ENR1 & RCC_APB1ENR1_TIM2EN);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    g_g474_model.hold_timer_gate_off = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8U));
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
}
TEST_F(G474System, HseFailureRollsBackAllOwnedFields) {
    auto old_pll = g_g474_model.rcc.PLLCFGR;
    auto old_flash = g_g474_model.flash.ACR;
    auto old_power = g_g474_model.power.CR1;
    auto old_boost = g_g474_model.power.CR5;
    g_g474_model.hold_hse = true;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.primary);
    EXPECT_EQ(NX_SUCCESS, r.cleanup);
    EXPECT_EQ(0U, r.remaining_effects);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_EQ(old_pll, g_g474_model.rcc.PLLCFGR);
    EXPECT_EQ(old_flash, g_g474_model.flash.ACR);
    EXPECT_EQ(old_power, g_g474_model.power.CR1);
    EXPECT_EQ(old_boost, g_g474_model.power.CR5);
    EXPECT_EQ(0U, g_g474_model.rcc.APB1ENR1 &
                      (RCC_APB1ENR1_TIM2EN | RCC_APB1ENR1_PWREN));
}
TEST_F(G474System, FailedRollbackRetainsActualClockOwnership) {
    g_g474_model.hold_hse = true;
    g_g474_model.hold_hsi = true;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.primary);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.cleanup);
    EXPECT_EQ(NX_STM32G474_EFFECT_CLOCK, r.remaining_effects);
    EXPECT_EQ(RCC_CR_HSEON, g_g474_model.rcc.CR & RCC_CR_HSEON);
}
TEST_F(G474System, LiveForeignClockIsRejectedWithoutWrites) {
    g_g474_model.rcc.CR |= RCC_CR_PLLON;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_BUSY, r.primary);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
}
TEST_F(G474System, FlashReadbackFailureDoesNotSwitchClock) {
    g_g474_model.ignore_write = &g_g474_model.flash.ACR;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_IO, r.primary);
    EXPECT_EQ(RCC_CFGR_SWS_HSI, g_g474_model.rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_FALSE(g_g474_model.boost_delayed);
}
TEST_F(G474System, PllAndSourceAcknowledgementFailuresAreBounded) {
    g_g474_model.hold_pll = true;
    auto r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.primary);
    EXPECT_EQ(NX_SUCCESS, r.cleanup);
    nx_g474_model_reset();
    g_g474_model.hold_switch = true;
    r = nx_stm32g474_system_start(&g_nx_stm32g474_system, 3);
    EXPECT_EQ(NX_ERROR_TIMEOUT, r.primary);
    EXPECT_EQ(NX_SUCCESS, r.cleanup);
}
TEST_F(G474System, PendingWrapSnapshotDoesNotDoubleCountIrq) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.timer.CNT = 7;
    g_g474_model.timer.SR = TIM_SR_UIF;
    EXPECT_EQ((UINT64_C(1) << 32U) + 7U,
              nx_stm32g474_system_now(&g_nx_stm32g474_system));
    nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system);
    EXPECT_EQ((UINT64_C(1) << 32U) + 7U,
              nx_stm32g474_system_now(&g_nx_stm32g474_system));
    EXPECT_EQ(0U, g_g474_model.timer.SR & TIM_SR_UIF);
}
TEST_F(G474System, StopRetainsTimebaseUntilIrqDisableIsObserved) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects &
                      NX_STM32G474_EFFECT_TIMER);
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
    EXPECT_EQ(16000000U, g_nx_stm32g474_system.core_hz);
}
TEST_F(G474System, DensityDeviceAndDbankAreExplicitPrerequisites) {
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_validate_identity());
    g_g474_model.flash_kib = 256;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_validate_identity());
    g_g474_model.flash_kib = 512;
    g_g474_model.device_id = 0x468;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_validate_identity());
    g_g474_model.device_id = 0x469;
    g_g474_model.flash.OPTR = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_validate_identity());
    EXPECT_EQ(0U, g_g474_model.writes);
}

TEST_F(G474System, PowerSnapshotsRequireObservedInterfaceClock) {
    const auto power = g_g474_model.power.CR1;
    const auto boost = g_g474_model.power.CR5;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    EXPECT_EQ(power, g_nx_stm32g474_system.saved_power);
    EXPECT_EQ(boost, g_nx_stm32g474_system.saved_boost);
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_EQ(power, g_g474_model.power.CR1);
    EXPECT_EQ(boost, g_g474_model.power.CR5);
}

TEST_F(G474System, BusyFlashRejectsBeforeClockOrLatencyWrites) {
    g_g474_model.flash.SR = FLASH_SR_BSY;
    auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, g_g474_model.writes);
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
}

TEST_F(G474System, FailedWithdrawalRetainsOwnerButDoesNotExposeLiveTime) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.timer.CNT = 13U;
    g_g474_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
}

TEST_F(G474System, RetainedOverflowIrqStillAcknowledgesItsOwnedLevelSource) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.hold_irq_off = true;
    ASSERT_EQ(NX_ERROR_IO, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    g_g474_model.timer.SR = TIM_SR_UIF;
    nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system);
    EXPECT_EQ(0U, g_g474_model.timer.SR & TIM_SR_UIF);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    g_g474_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
}

TEST_F(G474System, IsolatedAckFaultStillObservesTerminalOwnership) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U).primary);
    g_g474_model.timer.SR = TIM_SR_UIF;
    g_g474_model.timer.CNT = 7U;
    g_g474_model.irq_active[TIM2_IRQn] = true;
    g_g474_model.hold_timer_ack = true;
    EXPECT_CALL(fault_mock,
                Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                     unsigned line) {
            EXPECT_NE(nullptr, file);
            EXPECT_NE(0U, line);
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system),
                 ParkSignal);
    EXPECT_EQ(0U, g_nx_stm32g474_system.overflow_us);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32g474_system.timer_error);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects &
                      NX_STM32G474_EFFECT_TIMER);
    EXPECT_EQ(0U, g_g474_model.timer.DIER);
    EXPECT_EQ(0U, g_g474_model.timer.CR1 & TIM_CR1_CEN);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(0U, nx_stm32g474_system_now(&g_nx_stm32g474_system));
    EXPECT_TRUE(nx_arch_irq_is_masked());
}

TEST(TerminalWatchdog, MissingObserverEvidenceCannotPassAsPark) {
    EXPECT_FALSE(nexus_test::ObserveTerminal([] {
        for (;;) {
            pause();
        }
    }));
}
TEST(TerminalWatchdog, InvalidObserverEvidenceCannotPassAsPark) {
    EXPECT_FALSE(nexus_test::ObserveTerminal([] {
        nexus_test::RecordTerminalObservation(false);
        for (;;) {
            pause();
        }
    }));
}
TEST(TerminalWatchdog, EvidenceFollowedByReturnCannotPassAsPark) {
    EXPECT_FALSE(nexus_test::ObserveTerminal(
        [] { nexus_test::RecordTerminalObservation(true); }));
}
TEST(TerminalWatchdog, DuplicateEvidenceCannotPassAsPark) {
    EXPECT_FALSE(nexus_test::ObserveTerminal([] {
        nexus_test::RecordTerminalObservation(true);
        nexus_test::RecordTerminalObservation(true);
        for (;;) {
            pause();
        }
    }));
}

TEST(TerminalWatchdog, ChildGoogleFailureBeforeEvidenceCannotPassAsPark) {
    EXPECT_FALSE(nexus_test::ObserveTerminal([] {
        EXPECT_EQ(1, 2);
        nexus_test::RecordTerminalObservation(true);
        for (;;) {
            pause();
        }
    }));
}
TEST(TerminalWatchdog, ExtraMockCallAfterEvidenceCannotPassAsPark) {
    EXPECT_FALSE(nexus_test::ObserveTerminal([] {
        testing::MockFunction<void()> observer;
        EXPECT_CALL(observer, Call()).WillOnce([] {
            nexus_test::RecordTerminalObservation(true);
        });
        observer.Call();
        observer.Call();
        for (;;) {
            pause();
        }
    }));
}

TEST(TerminalWatchdog, CallbackExceptionCannotEscapeChildIsolation) {
    int escaped[2];
    ASSERT_EQ(0, pipe(escaped));
    bool observed = true;
    try {
        observed = nexus_test::ObserveTerminal([] { throw 7; });
    } catch (...) {
        if (write(escaped[1], "x", 1) != 1)
            _exit(95);
        _exit(96);
    }
    close(escaped[1]);
    char marker;
    EXPECT_EQ(0, read(escaped[0], &marker, sizeof(marker)));
    close(escaped[0]);
    EXPECT_FALSE(observed);
}

TEST_F(G474System, ReturningObserverCannotResumeIsolatedTimerProgress) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        if (nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U).primary !=
            NX_SUCCESS)
            _exit(90);
        g_g474_model.timer.SR = TIM_SR_UIF;
        g_g474_model.hold_timer_ack = true;
        EXPECT_CALL(fault_mock, Observe(NX_FAULT_PLATFORM, NX_ERROR_IO,
                                        testing::_, testing::_))
            .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                         unsigned line) {
                const bool valid =
                    file != nullptr && line != 0U && nx_arch_irq_is_masked() &&
                    !g_g474_model.irq_enabled[TIM2_IRQn] &&
                    g_nx_stm32g474_system.timer_error == NX_ERROR_IO &&
                    !g_nx_stm32g474_system.started &&
                    g_nx_stm32g474_system.remaining_effects != 0U &&
                    g_nx_stm32g474_system.overflow_us == 0U &&
                    g_g474_model.timer.DIER == 0U &&
                    (g_g474_model.timer.CR1 & TIM_CR1_CEN) == 0U;
                nexus_test::RecordTerminalObservation(valid);
            });
        nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system);
        _exit(99);
    }));
}

TEST_F(G474System, UnisolatableAckFaultRetainsMaskBeforeTerminalObservation) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U).primary);
    g_g474_model.timer.SR = TIM_SR_UIF;
    g_g474_model.hold_timer_ack = true;
    g_g474_model.hold_timer_source_off = true;
    EXPECT_CALL(fault_mock,
                Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system),
                 ParkSignal);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects &
                      NX_STM32G474_EFFECT_TIMER);
    EXPECT_EQ(0U, g_nx_stm32g474_system.overflow_us);
}

TEST_F(G474System, RefusedCounterIsolationRetainsTerminalOwnership) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U).primary);
    g_g474_model.timer.SR = TIM_SR_UIF;
    g_g474_model.irq_active[TIM2_IRQn] = true;
    g_g474_model.hold_timer_ack = true;
    g_g474_model.ignore_write = &g_g474_model.timer.CR1;
    EXPECT_CALL(fault_mock,
                Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system),
                 ParkSignal);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(0U, g_g474_model.timer.DIER);
    EXPECT_NE(0U, g_g474_model.timer.CR1 & TIM_CR1_CEN);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM2_IRQn]);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32g474_system.timer_error);
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects &
                      NX_STM32G474_EFFECT_TIMER);
    EXPECT_EQ(0U, g_nx_stm32g474_system.overflow_us);
}

TEST_F(G474System, RefusedVectorIsolationRetainsTerminalOwnership) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U).primary);
    g_g474_model.timer.SR = TIM_SR_UIF;
    g_g474_model.irq_active[TIM2_IRQn] = true;
    g_g474_model.hold_timer_ack = true;
    g_g474_model.hold_irq_off = true;
    EXPECT_CALL(fault_mock,
                Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system),
                 ParkSignal);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(0U, g_g474_model.timer.DIER);
    EXPECT_EQ(0U, g_g474_model.timer.CR1 & TIM_CR1_CEN);
    EXPECT_TRUE(g_g474_model.irq_enabled[TIM2_IRQn]);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32g474_system.timer_error);
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects &
                      NX_STM32G474_EFFECT_TIMER);
    EXPECT_EQ(0U, g_nx_stm32g474_system.overflow_us);
}

TEST_F(G474System, FaultBeforeIrqSaveCannotEscapeAsAStaleTimeSnapshot) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8U).primary);
    g_g474_model.timer.SR = TIM_SR_UIF;
    g_g474_model.timer.CNT = 7U;
    g_g474_model.hold_timer_ack = true;
    g_g474_model.before_irq_save = [](void*) {
        g_g474_model.irq_active[TIM2_IRQn] = true;
        nx_stm32g474_system_timer_irq(&g_nx_stm32g474_system);
        g_g474_model.irq_active[TIM2_IRQn] = false;
    };
    EXPECT_CALL(fault_mock,
                Observe(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                     unsigned line) {
            EXPECT_NE(nullptr, file);
            EXPECT_NE(0U, line);
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32g474_system_now(&g_nx_stm32g474_system), ParkSignal);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32g474_system.timer_error);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
}

/** \brief Mock only the terminal observation port, never timer operations. */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    fault_port->Observe(domain, reason, file, line);
}

TEST_F(G474System, ForeignTimerControlModesRejectBeforeGlobalClockEffects) {
    volatile uint32_t* fields[] = {
        &g_g474_model.timer.CR2, &g_g474_model.timer.SMCR,
        &g_g474_model.timer.CCER, &g_g474_model.timer.CCMR1,
        &g_g474_model.timer.CCMR2};
    for (auto* field : fields) {
        nx_g474_model_reset();
        *field = 1U;
        const auto result =
            nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(0U, g_g474_model.writes);
        EXPECT_FALSE(g_g474_model.timer_update_observed);
    }
}
TEST_F(G474System, PoweredTimerIsReadmittedBeforeResetAndUpdateEvent) {
    g_g474_model.timer_off_reads_zero = true;
    g_g474_model.timer.CR2 = TIM_CR2_MMS_1;
    const auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_FALSE(g_g474_model.timer_update_observed);
    EXPECT_EQ(TIM_CR2_MMS_1, g_g474_model.timer.CR2);
    EXPECT_EQ(0U, g_g474_model.rcc.APB1RSTR1 & RCC_APB1RSTR1_TIM2RST);
    EXPECT_EQ(0U, g_g474_model.rcc.APB1ENR1 & RCC_APB1ENR1_TIM2EN);
}

TEST_F(G474System, InitialSoftwareUpdateCannotEscapeAsATriggerOutput) {
    const auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    ASSERT_EQ(NX_SUCCESS, result.primary);
    EXPECT_TRUE(g_g474_model.timer_update_observed);
    EXPECT_FALSE(g_g474_model.timer_trigger_observed);
    EXPECT_EQ(0U, g_g474_model.timer.CR2);
}

TEST_F(G474System, RefusedTriggerRestoreRetainsTimerUntilStopRetry) {
    g_g474_model.hold_timer_trigger_restore = true;
    const auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects & NX_STM32G474_EFFECT_TIMER);
    EXPECT_FALSE(g_g474_model.timer_trigger_observed);
    g_g474_model.hold_timer_trigger_restore = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_EQ(0U, g_g474_model.timer.CR2);
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
}
TEST_F(G474System, RefusedResetReleaseRetainsItsGateAndEffectForRetry) {
    g_g474_model.hold_timer_reset_release = true;
    const auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_ERROR_IO, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects & NX_STM32G474_EFFECT_TIMER);
    EXPECT_NE(0U, g_g474_model.rcc.APB1ENR1 & RCC_APB1ENR1_TIM2EN);
    EXPECT_FALSE(g_g474_model.timer_update_observed);
    g_g474_model.hold_timer_reset_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_EQ(0U, g_g474_model.rcc.APB1RSTR1 & RCC_APB1RSTR1_TIM2RST);
}
TEST_F(G474System, ForeignAssertedTimerResetRejectsBeforeAnyMutation) {
    g_g474_model.rcc.APB1RSTR1 = RCC_APB1RSTR1_TIM2RST;
    const auto result = nx_stm32g474_system_start(&g_nx_stm32g474_system, 8);
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(0U, g_g474_model.writes);
}

TEST_F(G474System, PendingDuringSourceWithdrawalCannotOutliveSuccessfulStop) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.timer_repend_on_dier_off = true;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_FALSE(g_g474_model.irq_pending[TIM2_IRQn]);
    EXPECT_FALSE(g_g474_model.irq_enabled[TIM2_IRQn]);
    EXPECT_EQ(0U, g_nx_stm32g474_system.remaining_effects);
}
TEST_F(G474System, RefusedFinalPendingClearRetainsTheGateForStopRetry) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.timer_repend_on_dier_off = true;
    g_g474_model.hold_timer_pending_clear = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_TRUE(g_g474_model.irq_pending[TIM2_IRQn]);
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects &
                      NX_STM32G474_EFFECT_TIMER);
    EXPECT_NE(0U, g_g474_model.rcc.APB1ENR1 & RCC_APB1ENR1_TIM2EN);
    g_g474_model.hold_timer_pending_clear = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_FALSE(g_g474_model.irq_pending[TIM2_IRQn]);
}
TEST_F(G474System, UnacknowledgedStatusKeepsTimebaseOwnershipUntilRetry) {
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32g474_system_start(&g_nx_stm32g474_system, 8).primary);
    g_g474_model.timer.SR = TIM_SR_UIF;
    g_g474_model.ignore_write = &g_g474_model.timer.SR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
    EXPECT_NE(0U, g_nx_stm32g474_system.remaining_effects &
                      NX_STM32G474_EFFECT_TIMER);
    g_g474_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_system_stop(&g_nx_stm32g474_system, 8));
}
