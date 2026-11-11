/**
 * \file            stm32_exti_hardware_test.cpp
 * \brief           Real EMR ownership without a vector or event-ring producer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_exti_hardware.h"
nx_result_t nx_stm32_exti_hardware_admit(const nx_stm32_exti_hardware_state_t*,
                                         const nx_stm32_exti_hardware_config_t*)
    __attribute__((weak));
nx_result_t
nx_stm32_exti_hardware_prepare(nx_stm32_exti_hardware_state_t*,
                               const nx_stm32_exti_hardware_config_t*)
    __attribute__((weak));
nx_result_t nx_stm32_exti_hardware_close(nx_stm32_exti_hardware_state_t*)
    __attribute__((weak));
extern const nx_exti_ops_t nx_stm32_exti_hardware_ops __attribute__((weak));
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
bool privileged;
bool handler;
bool masked;
volatile uint32_t* refused;
bool refuse_pending;
unsigned writes;

class ExtiHardware : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32_exti_hardware_state_t state{};
    nx_stm32_exti_hardware_config_t config{GPIOB, NX_EXTI_BOTH, 1U, 5U, 1U};
    const nx_exti_port_t face{&nx_stm32_exti_hardware_ops, &state};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        privileged = true;
        handler = masked = refuse_pending = false;
        refused = nullptr;
        writes = 0U;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
        RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
        SYSCFG->EXTICR[1] = 2U << 4U;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
    void Start() {
        ASSERT_NE(nullptr, &nx_stm32_exti_hardware_admit);
        ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
        ASSERT_NE(nullptr, &nx_stm32_exti_hardware_close);
        ASSERT_NE(nullptr, &nx_stm32_exti_hardware_ops);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_hardware_admit(&state, &config));
        ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_hardware_prepare(&state, &config));
        ASSERT_TRUE(state.initialized);
    }
};
} /* namespace */

TEST_F(ExtiHardware, ArmAndCloseOnlyOwnTheSelectedEventAndMuxNibble) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    EXTI->IMR = 1U << 6U;
    EXTI->EMR = 1U << 7U;
    EXTI->RTSR = EXTI->FTSR = (1U << 6U) | (1U << 7U);
    GPIOB->MODER = 3U << 10U;
    GPIOB->PUPDR = 2U << 12U;
    Start();
    ASSERT_TRUE(state.initialized);
    EXPECT_TRUE(state.owned);
    EXPECT_EQ(&config, state.config);
    EXPECT_EQ(2U, state.saved_mux);
    EXPECT_EQ((1U << 5U) | (1U << 7U), EXTI->EMR);
    EXPECT_EQ(1U << 6U, EXTI->IMR);
    EXPECT_EQ(0U, GPIOB->MODER & (3U << 10U));
    EXPECT_EQ((1U << 10U) | (2U << 12U), GPIOB->PUPDR);
    EXPECT_EQ(1U << 4U, SYSCFG->EXTICR[1]);
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(nullptr, state.config);
    EXPECT_EQ(1U << 7U, EXTI->EMR);
    EXPECT_EQ(1U << 6U, EXTI->IMR);
    EXPECT_EQ((1U << 6U) | (1U << 7U), EXTI->RTSR);
    EXPECT_EQ((1U << 6U) | (1U << 7U), EXTI->FTSR);
    EXPECT_EQ(2U << 4U, SYSCFG->EXTICR[1]);
}

TEST_F(ExtiHardware, AllSixteenLinesSelectTheirOwnMuxAndEdgesWithoutVectors) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    for (uint8_t line = 0U; line < 16U; ++line) {
        for (const auto edge :
             {NX_EXTI_RISING, NX_EXTI_FALLING, NX_EXTI_BOTH}) {
            config.line = line;
            config.edge = edge;
            Start();
            ASSERT_TRUE(state.initialized);
            const uint32_t bit = 1U << line;
            EXPECT_EQ(bit, EXTI->EMR);
            EXPECT_EQ(0U, EXTI->IMR);
            EXPECT_EQ((edge & NX_EXTI_RISING) ? bit : 0U, EXTI->RTSR);
            EXPECT_EQ((edge & NX_EXTI_FALLING) ? bit : 0U, EXTI->FTSR);
            EXPECT_EQ(1U,
                      (SYSCFG->EXTICR[line / 4U] >> ((line % 4U) * 4U)) & 15U);
            ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
        }
    }
}

TEST_F(ExtiHardware, AHardwarePulseProvidesNoSoftwareQueueOrWakePublisher) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    Start();
    ASSERT_TRUE(state.initialized);
    nx_exti_event_t event{};
    size_t count = 7U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_exti_port_read(&face, &event, 1U, &count));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_exti_port_attach_wake(&face, nullptr, 0U));
    EXPECT_EQ(nullptr, nx_stm32_exti_hardware_ops.read);
    EXPECT_EQ(nullptr, nx_stm32_exti_hardware_ops.attach_wake);
    bool high = false;
    GPIOB->IDR = 1U << 5U;
    const auto before = writes;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_level(&face, &high));
    EXPECT_TRUE(high);
    EXPECT_EQ(before, writes);
}

TEST_F(ExtiHardware, RejectsEveryForeignSourceBeforeAnyWriteOrReference) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    volatile uint32_t* sources[]{&EXTI->IMR, &EXTI->EMR, &EXTI->PR, &EXTI->RTSR,
                                 &EXTI->FTSR};
    for (auto* source : sources) {
        *source = 1U << 5U;
        EXPECT_EQ(NX_ERROR_BUSY,
                  nx_stm32_exti_hardware_prepare(&state, &config));
        EXPECT_EQ(0U, writes);
        EXPECT_FALSE(state.owned);
        EXPECT_EQ(nullptr, state.config);
        *source = 0U;
    }
}

TEST_F(ExtiHardware, RejectsInvalidFactsAndContextWithoutChangingHardware) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    const auto original = config;
    config.line = 16U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_exti_hardware_prepare(&state, &config));
    config = original;
    config.gpio_index = 2U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_exti_hardware_prepare(&state, &config));
    config = original;
    config.pull = 3U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_exti_hardware_prepare(&state, &config));
    config = original;
    for (bool* context : {&handler, &masked}) {
        *context = true;
        EXPECT_EQ(NX_ERROR_CONTEXT,
                  nx_stm32_exti_hardware_prepare(&state, &config));
        *context = false;
    }
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_stm32_exti_hardware_prepare(&state, &config));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(0U, EXTI->EMR);
}

TEST_F(ExtiHardware, InaccessibleSharedGatesAreNotAcquiredByTheLine) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    RCC->APB2ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_exti_hardware_prepare(&state, &config));
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    RCC->AHB1ENR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_exti_hardware_prepare(&state, &config));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(state.owned);
}

TEST_F(ExtiHardware, PartialArmFailuresRetainTheSameMuxUntilObservedClose) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    volatile uint32_t* registers[]{&SYSCFG->EXTICR[1], &EXTI->RTSR, &EXTI->FTSR,
                                   &EXTI->EMR};
    for (auto* reg : registers) {
        refused = reg;
        EXPECT_EQ(NX_ERROR_IO, nx_stm32_exti_hardware_prepare(&state, &config));
        EXPECT_TRUE(state.owned);
        EXPECT_FALSE(state.initialized);
        EXPECT_EQ(&config, state.config);
        EXPECT_EQ(NX_ERROR_BUSY,
                  nx_stm32_exti_hardware_prepare(&state, &config));
        refused = nullptr;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_hardware_close(&state));
        EXPECT_EQ(2U << 4U, SYSCFG->EXTICR[1]);
        EXPECT_FALSE(state.owned);
    }
}

TEST_F(ExtiHardware,
       FailedEventTriggerOrMuxWithdrawalKeepsRetryResponsibility) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    volatile uint32_t* registers[]{&EXTI->EMR, &EXTI->RTSR, &SYSCFG->EXTICR[1]};
    for (auto* reg : registers) {
        Start();
        ASSERT_TRUE(state.initialized);
        refused = reg;
        EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
        EXPECT_TRUE(state.owned);
        EXPECT_EQ(&config, state.config);
        refused = nullptr;
        ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
        EXPECT_FALSE(state.owned);
    }
}

TEST_F(ExtiHardware, PendingWithdrawalAndForeignInterruptNeverRevokeOwnership) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    Start();
    ASSERT_TRUE(state.initialized);
    EXTI->PR = 1U << 5U;
    refuse_pending = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.owned);
    refuse_pending = false;
    EXTI->IMR = 1U << 5U;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_EQ(1U << 5U, EXTI->IMR);
    EXPECT_TRUE(state.owned);
    EXTI->IMR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}

TEST_F(ExtiHardware, LevelRejectsAliasesAndEndsOnlyAfterSuccessfulClose) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    Start();
    ASSERT_TRUE(state.initialized);
    for (void* output :
         {static_cast<void*>(&state), static_cast<void*>(&config),
          static_cast<void*>(GPIOB), static_cast<void*>(EXTI),
          static_cast<void*>(SYSCFG), static_cast<void*>(RCC)}) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_exti_port_level(&face, static_cast<bool*>(output)));
    }
    bool high = false;
    GPIOB->IDR = 1U << 5U;
    refused = &EXTI->EMR;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_level(&face, &high));
    EXPECT_TRUE(high);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    high = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_exti_port_level(&face, &high));
    EXPECT_FALSE(high);
}

TEST_F(ExtiHardware, CrystalLseExcludesPc13ButExplicitBypassDoesNot) {
    ASSERT_NE(nullptr, &nx_stm32_exti_hardware_prepare);
    config.gpio = GPIOC;
    config.gpio_index = 2U;
    config.line = 13U;
    for (uint32_t lse : {uint32_t{RCC_BDCR_LSEON}, uint32_t{RCC_BDCR_LSERDY}}) {
        RCC->BDCR = lse;
        EXPECT_EQ(NX_ERROR_BUSY,
                  nx_stm32_exti_hardware_prepare(&state, &config));
        EXPECT_FALSE(state.owned);
        EXPECT_EQ(0U, writes);
    }
    RCC->BDCR = RCC_BDCR_LSEBYP | RCC_BDCR_LSEON | RCC_BDCR_LSERDY;
    Start();
    ASSERT_TRUE(state.initialized);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}

/** \brief Model an observable register write, including explicit refusal. */
extern "C" void nx_exti_hardware_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    ++writes;
    if (reg != refused)
        *reg = value;
}

/** \brief Apply W1C without replacing any production control algorithm. */
extern "C" void nx_exti_hardware_model_clear(volatile uint32_t* reg,
                                             uint32_t bits) {
    ++writes;
    if (!refuse_pending)
        *reg &= ~bits;
}

/** \brief Model the selected caller's privilege without changing it. */
extern "C" bool nx_exti_hardware_privileged(void) {
    return privileged;
}

/** \brief Observe the modeled execution context of the serialized owner. */
extern "C" bool nx_exti_hardware_in_isr(void) {
    return handler;
}

/** \brief Observe incoming exclusion before any cold hardware effect. */
extern "C" bool nx_exti_hardware_masked(void) {
    return masked;
}
