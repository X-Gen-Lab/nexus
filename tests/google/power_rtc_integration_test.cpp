/**
 * \file            power_rtc_integration_test.cpp
 * \brief           Real RTC, System, power and static Core suspension together
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "clock_domain_impl.h"
#include "nexus/core/fault.h"
#include "nexus/core/platform_lifecycle.h"
#include "nexus/soc/stm32f407_power.h"
#include "power_rtc_integration_model.h"
#include <cstring>
#include <gtest/gtest.h>
#include <stdexcept>

extern "C" nx_stm32_system_t g_nx_stm32_system;
uint32_t nx_power_rtc_scr;
uint32_t nx_power_rtc_icsr;
uint32_t nx_power_rtc_tick;
uint32_t nx_power_rtc_enabled[3];
uint32_t nx_power_rtc_active[3];

namespace {
nx_stm32_rtc_state_t rtc_state;
nx_stm32_rtc_config_t rtc_config;
const nx_rtc_port_t rtc_port{&nx_stm32_rtc_ops, &rtc_state};
bool refuse_restore;
unsigned waits;
uint64_t prior_epoch;

nx_result_t context() {
    return nx_f407_rtc_privileged() && !nx_f407_rtc_masked() &&
                   !nx_f407_rtc_in_isr()
               ? NX_SUCCESS
               : NX_ERROR_CONTEXT;
}
nx_result_t admit(size_t index) {
    return index == 0U ? NX_SUCCESS : NX_ERROR_INVALID;
}
nx_platform_start_result_t start_system() {
    auto result = nx_stm32_system_start(&g_nx_stm32_system, 16U);
    return {result.primary == 0 ? NX_SUCCESS : NX_ERROR_IO,
            result.cleanup == 0 ? NX_SUCCESS : NX_ERROR_IO,
            result.remaining_effects};
}
nx_result_t empty_resources() {
    return NX_SUCCESS;
}
nx_result_t release_system(bool) {
    return nx_stm32_system_stop(&g_nx_stm32_system, 16U) == 0 ? NX_SUCCESS
                                                              : NX_ERROR_IO;
}
nx_lifecycle_acquisition_t acquire(size_t) {
    nx_result_t result = nx_stm32_rtc_prepare(&rtc_state, &rtc_config);
    return {result, result == NX_SUCCESS || nx_stm32_rtc_effects(&rtc_state)};
}
nx_result_t release(size_t) {
    return nx_stm32_rtc_close(&rtc_state);
}
nx_result_t suspend(size_t) {
    return nx_stm32_rtc_suspend(&rtc_state);
}
nx_lifecycle_acquisition_t resume(size_t) {
    nx_result_t result = nx_stm32_rtc_resume(&rtc_state);
    return {result, result == NX_SUCCESS};
}
nx_result_t resume_resources() {
    return g_nx_stm32_system.started &&
                   nx_stm32_clock_profile_matches(g_nx_stm32_system.rcc,
                                                  g_nx_stm32_system.flash)
               ? NX_SUCCESS
               : NX_ERROR_STATE;
}
const size_t ordinals[] = {0U};
const nx_platform_lifecycle_plan_t plan{
    1U,           ordinals,        context,         admit,
    start_system, empty_resources, empty_resources, release_system,
    acquire,      release,         false,           suspend,
    resume,       resume_resources};

class PowerRtcIntegration : public testing::Test {
  protected:
    nx_platform_lifecycle_state_t platform{};
    nx_clock_domain_t domain{};
    nx_clock_barrier_t barrier{};
    nx_rtc_standby_t standby{};
    FLASH_TypeDef flash{};
    TIM_TypeDef timer{};
    nx_stm32_power_options_t options{};

    void SetUp() override {
        nx_f407_rtc_model_reset(false);
        auto& model = g_f407_rtc_model;
        rtc_state = {};
        rtc_config = {};
        rtc_config.clock = {nx_stm32_rtc_clock_read, &g_nx_stm32_system};
        rtc_config.policy = NX_STM32_RTC_PROVISION_EMPTY;
        rtc_config.has_initial = true;
        rtc_config.initial = {24U, 2U, 29U, 4U, 12U, 34U, 56U};
        rtc_config.timeout_us = 100U;
        rtc_config.poll_limit = 32U;
        model.rcc.CR =
            RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
        model.rcc.PLLCFGR = 0x24003010U;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &model.rcc;
        g_nx_stm32_system.power = &model.power;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.timer = &timer;
        nx_power_rtc_scr = nx_power_rtc_icsr = nx_power_rtc_tick = 0U;
        std::memset(nx_power_rtc_enabled, 0, sizeof(nx_power_rtc_enabled));
        std::memset(nx_power_rtc_active, 0, sizeof(nx_power_rtc_active));
        refuse_restore = false;
        waits = 0U;
        options.poll_limit = 16U;
        options.standby = true;
        options.wake_events = NX_STM32_WAKE_RTC_TIMER;
        ASSERT_EQ(NX_SUCCESS, nx_clock_domain_construct(&domain, 1U));
    }
    void Prepare() {
        ASSERT_EQ(
            NX_SUCCESS,
            nx_platform_lifecycle_start(&plan, &platform, nullptr).primary);
        nx_rtc_wakeup_t wake{true, NX_RTC_WAKE_DIV16, 1U};
        ASSERT_EQ(NX_SUCCESS, nx_rtc_port_wakeup(&rtc_port, &wake));
        ASSERT_EQ(NX_SUCCESS,
                  nx_rtc_port_wake_route(&rtc_port, NX_RTC_EVENT_WAKEUP));
        ASSERT_EQ(NX_SUCCESS,
                  nx_platform_lifecycle_suspend(&plan, &platform, nullptr));
        ASSERT_EQ(NX_SUCCESS,
                  nx_rtc_port_standby_prepare(&rtc_port, 0U, 32U, &standby));
        ASSERT_EQ(NX_SUCCESS, nx_clock_domain_close(&domain, &barrier));
        prior_epoch = g_nx_stm32_system.time_epoch;
        timer.CNT = 123U;
    }
    void TearDown() override {
        g_f407_rtc_model.hold_write = false;
        g_f407_rtc_model.masked = false;
        if (standby.owner != nullptr) {
            EXPECT_EQ(NX_SUCCESS,
                      nx_rtc_port_standby_restore(&rtc_port, &standby));
        }
        if (platform.suspend_phase == NX_PLATFORM_SUSPENDED) {
            EXPECT_EQ(NX_SUCCESS,
                      nx_platform_lifecycle_resume(&plan, &platform, nullptr)
                          .primary);
        }
        EXPECT_EQ(NX_SUCCESS, nx_rtc_port_stop(&rtc_port));
        if (rtc_state.effects != 0U) {
            nx_rtc_backup_handoff_t handoff{};
            EXPECT_EQ(NX_SUCCESS,
                      nx_rtc_port_backup_handoff(&rtc_port, &handoff));
        }
        EXPECT_EQ(NX_SUCCESS,
                  nx_platform_lifecycle_stop(&plan, &platform, nullptr));
    }
};
} /* namespace */

TEST_F(PowerRtcIntegration, UnselectedDurationWakeupAddsNoHardwareBehavior) {
    ASSERT_EQ(NX_SUCCESS,
              nx_platform_lifecycle_start(&plan, &platform, nullptr).primary);
    nx_rtc_period_t actual{123U, 456U};
    const auto writes = g_f407_rtc_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_rtc_port_periodic_wakeup(&rtc_port, 1000000U, 0U, &actual));
    EXPECT_EQ(writes, g_f407_rtc_model.writes);
    EXPECT_EQ(123U, actual.ticks);
    EXPECT_EQ(456U, actual.ticks_per_second);
}
TEST_F(PowerRtcIntegration, StandbyReceiptCrossesRealSystemStopAndColdResume) {
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32f407_standby_enter(barrier, &options, &rtc_port, &standby,
                                         &receipt));
    EXPECT_EQ(1U, waits);
    EXPECT_EQ(nullptr, standby.owner);
    EXPECT_EQ(prior_epoch, receipt.old_epoch);
    EXPECT_GT(receipt.new_epoch, receipt.old_epoch);
    EXPECT_TRUE(receipt.waited);
    EXPECT_EQ(123U, receipt.before_us);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_NE(0U, g_f407_rtc_model.rcc.APB1ENR & RCC_APB1ENR_PWREN);
    EXPECT_NE(0U, g_f407_rtc_model.power.CR & PWR_CR_DBP);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_WUTF);
    EXPECT_NE(0U, g_f407_rtc_model.exti.PR & NX_STM32_WAKE_RTC_TIMER);
    ASSERT_EQ(NX_SUCCESS,
              nx_platform_lifecycle_resume(&plan, &platform, nullptr).primary);
    nx_rtc_snapshot_t snapshot{};
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_read(&rtc_port, &snapshot));
}

TEST_F(PowerRtcIntegration, FailedRtcRestoreRetainsActualOwnerAfterNewEpoch) {
    Prepare();
    ASSERT_FALSE(HasFatalFailure());
    refuse_restore = true;
    nx_stm32_stop_receipt_t receipt{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_stm32f407_standby_enter(barrier, &options, &rtc_port, &standby,
                                         &receipt));
    EXPECT_EQ(1U, waits);
    EXPECT_NE(nullptr, standby.owner);
    EXPECT_TRUE(g_nx_stm32_system.started);
    EXPECT_GT(receipt.new_epoch, receipt.old_epoch);
    g_f407_rtc_model.hold_write = false;
    EXPECT_EQ(NX_SUCCESS, nx_rtc_port_standby_restore(&rtc_port, &standby));
    EXPECT_EQ(nullptr, standby.owner);
    EXPECT_NE(0U, g_f407_rtc_model.rtc.ISR & RTC_ISR_WUTF);
}

/** \brief Model oscillator-ready and mux status as external hardware facts.
 */
extern "C" void nx_power_rtc_poll(void* raw) {
    auto* system = static_cast<nx_stm32_system_t*>(raw);
    RCC_TypeDef* rcc = system->rcc;
    if ((rcc->CR & RCC_CR_HSION) != 0U)
        rcc->CR |= RCC_CR_HSIRDY;
    rcc->CR = (rcc->CR & ~(uint32_t)(RCC_CR_HSERDY | RCC_CR_PLLRDY)) |
              ((rcc->CR & RCC_CR_HSEON) != 0U ? RCC_CR_HSERDY : 0U) |
              ((rcc->CR & RCC_CR_PLLON) != 0U ? RCC_CR_PLLRDY : 0U);
    rcc->CFGR = (rcc->CFGR & ~(uint32_t)RCC_CFGR_SWS) |
                ((rcc->CFGR & (uint32_t)RCC_CFGR_SW) << 2U);
}
/** \brief Model timer update/W0C and PWR CWUF, leaving algorithms in C. */
extern "C" void nx_power_rtc_write(volatile uint32_t* reg, uint32_t value) {
    auto* timer = g_nx_stm32_system.timer;
    if (reg == &timer->SR)
        *reg &= value;
    else if (reg == &g_f407_rtc_model.power.CR && (value & PWR_CR_CWUF) != 0U) {
        *reg = value & ~(uint32_t)PWR_CR_CWUF;
        g_f407_rtc_model.power.CSR &= ~(uint32_t)PWR_CSR_WUF;
    } else
        *reg = value;
    if (reg == &timer->EGR && (value & TIM_EGR_UG) != 0U) {
        timer->SR |= TIM_SR_UIF;
        timer->CNT = 0U;
    }
}
/** \brief The timer vector occupies its real NVIC bank and bit. */
extern "C" int nx_power_rtc_timer_irq(bool enabled) {
    uint32_t bit = 1U << (static_cast<unsigned>(TIM2_IRQn) % 32U);
    uint32_t& bank =
        nx_power_rtc_enabled[static_cast<unsigned>(TIM2_IRQn) / 32U];
    bank = enabled ? bank | bit : bank & ~bit;
    return 0;
}
/** \brief An independent RTC wake retains its real pending business facts.
 */
extern "C" void nx_power_rtc_wait(bool event) {
    /* These real RTC integration cases enter Standby through WFI. */
    EXPECT_FALSE(event);
    auto& model = g_f407_rtc_model;
    EXPECT_TRUE(model.masked);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(RTC_CR_WUTE | RTC_CR_WUTIE,
              model.rtc.CR & (RTC_CR_WUTE | RTC_CR_WUTIE));
    EXPECT_EQ(0U, model.power.CSR & PWR_CSR_WUF);
    model.rtc.ISR |= RTC_ISR_WUTF;
    model.exti.PR |= NX_STM32_WAKE_RTC_TIMER;
    model.power.CSR |= PWR_CSR_WUF;
    model.hold_write = refuse_restore;
    if (refuse_restore)
        model.rtc.CR &= ~(uint32_t)RTC_CR_WUTE;
    ++waits;
}
/** \brief Save the actual modeled CPU mask shared by both C owners. */
extern "C" nx_arch_irq_state_t __wrap_nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved{g_f407_rtc_model.masked ? 1U : 0U};
    g_f407_rtc_model.masked = true;
    return saved;
}
/** \brief Restore the exact incoming mask without advancing any clock. */
extern "C" void __wrap_nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    g_f407_rtc_model.masked = saved.value != 0U;
}
/** \brief The host has no instruction pipeline requiring synchronization. */
extern "C" void __wrap_nx_arch_isb(void) {
}
/** \brief Intercept unexpected terminal faults without returning into C. */
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    throw std::runtime_error("unexpected real power/RTC terminal failure");
}

/** \brief Core and both providers observe the same modeled CPU privilege. */
extern "C" bool __wrap_nx_arch_is_privileged(void) {
    return nx_f407_rtc_privileged();
}
/** \brief Core and providers share the exact exception-context boundary. */
extern "C" bool __wrap_nx_arch_in_isr(void) {
    return nx_f407_rtc_in_isr();
}
/** \brief Mask inspection uses the same CPU as actual RTC commit. */
extern "C" bool __wrap_nx_arch_irq_is_masked(void) {
    return nx_f407_rtc_masked();
}
/** \brief Expose all three masks to independently compiled Core code. */
extern "C" nx_arch_irq_masks_t __wrap_nx_arch_irq_masks(void) {
    return nx_f407_rtc_masks();
}
/** \brief Share synchronous external-MMIO visibility with Core. */
extern "C" void __wrap_nx_arch_dsb(void) {
    nx_f407_rtc_dsb();
}
