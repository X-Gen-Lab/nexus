/**
 * \file            g474_pin_state_factory_test.cpp
 * \brief           Actual System, static factory and retained cold pad lease
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pin_state_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_system.h"
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
constexpr uint32_t Pin = 1U << 5U;
constexpr uint32_t Pair = 3U << 10U;
bool injected;
unsigned observed;
bool gpio_unmasked;
void LoseAfterDefaultDrive(volatile uint32_t* reg, uint32_t value) {
    if (reg == &GPIOA->MODER && (value & Pair) == (1U << 10U) && !injected) {
        injected = true;
        RCC->AHB2ENR &= ~static_cast<uint32_t>(RCC_AHB2ENR_GPIOAEN);
    }
}
void ObserveGpioMask(volatile uint32_t* reg, uint32_t value) {
    (void)value;
    const auto address = reinterpret_cast<uintptr_t>(reg);
    const auto first = reinterpret_cast<uintptr_t>(GPIOA);
    if (address >= first && address < first + sizeof(*GPIOA)) {
        ++observed;
        gpio_unmasked = gpio_unmasked || !nx_arch_irq_is_masked();
    }
}
void GateOffOnPinRead(volatile uint32_t* reg) {
    if (reg == &GPIOA->OTYPER && !injected) {
        injected = true;
        RCC->AHB2ENR &= ~static_cast<uint32_t>(RCC_AHB2ENR_GPIOAEN);
    }
}
void ResetAfterGate(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->AHB2ENR && (value & RCC_AHB2ENR_GPIOAEN) != 0U &&
        !injected) {
        injected = true;
        RCC->AHB2RSTR |= RCC_AHB2RSTR_GPIOARST;
    }
}
void LoseAfterColdLatch(volatile uint32_t* reg, uint32_t value) {
    if (reg == &GPIOA->BSRR && (value & Pin) != 0U && !injected) {
        injected = true;
        RCC->AHB2RSTR |= RCC_AHB2RSTR_GPIOARST;
    }
}
void ObserveWrites(volatile uint32_t* reg, uint32_t value) {
    (void)value;
    if (reg == &GPIOA->MODER || reg == &GPIOA->BSRR)
        ++observed;
}
} /* namespace */

class G474PinStateFactory : public testing::Test {
  protected:
    void SetUp() override {
        nx_pin_state_model_reset();
        injected = false;
        observed = 0U;
        GPIOA->MODER = Pair;
        GPIOA->ODR = Pin;
        GPIOA->PUPDR = 1U << 10U;
        GPIOA->AFR[0] = 7U << 20U;
    }
    void TearDown() override {
        /* Never clear an injected fault or reset a retained owner for cleanup.
         */
        if (nx_platform_stop() != NX_SUCCESS)
            std::abort();
        EXPECT_EQ(0U, g_pin_state_model.forbidden);
        EXPECT_EQ(0U, g_pin_state_model.unselected_change);
    }
    const nx_pin_state_port_t* Port() const {
        return nx_factory_pin_state(NX_PIN_STATE_ID_STATUS);
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
        ASSERT_TRUE(g_nx_stm32g474_system.started);
    }
    void ColdImage() {
        EXPECT_EQ(Pair, GPIOA->MODER);
        EXPECT_EQ(Pin, GPIOA->ODR);
        EXPECT_EQ(1U << 10U, GPIOA->PUPDR);
        EXPECT_EQ(7U << 20U, GPIOA->AFR[0]);
    }
};

TEST_F(G474PinStateFactory, LookupIsPureAndCannotInitializeReleasedPads) {
    const auto* face = Port();
    ASSERT_NE(nullptr, face);
    EXPECT_EQ(face, Port());
    EXPECT_EQ(nullptr, nx_factory_pin_state(NX_PIN_STATE_ID_COUNT));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_pin_state_select(face, NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(0U, g_pin_state_model.reads);
    EXPECT_EQ(0U, g_pin_state_model.writes);
}
TEST_F(G474PinStateFactory, RealSystemDefaultSleepAndColdImageRoundTrip) {
    g_pin_state_model.after_write = ObserveWrites;
    Start();
    EXPECT_EQ(1U << 10U, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOA->ODR);
    EXPECT_EQ(NX_SUCCESS, nx_pin_state_select(Port(), NX_PIN_STATE_SLEEP));
    EXPECT_EQ(Pair, GPIOA->MODER);
    EXPECT_EQ(NX_SUCCESS, nx_pin_state_select(Port(), NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    ColdImage();
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN);
    EXPECT_GT(observed, 0U);
    const auto accesses = g_pin_state_model.reads + g_pin_state_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_pin_state_select(Port(), NX_PIN_STATE_SLEEP));
    EXPECT_EQ(accesses, g_pin_state_model.reads + g_pin_state_model.writes);
}
TEST_F(G474PinStateFactory,
       ExistingGateAndUnselectedFieldsRemainOwnedElsewhere) {
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    GPIOA->MODER |= 2U;
    GPIOA->ODR |= 1U;
    Start();
    EXPECT_EQ(NX_SUCCESS, nx_pin_state_select(Port(), NX_PIN_STATE_SLEEP));
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(Pair | 2U, GPIOA->MODER);
    EXPECT_EQ(Pin | 1U, GPIOA->ODR);
    EXPECT_EQ(RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN, RCC->AHB2ENR);
}
TEST_F(G474PinStateFactory, HeldResetRejectsBeforeAnyGpioOrSystemEffect) {
    for (uint32_t gate : {0U, static_cast<uint32_t>(RCC_AHB2ENR_GPIOAEN)}) {
        RCC->AHB2ENR = gate;
        RCC->AHB2RSTR = RCC_AHB2RSTR_GPIOARST;
        nx_pin_state_model_counters();
        const auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_BUSY, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, g_pin_state_model.gpio_reads);
        EXPECT_EQ(0U, g_pin_state_model.gpio_writes);
        EXPECT_EQ(0U, g_pin_state_model.writes);
        EXPECT_FALSE(g_nx_stm32g474_system.started);
        EXPECT_EQ(RCC_AHB2RSTR_GPIOARST, RCC->AHB2RSTR);
        RCC->AHB2RSTR = 0U;
        EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    }
}
TEST_F(G474PinStateFactory, ResetAfterOwnedGateEnablePreventsColdProbe) {
    g_pin_state_model.after_write = ResetAfterGate;
    const auto result = nx_platform_start();
    EXPECT_TRUE(injected);
    EXPECT_NE(NX_SUCCESS, result.primary);
    EXPECT_EQ(0U, g_pin_state_model.gpio_reads);
    EXPECT_EQ(0U, g_pin_state_model.gpio_writes);
    EXPECT_EQ(RCC_AHB2RSTR_GPIOARST, RCC->AHB2RSTR);
    g_pin_state_model.after_write = nullptr;
    RCC->AHB2RSTR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
}
TEST_F(G474PinStateFactory, LossDuringColdCaptureStopsAtNextProtectedAccess) {
    g_pin_state_model.after_read = GateOffOnPinRead;
    const auto result = nx_platform_start();
    EXPECT_TRUE(injected);
    EXPECT_NE(NX_SUCCESS, result.primary);
    EXPECT_EQ(0U, g_pin_state_model.forbidden);
    EXPECT_EQ(0U, g_pin_state_model.gpio_writes);
    g_pin_state_model.after_read = nullptr;
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
}
TEST_F(G474PinStateFactory, FailedDefaultRetainsRealWithdrawAndColdRestore) {
    g_pin_state_model.refuse_write = &GPIOA->BSRR;
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    ColdImage();
    g_pin_state_model.refuse_write = nullptr;
    Start();
}
TEST_F(G474PinStateFactory, GlobalRestoreRetryGuardsAccessAfterDriverReleased) {
    Start();
    g_pin_state_model.refuse_write = &GPIOA->PUPDR;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    const auto before =
        g_pin_state_model.gpio_reads + g_pin_state_model.gpio_writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_pin_state_select(Port(), NX_PIN_STATE_DEFAULT));
    EXPECT_EQ(before,
              g_pin_state_model.gpio_reads + g_pin_state_model.gpio_writes);
    for (bool reset : {false, true}) {
        RCC->AHB2ENR = reset ? RCC_AHB2ENR_GPIOAEN : 0U;
        RCC->AHB2RSTR = reset ? RCC_AHB2RSTR_GPIOARST : 0U;
        nx_pin_state_model_counters();
        EXPECT_EQ(NX_ERROR_STATE, nx_platform_stop());
        EXPECT_EQ(0U, g_pin_state_model.gpio_reads);
        EXPECT_EQ(0U, g_pin_state_model.gpio_writes);
        EXPECT_TRUE(g_nx_stm32g474_system.started);
    }
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN;
    RCC->AHB2RSTR = 0U;
    g_pin_state_model.refuse_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    ColdImage();
}
TEST_F(G474PinStateFactory, LossDuringRestoreRetainsUntilRealExplicitRetry) {
    Start();
    g_pin_state_model.after_write = LoseAfterColdLatch;
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_stop());
    EXPECT_TRUE(injected);
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    EXPECT_EQ(0U, g_pin_state_model.forbidden);
    g_pin_state_model.after_write = nullptr;
    RCC->AHB2RSTR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    ColdImage();
}
TEST_F(G474PinStateFactory, ClockReleaseFailureRetainsOnlyItsActualAddedBit) {
    Start();
    g_pin_state_model.refuse_write = &RCC->AHB2ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    ColdImage();
    nx_pin_state_model_counters();
    g_pin_state_model.refuse_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, g_pin_state_model.gpio_reads);
    EXPECT_EQ(0U, g_pin_state_model.gpio_writes);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN);
}

TEST_F(G474PinStateFactory,
       FailedInitializationKeepsDriverAndGlobalCleanupResponsibility) {
    g_pin_state_model.after_write = LoseAfterDefaultDrive;
    const auto result = nx_platform_start();
    EXPECT_TRUE(injected);
    EXPECT_EQ(NX_ERROR_STATE, result.primary);
    EXPECT_EQ(NX_ERROR_STATE, result.cleanup);
    EXPECT_NE(0U, result.remaining_effects);
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    const auto before =
        g_pin_state_model.gpio_reads + g_pin_state_model.gpio_writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_stop());
    EXPECT_EQ(before,
              g_pin_state_model.gpio_reads + g_pin_state_model.gpio_writes);
    EXPECT_EQ(0U, g_pin_state_model.forbidden);
    g_pin_state_model.after_write = nullptr;
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    ColdImage();
    EXPECT_FALSE(g_nx_stm32g474_system.started);
}
TEST_F(G474PinStateFactory,
       ColdAndRuntimeGpioWritesKeepSavedMaskWithoutMaskingSystemPoll) {
    gpio_unmasked = false;
    g_pin_state_model.after_write = ObserveGpioMask;
    Start();
    EXPECT_FALSE(nx_arch_irq_is_masked());
    EXPECT_EQ(NX_SUCCESS, nx_pin_state_select(Port(), NX_PIN_STATE_SLEEP));
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_GT(observed, 0U);
    EXPECT_FALSE(gpio_unmasked);
    EXPECT_FALSE(nx_arch_irq_is_masked());
}
