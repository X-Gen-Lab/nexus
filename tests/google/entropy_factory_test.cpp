/**
 * \file            entropy_factory_test.cpp
 * \brief           Actual generated RNG factory and whole Platform lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_clock.h"
#include "stm32f407_entropy.h"
#include "stm32f407_provider.h"
RNG_TypeDef nx_entropy_model_rng;
RCC_TypeDef nx_entropy_model_rcc;
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <deque>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class EntropySystemBoundary {
  public:
    MOCK_METHOD(void, Start, ());
    MOCK_METHOD(void, Stop, ());
};
static EntropySystemBoundary* boundary;
static FLASH_TypeDef flash_registers;
static std::deque<uint32_t> words;
static bool block_disable;
static bool invalid_clock;
static nx_time_us_t model_now;
static unsigned data_reads;

class EntropyFactory : public testing::Test {
  protected:
    testing::NiceMock<EntropySystemBoundary> system;
    void SetUp() override {
        boundary = &system;
        nx_entropy_model_rng = {};
        nx_entropy_model_rcc = {};
        g_nx_stm32_system = {};
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.flash = &flash_registers;
        block_disable = false;
        invalid_clock = false;
        data_reads = 0U;
        model_now = 100U;
        words.clear();
    }
    void TearDown() override {
        block_disable = false;
        EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
        boundary = nullptr;
    }
    const nx_entropy_port_t* Port() {
        return nx_factory_entropy(NX_ENTROPY_ID_RANDOM);
    }
    void Start() {
        auto result = nx_platform_start();
        ASSERT_EQ(result.primary, NX_SUCCESS);
        ASSERT_EQ(result.cleanup, NX_SUCCESS);
        ASSERT_EQ(result.remaining_effects, 0U);
    }
};

TEST_F(EntropyFactory, GetterIsPureStableAndExposesOnlyTheSelectedDirectFace) {
    const auto* port = Port();
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(Port(), port);
    EXPECT_EQ(port->ops, &nx_stm32f407_entropy_ops);
    EXPECT_NE(port->context, nullptr);
    EXPECT_EQ(nx_factory_entropy(NX_ENTROPY_ID_COUNT), nullptr);
    EXPECT_EQ(RCC->AHB2ENR, 0U);
    EXPECT_EQ(RNG->CR, 0U);
    uint32_t output = 99U;
    EXPECT_EQ(nx_entropy_port_try_read(port, &output), NX_ERROR_STATE);
    EXPECT_EQ(output, 99U);
    EXPECT_EQ(data_reads, 0U);
    Start();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_NE(RNG->CR & RNG_CR_RNGEN, 0U);
}

TEST_F(EntropyFactory, ForeignRngRejectsBeforeSystemClockEffects) {
    EXPECT_CALL(system, Start()).Times(0);
    EXPECT_CALL(system, Stop()).Times(0);
    for (uint32_t foreign : {(uint32_t)RNG_CR_RNGEN, (uint32_t)RNG_CR_IE}) {
        RCC->AHB2ENR = RCC_AHB2ENR_RNGEN;
        RNG->CR = foreign;
        auto result = nx_platform_start();
        EXPECT_EQ(result.primary, NX_ERROR_BUSY);
        EXPECT_EQ(result.cleanup, NX_SUCCESS);
        EXPECT_EQ(result.remaining_effects, 0U);
        EXPECT_EQ(RCC->CR, 0U);
        EXPECT_EQ(RCC->AHB2ENR, (uint32_t)RCC_AHB2ENR_RNGEN);
        EXPECT_EQ(RNG->CR, foreign);
    }
    RNG->CR = 0U;
    RCC->AHB2ENR = 0U;
}

TEST_F(EntropyFactory, PlatformStartRejectsDuplicateAndPreservesTheFace) {
    EXPECT_CALL(system, Start()).Times(1);
    EXPECT_CALL(system, Stop()).Times(1);
    const auto* port = Port();
    Start();
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_NE(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(Port(), port);
    words = {1U, 2U};
    uint32_t output = 99U;
    EXPECT_EQ(nx_entropy_port_try_read(port, &output), NX_ERROR_BUSY);
    EXPECT_EQ(nx_entropy_port_try_read(port, &output), NX_SUCCESS);
    EXPECT_EQ(output, 2U);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_entropy_port_try_read(port, &output), NX_ERROR_STATE);
    EXPECT_EQ(Port(), port);
}

TEST_F(EntropyFactory, ActualClockMismatchRejectsBeforeRngEffects) {
    invalid_clock = true;
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    EXPECT_EQ(RCC->AHB2ENR & RCC_AHB2ENR_RNGEN, 0U);
    EXPECT_EQ(RNG->CR, 0U);
    uint32_t output = 99U;
    EXPECT_EQ(nx_entropy_port_try_read(Port(), &output), NX_ERROR_STATE);
    EXPECT_EQ(output, 99U);
    invalid_clock = false;
    Start();
}

TEST_F(EntropyFactory, FailedPlatformStopRetainsTheOwnerUntilRetry) {
    Start();
    ASSERT_FALSE(HasFatalFailure());
    block_disable = true;
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_NE(RCC->AHB2ENR & RCC_AHB2ENR_RNGEN, 0U);
    EXPECT_NE(nx_platform_start().primary, NX_SUCCESS);
    uint32_t output = 99U;
    EXPECT_EQ(nx_entropy_port_try_read(Port(), &output), NX_ERROR_STATE);
    EXPECT_EQ(output, 99U);
    block_disable = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(RCC->AHB2ENR & RCC_AHB2ENR_RNGEN, 0U);
    Start();
}

TEST_F(EntropyFactory, SuspendAndResumeUseTheSameOwnerAndTheNewCoreEpoch) {
    EXPECT_CALL(system, Start()).Times(1);
    EXPECT_CALL(system, Stop()).Times(1);
    Start();
    ASSERT_FALSE(HasFatalFailure());
    const auto* port = Port();
    auto* state = static_cast<nx_stm32f407_entropy_state_t*>(port->context);
    uint64_t old_epoch = state->time_epoch;
    EXPECT_EQ(nx_platform_suspend(), NX_SUCCESS);
    EXPECT_EQ(RNG->CR & RNG_CR_RNGEN, 0U);
    uint32_t output = 99U;
    EXPECT_EQ(nx_entropy_port_try_read(port, &output), NX_ERROR_STATE);
    ++g_nx_stm32_system.time_epoch;
    EXPECT_EQ(nx_platform_resume().primary, NX_SUCCESS);
    EXPECT_EQ(Port(), port);
    EXPECT_GT(state->time_epoch, old_epoch);
    words = {7U, 8U};
    EXPECT_EQ(nx_entropy_port_try_read(port, &output), NX_ERROR_BUSY);
    EXPECT_EQ(nx_entropy_port_try_read(port, &output), NX_SUCCESS);
    EXPECT_EQ(output, 8U);
}

extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++model_now;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    boundary->Start();
    g_nx_stm32_system.started = true;
    g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
    g_nx_stm32_system.timer_error = NX_SUCCESS;
    ++g_nx_stm32_system.time_epoch;
    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
    RCC->CFGR = NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
    flash_registers.ACR = NEXUS_FLASH_WAIT_STATES;
    if (invalid_clock)
        RCC->CR &= ~(uint32_t)RCC_CR_PLLRDY;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    EXPECT_EQ(RNG->CR & RNG_CR_RNGEN, 0U);
    boundary->Stop();
    g_nx_stm32_system.started = false;
    return 0;
}
extern "C" uint32_t nx_entropy_model_read(const volatile uint32_t* address) {
    if (address == &RNG->SR && !words.empty())
        RNG->SR |= RNG_SR_DRDY;
    if (address == &RNG->DR) {
        ++data_reads;
        uint32_t value = words.empty() ? RNG->DR : words.front();
        if (!words.empty())
            words.pop_front();
        RNG->SR &= ~(uint32_t)RNG_SR_DRDY;
        return value;
    }
    return *address;
}
extern "C" void nx_entropy_model_write(volatile uint32_t* address,
                                       uint32_t value) {
    if (address == &RNG->SR)
        *address &= value;
    else if (address == &RNG->CR && block_disable &&
             (value & RNG_CR_RNGEN) == 0U)
        *address = value | RNG_CR_RNGEN;
    else
        *address = value;
}
