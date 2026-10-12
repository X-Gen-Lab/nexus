/**
 * \file            stm32_system_transaction_test.cpp
 * \brief           Owned F407 clock transaction and failed readback contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_system.h"
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>

nx_stm32_system_t g_nx_stm32_system{};

namespace {
class HardwarePort {
  public:
    MOCK_METHOD(void, Write, (volatile uint32_t * reg, uint32_t value));
};
HardwarePort* hardware_port;
uint32_t mask;
bool privileged = true;
bool handler;
bool irq_enabled;
bool irq_pending;
bool irq_active;
bool refuse_irq_disable;
bool hold_hse;
bool hold_hsi;
bool hold_pll;
bool hold_switch;

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
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        rcc.PLLCFGR = UINT32_C(0x24003010);
        flash.ACR = FLASH_ACR_ICEN;
        power.CR = PWR_CR_DBP;
        mask = 0U;
        privileged = true;
        handler = false;
        irq_enabled = irq_pending = irq_active = false;
        refuse_irq_disable = false;
        hold_hse = hold_hsi = hold_pll = hold_switch = false;
        hardware_port = &hardware_mock;
        EXPECT_CALL(hardware_mock, Write(testing::_, testing::_))
            .Times(testing::AnyNumber());
        ON_CALL(hardware_mock, Write(testing::_, testing::_))
            .WillByDefault(
                [](volatile uint32_t* reg, uint32_t value) { *reg = value; });
    }
    void TearDown() override {
        testing::Mock::VerifyAndClearExpectations(&hardware_mock);
        hold_hse = hold_hsi = hold_pll = hold_switch = false;
        refuse_irq_disable = false;
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
                    ((rcc->CFGR & RCC_CFGR_SW) << 2U);
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
    irq_enabled = enabled;
    irq_pending = false;
    return 0;
}

/** \brief Save the injected incoming CPU mask for one short metadata span. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
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
