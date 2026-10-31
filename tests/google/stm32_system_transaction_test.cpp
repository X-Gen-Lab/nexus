/**
 * \file            stm32_system_transaction_test.cpp
 * \brief           Owned F407 clock transaction and failed readback contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/core/fault.h"
#include "stm32f407_system.h"
/* A missing CMSIS producer is an observable RED, not a fake clock provider. */
extern uint32_t SystemCoreClock __attribute__((weak));
uint32_t HAL_GetTick(void) __attribute__((weak));
}
#include "terminal_watchdog.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>

nx_stm32_system_t g_nx_stm32_system{};

namespace {
class HardwarePort {
  public:
    MOCK_METHOD(void, Write, (volatile uint32_t * reg, uint32_t value));
    MOCK_METHOD(void, Fault,
                (nx_fault_domain_t, nx_result_t, const char*, unsigned));
};
struct ParkSignal {};
HardwarePort* hardware_port;
uint32_t mask;
bool privileged = true;
bool handler;
bool irq_enabled;
bool irq_pending;
bool irq_active;
bool refuse_irq_disable;
bool source_prevents_vector_proof;
bool hold_hse;
bool hold_hsi;
bool hold_pll;
bool hold_switch;
bool update_observed;
bool update_trigger_observed;
void (*before_save)();

class Stm32System : public testing::Test {
  protected:
    RCC_TypeDef rcc{};
    FLASH_TypeDef flash{};
    PWR_TypeDef power{};
    TIM_TypeDef timer{};
    testing::NiceMock<HardwarePort> hardware_mock;

    void SetUp() override {
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &timer;
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        rcc.PLLCFGR = UINT32_C(0x24003010);
        flash.ACR = FLASH_ACR_ICEN;
        power.CR = PWR_CR_DBP;
        mask = 0U;
        privileged = true;
        handler = false;
        irq_enabled = irq_pending = irq_active = false;
        refuse_irq_disable = false;
        source_prevents_vector_proof = false;
        hold_hse = hold_hsi = hold_pll = hold_switch = false;
        update_observed = false;
        update_trigger_observed = false;
        before_save = nullptr;
        hardware_port = &hardware_mock;
        EXPECT_CALL(hardware_mock, Write(testing::_, testing::_))
            .Times(testing::AnyNumber());
        ON_CALL(hardware_mock, Write(testing::_, testing::_))
            .WillByDefault([](volatile uint32_t* reg, uint32_t value) {
                auto* hardware_timer = g_nx_stm32_system.timer;
                if (reg == &hardware_timer->SR) {
                    *reg &= value;
                } else {
                    *reg = value;
                }
                if (reg == &hardware_timer->EGR && (value & TIM_EGR_UG) != 0U) {
                    update_observed = true;
                    if ((hardware_timer->CR2 & TIM_CR2_MMS) == 0U)
                        update_trigger_observed = true;
                    hardware_timer->SR |= TIM_SR_UIF;
                    hardware_timer->CNT = 0U;
                }
            });
    }
    void TearDown() override {
        /* Host fixture reset after observer interception is not recovery from
         * the production non-returning terminal path. */
        testing::Mock::VerifyAndClearExpectations(&hardware_mock);
        hold_hse = hold_hsi = hold_pll = hold_switch = false;
        refuse_irq_disable = false;
        source_prevents_vector_proof = false;
        before_save = nullptr;
        irq_active = false;
        mask = 0U;
        handler = false;
        privileged = true;
        EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
        hardware_port = nullptr;
    }
    nx_stm32_start_result_t Start() {
        return nx_stm32_system_start(&g_nx_stm32_system, 8U);
    }
};
} /* namespace */

TEST_F(Stm32System, ExactClockAndIndependentTimerHaveOneLiveEpoch) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(168000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(83U, timer.PSC);
    EXPECT_EQ(UINT32_MAX, timer.ARR);
    timer.CNT = 5U;
    EXPECT_EQ(5U, nx_stm32_system_now(&g_nx_stm32_system));
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(Stm32System, CmsisClockTracksConfirmedStartAndHsiReturn) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    EXPECT_EQ(16000000U, SystemCoreClock);
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(168000000U, SystemCoreClock);
    EXPECT_EQ(g_nx_stm32_system.core_hz, SystemCoreClock);
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_EQ(g_nx_stm32_system.core_hz, SystemCoreClock);
}

TEST_F(Stm32System, CmsisClockKeepsPllAfterTimerFailureAndRefusedHsiReturn) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    EXPECT_CALL(hardware_mock, Write(&timer.PSC, 83U))
        .WillOnce([](volatile uint32_t*, uint32_t) { hold_hsi = true; });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(RCC_CFGR_SWS_PLL, rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(168000000U, SystemCoreClock);
    EXPECT_EQ(g_nx_stm32_system.core_hz, SystemCoreClock);
}

TEST_F(Stm32System, CmsisClockDoesNotClaimPllAfterEarlySourceFailure) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    hold_hse = true;
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_EQ(g_nx_stm32_system.core_hz, SystemCoreClock);
}

TEST_F(Stm32System, ConfirmedHsiMetadataSurvivesLaterFlashRestoreRefusal) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    const uint32_t saved_flash = flash.ACR;
    ASSERT_EQ(0, Start().primary);
    EXPECT_CALL(hardware_mock, Write(&flash.ACR, saved_flash))
        .WillOnce([](volatile uint32_t*, uint32_t) {})
        .RetiresOnSaturation();
    EXPECT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, rcc.CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE));
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(16000000U, SystemCoreClock);
    EXPECT_EQ(16000000U, g_nx_stm32_system.core_hz);
}

TEST_F(Stm32System, ForeignClockRefusalPreservesObservedClockMetadata) {
    ASSERT_NE(nullptr, &SystemCoreClock);
    const uint32_t previous_clock = SystemCoreClock;
    SystemCoreClock = 84000000U;
    g_nx_stm32_system.core_hz = 84000000U;
    rcc.CR |= RCC_CR_PLLON | RCC_CR_PLLRDY;
    rcc.CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, Start().primary);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(RCC_CFGR_SWS_PLL, rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_EQ(84000000U, SystemCoreClock);
    EXPECT_EQ(84000000U, g_nx_stm32_system.core_hz);
    SystemCoreClock = previous_clock;
}

TEST_F(Stm32System, HalMillisecondsUseTheOwnedEpochAndModuloWrap) {
    auto* volatile get_tick = &HAL_GetTick;
    ASSERT_NE(nullptr, get_tick);
    EXPECT_EQ(0U, HAL_GetTick());
    ASSERT_EQ(0, Start().primary);
    timer.CNT = 1999U;
    EXPECT_EQ(1U, HAL_GetTick());
    timer.CNT = 2000U;
    EXPECT_EQ(2U, HAL_GetTick());
    g_nx_stm32_system.overflow_us =
        (static_cast<uint64_t>(UINT32_MAX) + 1U) * 1000U;
    timer.CNT = 1234U;
    EXPECT_EQ(1U, HAL_GetTick());
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, HAL_GetTick());
}

TEST_F(Stm32System, PendingAtSourceWithdrawalIsClearedBeforeOwnerRelease) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, 0U))
        .WillOnce([](volatile uint32_t* reg, uint32_t value) {
            irq_pending = true;
            *reg = value;
        })
        .RetiresOnSaturation();
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_FALSE(irq_pending);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_system_preflight(&g_nx_stm32_system));
}

TEST_F(Stm32System, RefusedStopFlagClearRetainsTimerAndClockForRetry) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, 0U))
        .WillOnce([](volatile uint32_t*, uint32_t) {})
        .RetiresOnSaturation();
    EXPECT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_NE(0U, rcc.APB1ENR & RCC_APB1ENR_TIM2EN);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, AssertedSourceCannotPreventItsOwnWithdrawal) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    irq_pending = true;
    source_prevents_vector_proof = true;
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_EQ(0U, timer.SR);
    EXPECT_FALSE(irq_pending);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, SoftwareUpdateIsClearedBeforePublishingAZeroEpoch) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_TRUE(update_observed);
    EXPECT_EQ(0U, timer.SR & TIM_SR_UIF);
    EXPECT_EQ(0U, timer.CNT);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_TRUE(g_nx_stm32_system.started);
}

TEST_F(Stm32System, PrescalerShadowUpdateDoesNotEmitAResetModeTrigger) {
    ASSERT_EQ(0, Start().primary);
    EXPECT_TRUE(update_observed);
    EXPECT_FALSE(update_trigger_observed);
    EXPECT_EQ(0U, timer.CR2 & TIM_CR2_MMS);
}

TEST_F(Stm32System, RefusedInitialUpdateClearCannotPublishLiveTime) {
    EXPECT_CALL(hardware_mock, Write(&timer.SR, 0U))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_NE(0U, rcc.APB1ENR & RCC_APB1ENR_TIM2EN);
}

TEST_F(Stm32System, RefusedSoftwareUpdateCannotReuseAnOldUpdateFlag) {
    timer.SR = TIM_SR_UIF;
    EXPECT_CALL(hardware_mock, Write(&timer.EGR, TIM_EGR_UG))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(Stm32System, RefusedInitialCounterZeroCannotPublishLiveTime) {
    EXPECT_CALL(hardware_mock, Write(&timer.CNT, 0U))
        .WillRepeatedly([](volatile uint32_t* reg, uint32_t) { *reg = 37U; });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
}

TEST_F(Stm32System, CounterFailureRetainsObservedCleanupResponsibility) {
    EXPECT_CALL(hardware_mock, Write(&timer.CNT, 0U))
        .WillRepeatedly([](volatile uint32_t* reg, uint32_t) { *reg = 37U; });
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1ENR, testing::_))
        .WillRepeatedly([](volatile uint32_t* reg, uint32_t value) {
            if ((*reg & RCC_APB1ENR_TIM2EN) != 0U)
                value |= RCC_APB1ENR_TIM2EN;
            *reg = value;
        });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_NE(0U, rcc.APB1ENR & RCC_APB1ENR_TIM2EN);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(Stm32System, SuccessfulStopRestoresOnlyAcquiredPowerAndClockFields) {
    const uint32_t old_pll = rcc.PLLCFGR;
    const uint32_t old_flash = flash.ACR;
    const uint32_t old_power = power.CR;
    ASSERT_EQ(0, Start().primary);
    power.CR |= PWR_CR_PVDE;
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(old_pll, rcc.PLLCFGR);
    EXPECT_EQ(old_flash, flash.ACR);
    EXPECT_EQ(old_power | PWR_CR_PVDE, power.CR);
    EXPECT_EQ(0U, rcc.APB1ENR & RCC_APB1ENR_PWREN);
}

TEST_F(Stm32System, ExistingPowerGateSurvivesOwnedTransaction) {
    rcc.APB1ENR |= RCC_APB1ENR_PWREN;
    ASSERT_EQ(0, Start().primary);
    ASSERT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_NE(0U, rcc.APB1ENR & RCC_APB1ENR_PWREN);
}

TEST_F(Stm32System, HseFailureRestoresSavedFieldsWithoutPublishingTime) {
    const uint32_t old_pll = rcc.PLLCFGR;
    const uint32_t old_flash = flash.ACR;
    const uint32_t old_power = power.CR;
    hold_hse = true;
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(old_pll, rcc.PLLCFGR);
    EXPECT_EQ(old_flash, flash.ACR);
    EXPECT_EQ(old_power, power.CR);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

class Stm32AdjustedHsiTrim : public Stm32System,
                             public testing::WithParamInterface<uint32_t> {};

TEST_P(Stm32AdjustedHsiTrim, RejectsBeforeAnyWriteOrOwnership) {
    rcc.CR = (rcc.CR & ~(uint32_t)RCC_CR_HSITRIM) |
             (GetParam() << RCC_CR_HSITRIM_Pos) | (0xA5U << RCC_CR_HSICAL_Pos);
    const uint32_t saved_clock = rcc.CR;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_system_preflight(&g_nx_stm32_system));
    const auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(0U, g_nx_stm32_system.core_hz);
    EXPECT_FALSE(g_nx_stm32_system.owned_power_clock);
    EXPECT_FALSE(g_nx_stm32_system.power_snapshotted);
    EXPECT_EQ(saved_clock, rcc.CR);
    EXPECT_FALSE(irq_enabled);
    EXPECT_FALSE(irq_pending);
    EXPECT_EQ(0U, mask);
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
}

INSTANTIATE_TEST_SUITE_P(Adjusted, Stm32AdjustedHsiTrim,
                         testing::Values(0U, 15U, 17U, 31U));

class Stm32FactoryHsiCalibration
    : public Stm32System,
      public testing::WithParamInterface<uint32_t> {};

TEST_P(Stm32FactoryHsiCalibration, DefaultTrimPreservesFactoryCalibration) {
    rcc.CR |= GetParam() << RCC_CR_HSICAL_Pos;
    const uint32_t calibration = rcc.CR & (RCC_CR_HSITRIM | RCC_CR_HSICAL);
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_system_preflight(&g_nx_stm32_system));
    testing::Mock::VerifyAndClearExpectations(&hardware_mock);
    ASSERT_EQ(0, Start().primary);
    EXPECT_EQ(168000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(16000000U, g_nx_stm32_system.core_hz);
    EXPECT_EQ(calibration, rcc.CR & (RCC_CR_HSITRIM | RCC_CR_HSICAL));
}

INSTANTIATE_TEST_SUITE_P(Factory, Stm32FactoryHsiCalibration,
                         testing::Values(0U, 0x55U, 0xAAU, 0xFFU));

TEST_F(Stm32System, ForeignClockIsRejectedBeforeAnyOwnedWrite) {
    rcc.CR |= RCC_CR_PLLON;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, Start().primary);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_NE(0U, rcc.CR & RCC_CR_PLLON);
}

TEST_F(Stm32System, ForeignTimerIsRejectedBeforeClockChanges) {
    timer.CR1 = TIM_CR1_CEN;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, Start().primary);
    EXPECT_EQ(TIM_CR1_CEN, timer.CR1);
    EXPECT_EQ(0U, rcc.CR & RCC_CR_HSEON);
    timer.CR1 = 0U;
}

TEST_F(Stm32System, PendingReservedIrqIsAColdAdmissionConflict) {
    irq_pending = true;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_NE(0, Start().primary);
    EXPECT_TRUE(irq_pending);
    irq_pending = false;
}

TEST_F(Stm32System, RetainedTimerModesRejectBeforeClockOrUpdateEffects) {
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    for (volatile uint32_t* field : {&timer.CR2, &timer.SMCR, &timer.CCMR1,
                                     &timer.CCMR2, &timer.CCER, &timer.DCR}) {
        *field = 1U;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_system_preflight(&g_nx_stm32_system));
        EXPECT_EQ(1U, *field);
        EXPECT_FALSE(update_observed);
        *field = 0U;
    }
}

TEST_F(Stm32System, PoweredForeignTimerKeepsItsRegistersDuringCleanupRetry) {
    bool refuse_gate_drop = true;
    EXPECT_CALL(hardware_mock, Write(&timer.CR1, testing::_)).Times(0);
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, testing::_)).Times(0);
    EXPECT_CALL(hardware_mock, Write(&timer.EGR, testing::_)).Times(0);
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1ENR, testing::_))
        .WillRepeatedly([&](volatile uint32_t* reg, uint32_t value) {
            if ((value & RCC_APB1ENR_TIM2EN) != 0U) {
                *reg = value;
                timer.SMCR = 1U;
            } else if ((*reg & RCC_APB1ENR_TIM2EN) == 0U || !refuse_gate_drop) {
                *reg = value;
            }
        });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_EQ(1U, timer.SMCR);
    EXPECT_NE(0U, result.remaining_effects);
    refuse_gate_drop = false;
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(1U, timer.SMCR);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, RefusedTimerResetCannotPublishAnUpdateOrLiveEpoch) {
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1RSTR, testing::_))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock, Write(&timer.EGR, testing::_)).Times(0);
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_FALSE(update_observed);
}

TEST_F(Stm32System, ForeignAssertedTimerResetIsNotAnAvailableResource) {
    rcc.APB1RSTR = RCC_APB1RSTR_TIM2RST;
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_system_preflight(&g_nx_stm32_system));
    EXPECT_EQ(RCC_APB1RSTR_TIM2RST, rcc.APB1RSTR);
}

TEST_F(Stm32System, RefusedOwnedResetReleaseRetainsCleanupUntilRetry) {
    bool refuse_reset_release = true;
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1RSTR, testing::_))
        .WillRepeatedly([&](volatile uint32_t* reg, uint32_t value) {
            if ((value & RCC_APB1RSTR_TIM2RST) != 0U || !refuse_reset_release)
                *reg = value;
        });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_NE(0, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_NE(0U, rcc.APB1RSTR & RCC_APB1RSTR_TIM2RST);
    refuse_reset_release = false;
    EXPECT_EQ(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_EQ(0U, rcc.APB1RSTR & RCC_APB1RSTR_TIM2RST);
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, MaskedOrUnprivilegedStartHasNoEffects) {
    EXPECT_CALL(hardware_mock, Write(testing::_, testing::_)).Times(0);
    mask = 1U;
    EXPECT_NE(0, Start().primary);
    mask = 0U;
    privileged = false;
    EXPECT_NE(0, Start().primary);
    privileged = true;
    EXPECT_EQ(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, RefusedFlashLatencyCannotPublishAHighSpeedClock) {
    EXPECT_CALL(hardware_mock, Write(&flash.ACR, testing::_))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0U, rcc.CFGR & RCC_CFGR_SWS);
    EXPECT_FALSE(g_nx_stm32_system.started);
}

TEST_F(Stm32System, RefusedTimerGateRetainsOnlyFailedCleanupResponsibility) {
    EXPECT_CALL(hardware_mock, Write(&rcc.APB1ENR, testing::_))
        .WillRepeatedly([&](volatile uint32_t* reg, uint32_t value) {
            *reg = value & ~(uint32_t)RCC_APB1ENR_TIM2EN;
        });
    auto result = Start();
    EXPECT_NE(0, result.primary);
    EXPECT_EQ(0, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
}

TEST_F(Stm32System, RefusedIrqWithdrawalKeepsOwnershipButEndsLiveTime) {
    ASSERT_EQ(0, Start().primary);
    timer.CNT = 13U;
    refuse_irq_disable = true;
    EXPECT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_TRUE(irq_enabled);
}

TEST_F(Stm32System, RetainedTimerStillAcknowledgesOverflowWithoutLiveEpoch) {
    ASSERT_EQ(0, Start().primary);
    refuse_irq_disable = true;
    ASSERT_NE(0, nx_stm32_system_stop(&g_nx_stm32_system, 8U));
    timer.SR = TIM_SR_UIF;
    nx_stm32_system_timer_irq(&g_nx_stm32_system);
    EXPECT_EQ(0U, timer.SR & TIM_SR_UIF);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects);
}

TEST_F(Stm32System, IsolatedAckFaultStillObservesTerminalOwnership) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    timer.CNT = 7U;
    irq_active = true;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                     unsigned line) {
            EXPECT_NE(nullptr, file);
            EXPECT_NE(0U, line);
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_timer_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(0U, g_nx_stm32_system.overflow_us);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects & 2U);
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_FALSE(irq_enabled);
    EXPECT_EQ(0U, nx_stm32_system_now(&g_nx_stm32_system));
    EXPECT_TRUE(nx_arch_irq_is_masked());
}

TEST_F(Stm32System, ReturningObserverCannotResumeIsolatedTimerProgress) {
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        if (Start().primary != 0)
            _exit(90);
        timer.SR = TIM_SR_UIF;
        EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
            .WillOnce([](volatile uint32_t*, uint32_t) {});
        EXPECT_CALL(hardware_mock, Fault(NX_FAULT_PLATFORM, NX_ERROR_IO,
                                         testing::_, testing::_))
            .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                         unsigned line) {
                const bool valid =
                    file != nullptr && line != 0U && mask == 1U &&
                    !irq_enabled &&
                    g_nx_stm32_system.timer_error == NX_ERROR_IO &&
                    !g_nx_stm32_system.started &&
                    g_nx_stm32_system.remaining_effects != 0U &&
                    g_nx_stm32_system.overflow_us == 0U &&
                    g_nx_stm32_system.timer->DIER == 0U &&
                    (g_nx_stm32_system.timer->CR1 & TIM_CR1_CEN) == 0U;
                nexus_test::RecordTerminalObservation(valid);
            });
        nx_stm32_system_timer_irq(&g_nx_stm32_system);
        _exit(99);
    }));
}

TEST_F(Stm32System, UnisolatableAckFaultRetainsMaskBeforeTerminalObservation) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, 0U))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_timer_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects & 2U);
    EXPECT_EQ(0U, g_nx_stm32_system.overflow_us);
}

TEST_F(Stm32System, RefusedCounterIsolationRetainsTerminalOwnership) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    irq_active = true;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, 0U)).Times(1);
    EXPECT_CALL(hardware_mock, Write(&timer.CR1, 0U))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_timer_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_NE(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_FALSE(irq_enabled);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects & 2U);
    EXPECT_EQ(0U, g_nx_stm32_system.overflow_us);
}

TEST_F(Stm32System, RefusedVectorIsolationRetainsTerminalOwnership) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    irq_active = true;
    refuse_irq_disable = true;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillOnce([](volatile uint32_t*, uint32_t) {});
    EXPECT_CALL(hardware_mock, Write(&timer.DIER, 0U)).Times(1);
    EXPECT_CALL(hardware_mock, Write(&timer.CR1, 0U)).Times(1);
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char*, unsigned) {
            EXPECT_EQ(1U, mask);
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_timer_irq(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_EQ(0U, timer.DIER);
    EXPECT_EQ(0U, timer.CR1 & TIM_CR1_CEN);
    EXPECT_TRUE(irq_enabled);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
    EXPECT_NE(0U, g_nx_stm32_system.remaining_effects & 2U);
    EXPECT_EQ(0U, g_nx_stm32_system.overflow_us);
}

TEST_F(Stm32System, FaultBeforeIrqSaveCannotEscapeAsAStaleTimeSnapshot) {
    ASSERT_EQ(0, Start().primary);
    timer.SR = TIM_SR_UIF;
    timer.CNT = 7U;
    EXPECT_CALL(hardware_mock, Write(&timer.SR, ~(uint32_t)TIM_SR_UIF))
        .WillRepeatedly([](volatile uint32_t*, uint32_t) {});
    before_save = []() {
        irq_active = true;
        nx_stm32_system_timer_irq(&g_nx_stm32_system);
        irq_active = false;
    };
    EXPECT_CALL(hardware_mock,
                Fault(NX_FAULT_PLATFORM, NX_ERROR_IO, testing::_, testing::_))
        .WillOnce([](nx_fault_domain_t, nx_result_t, const char* file,
                     unsigned line) {
            EXPECT_NE(nullptr, file);
            EXPECT_NE(0U, line);
            EXPECT_TRUE(nx_arch_irq_is_masked());
            throw ParkSignal{};
        });
    EXPECT_THROW(nx_stm32_system_now(&g_nx_stm32_system), ParkSignal);
    EXPECT_EQ(1U, mask);
    EXPECT_EQ(NX_ERROR_IO, g_nx_stm32_system.timer_error);
    EXPECT_FALSE(g_nx_stm32_system.started);
}

/** \brief Advance only external hardware readiness and source acknowledgement.
 */
extern "C" void nx_stm32_system_model_poll(void* raw) {
    auto* system = static_cast<nx_stm32_system_t*>(raw);
    auto* rcc = system->rcc;
    if (hold_hsi)
        rcc->CR &= ~(uint32_t)RCC_CR_HSIRDY;
    else if ((rcc->CR & RCC_CR_HSION) != 0U)
        rcc->CR |= RCC_CR_HSIRDY;
    if ((rcc->CR & RCC_CR_HSEON) != 0U && !hold_hse)
        rcc->CR |= RCC_CR_HSERDY;
    else
        rcc->CR &= ~(uint32_t)RCC_CR_HSERDY;
    if ((rcc->CR & RCC_CR_PLLON) != 0U && !hold_pll)
        rcc->CR |= RCC_CR_PLLRDY;
    else
        rcc->CR &= ~(uint32_t)RCC_CR_PLLRDY;
    if (!hold_switch)
        rcc->CFGR = (rcc->CFGR & ~(uint32_t)RCC_CFGR_SWS) |
                    ((rcc->CFGR & (uint32_t)RCC_CFGR_SW) << 2U);
}

/** \brief GoogleMock observes writes at the hardware port, not inside logic. */
extern "C" void nx_stm32_system_model_write(volatile uint32_t* reg,
                                            uint32_t value) {
    hardware_port->Write(reg, value);
}

/** \brief Model existing enabled, pending and active vector owners. */
extern "C" bool nx_stm32_system_model_irq_available(int) {
    return !irq_enabled && !irq_pending && !irq_active;
}

/** \brief Refuse an actual modeled withdrawal without pretending it succeeded.
 */
extern "C" int nx_stm32_system_model_irq_control(int, bool enabled) {
    if (!enabled && refuse_irq_disable)
        return -1;
    if (!enabled && source_prevents_vector_proof &&
        (g_nx_stm32_system.timer->DIER & TIM_DIER_UIE) != 0U &&
        (g_nx_stm32_system.timer->SR & TIM_SR_UIF) != 0U) {
        irq_pending = true;
        return -1;
    }
    irq_enabled = enabled;
    irq_pending = false;
    return 0;
}

/** \brief Save the injected incoming CPU mask for one short metadata span. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    if (before_save != nullptr && mask == 0U) {
        auto callback = before_save;
        before_save = nullptr;
        callback();
    }
    nx_arch_irq_state_t previous = {mask};
    mask = 1U;
    return previous;
}
/** \brief Restore the exact injected CPU mask. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    mask = previous.value;
}
/** \brief Query injected privilege at the external CPU boundary. */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/** \brief Query injected exception context. */
extern "C" bool nx_arch_in_isr(void) {
    return handler;
}
/** \brief Query the incoming mask without changing it. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask != 0U;
}
/** \brief Synchronous host register writes do not establish physical timing. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief The host model has no Cortex-M instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}

/** \brief The host-only observer stops before the retained terminal loop. */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    hardware_port->Fault(domain, reason, file, line);
}
