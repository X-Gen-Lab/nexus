/**
 * \file            standby_test.cpp
 * \brief           F103RB WKUP admission, pending evidence and WFI return.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/components/clock_domain_owner.h"
#include "nexus/io/watchdog.h"
#include "nexus_bindings.h"
#include "system/platform.h"
#include "system/standby.h"
#include <gtest/gtest.h>

GPIO_TypeDef f1_test_standby_gpio;

namespace {
bool late_wake;
bool stuck_wuf;
unsigned clear_count;
unsigned wait_count;
unsigned stop_requests;
bool gpio_gate_stuck;
nx_result_t stop_result;
constexpr nx_stm32f1_platform_config_t rb{131072U};

class F103Standby : public testing::Test {
  protected:
    nx_clock_domain_t domain{};
    nx_clock_barrier_t barrier{};

    void SetUp() override {
        f1_test_rcc = {};
        f1_test_flash = {};
        f1_test_power = {};
        f1_test_exti = {};
        f1_test_rtc = {};
        f1_test_standby_gpio = {};
        SCB->SCR = SCB->ICSR = SCB->SHCSR = 0U;
        SysTick->LOAD = SysTick->VAL = 0U;
        NVIC->ISER[0] = NVIC->ISER[1] = 0U;
        NVIC->ISPR[0] = NVIC->ISPR[1] = 0U;
        NVIC->IABR[0] = NVIC->IABR[1] = 0U;
        RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        RCC->AHBENR = RCC_AHBENR_FLITFEN | RCC_AHBENR_SRAMEN;
        RCC->APB1ENR = RCC_APB1ENR_PWREN;
        RCC->APB2ENR = RCC_APB2ENR_IOPAEN;
        GPIOA->CRL = 0x44444444U;
        FLASH->OBR = FLASH_OBR_IWDG_SW | FLASH_OBR_nRST_STDBY;
        late_wake = stuck_wuf = gpio_gate_stuck = false;
        clear_count = wait_count = stop_requests = 0U;
        stop_result = NX_SUCCESS;
        ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
        ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    }
};

TEST_F(F103Standby, ExistingWakeAndStandbyEvidenceRemainUntilExplicitDiscard) {
    PWR->CSR = PWR_CSR_WUF | PWR_CSR_SBF | PWR_CSR_EWUP;
    constexpr uint32_t flags = PWR_CSR_WUF | PWR_CSR_SBF;
    const auto refused = nx_stm32f1_standby_pa0(barrier, false);
    EXPECT_EQ(NX_ERROR_BUSY, refused.status);
    EXPECT_EQ(flags, refused.before_flags);
    EXPECT_EQ(flags, refused.after_flags);
    EXPECT_EQ(PWR_CSR_WUF | PWR_CSR_SBF | PWR_CSR_EWUP, PWR->CSR);
    EXPECT_EQ(0U, clear_count);
    EXPECT_EQ(0U, wait_count);
}

TEST_F(F103Standby, HighWakePinAndRetainedRtcAreRefusedBeforeAcknowledgment) {
    GPIOA->IDR = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_standby_pa0(barrier, true).status);
    GPIOA->IDR = 0U;
    RCC->BDCR = RCC_BDCR_RTCSEL_LSE;
    RTC->CRL = RTC_CRL_ALRF;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_standby_pa0(barrier, true).status);
    EXPECT_EQ(RCC_BDCR_RTCSEL_LSE, RCC->BDCR);
    EXPECT_EQ(RTC_CRL_ALRF, RTC->CRL);
    EXPECT_EQ(0U, clear_count);
    EXPECT_EQ(0U, wait_count);
}

TEST_F(F103Standby, ExplicitDiscardDisablesWakeFirstAndReturnsOnlyAsError) {
    PWR->CSR = PWR_CSR_WUF | PWR_CSR_SBF | PWR_CSR_EWUP;
    const auto result = nx_stm32f1_standby_pa0(barrier, true);
    EXPECT_EQ(NX_ERROR_IO, result.status);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_TRUE(result.wait_issued);
    EXPECT_EQ(1U, clear_count);
    EXPECT_EQ(1U, wait_count);
    EXPECT_EQ(PWR_CSR_WUF | PWR_CSR_SBF, result.before_flags);
    EXPECT_EQ(PWR_CSR_WUF | PWR_CSR_SBF, result.after_flags);
    EXPECT_EQ(PWR_CSR_WUF | PWR_CSR_SBF | PWR_CSR_EWUP, PWR->CSR);
    EXPECT_EQ(0U, PWR->CR);
    EXPECT_EQ(0U, SCB->SCR);
    EXPECT_EQ(0U, nx_arch_irq_masks().primask);
}

TEST_F(F103Standby, LateWakeAfterRearmSkipsWfiAndPreservesNewEvidence) {
    late_wake = true;
    const auto result = nx_stm32f1_standby_pa0(barrier, false);
    EXPECT_EQ(NX_ERROR_BUSY, result.status);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_FALSE(result.wait_issued);
    EXPECT_EQ(0U, clear_count);
    EXPECT_EQ(0U, wait_count);
    EXPECT_EQ(PWR_CSR_WUF, result.after_flags);
    EXPECT_EQ(PWR_CSR_WUF, PWR->CSR);
    EXPECT_EQ(0U, SCB->SCR);
    EXPECT_EQ(0U, PWR->CR);
}

TEST_F(F103Standby, UnacknowledgedWakeFlagCannotEnterStandby) {
    PWR->CSR = PWR_CSR_WUF;
    stuck_wuf = true;
    const auto result = nx_stm32f1_standby_pa0(barrier, true);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.status);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(PWR_CSR_WUF, result.after_flags);
    EXPECT_EQ(1U, clear_count);
    EXPECT_EQ(0U, wait_count);
}

TEST_F(F103Standby, DrainedAdmissionAndPendingSystemChecksHaveNoEffects) {
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_reopen(barrier));
    nx_clock_lease_t lease{};
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_acquire(&domain, &lease));
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_standby_pa0(barrier, false).status);
    ASSERT_EQ(NX_SUCCESS, nx_clock_domain_release(&domain, &lease));
    SCB->ICSR = SCB_ICSR_PENDSTSET_Msk;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_standby_pa0(barrier, false).status);
    EXPECT_EQ(SCB_ICSR_PENDSTSET_Msk, SCB->ICSR);
    EXPECT_EQ(0U, clear_count);
    EXPECT_EQ(0U, wait_count);
}
TEST_F(F103Standby, PublicAttemptJoinsPlatformBeforeOwningGates) {
    RCC->APB1ENR = RCC->APB2ENR = 0U;
    stop_result = NX_ERROR_BUSY;
    auto attempt = nx_stm32f103_standby_enter(barrier, false);
    EXPECT_EQ(NX_ERROR_BUSY, attempt.status);
    EXPECT_FALSE(attempt.platform_stopped);
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    EXPECT_EQ(0U, wait_count);

    stop_result = NX_SUCCESS;
    gpio_gate_stuck = true;
    attempt = nx_stm32f103_standby_enter(barrier, false);
    EXPECT_EQ(NX_ERROR_IO, attempt.status);
    EXPECT_TRUE(attempt.platform_stopped);
    EXPECT_EQ(NX_SUCCESS, attempt.cleanup);
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
    EXPECT_EQ(2U, stop_requests);
}

TEST_F(F103Standby, PublicBootKeepsEvidenceAndRefusesForeignPowerGate) {
    RCC->APB1ENR = RCC->APB2ENR = 0U;
    const auto started = nx_stm32f1_platform_system_start(&rb);
    ASSERT_EQ(NX_SUCCESS, started.primary);
    PWR->CSR = PWR_CSR_SBF | PWR_CSR_EWUP;
    nx_stm32f103_standby_boot_t evidence{};
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_standby_boot(&evidence));
    EXPECT_EQ(nx_reset_cause(), evidence.reset_causes);
    EXPECT_EQ(NX_STM32F103_STANDBY_ENTERED, evidence.standby_flags);
    EXPECT_EQ(PWR_CSR_SBF | PWR_CSR_EWUP, PWR->CSR);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_PWREN);

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_standby_boot(&evidence));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_PWREN);
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_PWREN);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_release(&rb, true));
}
TEST_F(F103Standby, BootRejectsHeldPowerInterfaceResetBeforeObservation) {
    RCC->APB1ENR = RCC->APB2ENR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_start(&rb).primary);
    RCC->APB1RSTR = RCC_APB1RSTR_PWRRST;
    PWR->CSR = PWR_CSR_SBF | PWR_CSR_WUF;
    nx_stm32f103_standby_boot_t evidence{UINT32_MAX, UINT32_MAX};
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_standby_boot(&evidence));
    EXPECT_EQ(UINT32_MAX, evidence.reset_causes);
    EXPECT_EQ(UINT32_MAX, evidence.standby_flags);
    EXPECT_EQ(RCC_APB1RSTR_PWRRST, RCC->APB1RSTR);
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_PWREN);
    EXPECT_EQ(PWR_CSR_SBF | PWR_CSR_WUF, PWR->CSR);
    RCC->APB1RSTR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_platform_system_release(&rb, true));
}
} /* namespace */

extern "C" nx_result_t nx_platform_stop(void) {
    ++stop_requests;
    return stop_result;
}

bool f1_test_standby_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->APB2ENR && (value & RCC_APB2ENR_IOPAEN) != 0U &&
        gpio_gate_stuck) {
        return true;
    }
    if (reg == &PWR->CSR) {
        const uint32_t enabled = value & PWR_CSR_EWUP;
        *reg = (*reg & ~static_cast<uint32_t>(PWR_CSR_EWUP)) | enabled;
        if (enabled != 0U && late_wake) {
            *reg = *reg | PWR_CSR_WUF;
        }
        return true;
    }
    if (reg == &PWR->CR && (value & PWR_CR_CWUF) != 0U) {
        EXPECT_EQ(0U, PWR->CSR & PWR_CSR_EWUP);
        EXPECT_EQ(0U, value & PWR_CR_CSBF);
        ++clear_count;
        if (!stuck_wuf) {
            PWR->CSR = PWR->CSR & ~static_cast<uint32_t>(PWR_CSR_WUF);
        }
        *reg = value & ~static_cast<uint32_t>(PWR_CR_CWUF);
        return true;
    }
    return false;
}

void f1_test_standby_wait(void) {
    EXPECT_EQ(1U, nx_arch_irq_masks().primask);
    EXPECT_NE(0U, PWR->CR & PWR_CR_PDDS);
    EXPECT_NE(0U, SCB->SCR & SCB_SCR_SLEEPDEEP_Msk);
    EXPECT_NE(0U, PWR->CSR & PWR_CSR_EWUP);
    EXPECT_EQ(0U, PWR->CSR & PWR_CSR_WUF);
    EXPECT_NE(0U, RCC->AHBENR & RCC_AHBENR_FLITFEN);
    ++wait_count;
    PWR->CSR = PWR->CSR | PWR_CSR_WUF;
}
