/**
 * \file            entropy_profile_test.cpp
 * \brief           Actual RNG admission for HSE26, HSI and input SYSCLK
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_clock.h"
#include "stm32f407_entropy.h"
#include "stm32f407_system.h"
RNG_TypeDef nx_entropy_model_rng;
RCC_TypeDef nx_entropy_model_rcc;
nx_stm32_system_t g_nx_stm32_system;
}
#include <cstring>
#include <gtest/gtest.h>

class EntropyProfile : public testing::Test {
  protected:
    nx_stm32f407_entropy_state_t state{};
    FLASH_TypeDef flash{};
    const nx_entropy_port_t port{&nx_stm32f407_entropy_ops, &state};
    void SetUp() override {
        std::memset(&nx_entropy_model_rcc, 0, sizeof(nx_entropy_model_rcc));
        std::memset(&nx_entropy_model_rng, 0, sizeof(nx_entropy_model_rng));
        nx_entropy_model_rcc.CR =
            RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos) |
            RCC_CR_PLLON | RCC_CR_PLLRDY |
            (NEXUS_CLOCK_SOURCE_HSE ? RCC_CR_HSEON | RCC_CR_HSERDY : 0U);
        nx_entropy_model_rcc.PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        nx_entropy_model_rcc.CFGR =
            NEXUS_CLOCK_CFGR | NEXUS_CLOCK_SW | (NEXUS_CLOCK_SW << 2U);
        flash.ACR = NEXUS_FLASH_WAIT_STATES;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &nx_entropy_model_rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
        g_nx_stm32_system.time_epoch = 1U;
        g_nx_stm32_system.started = true;
    }
    void TearDown() override {
        EXPECT_EQ(nx_stm32f407_entropy_stop(&state), NX_SUCCESS);
    }
};

TEST_F(EntropyProfile, SelectedSourceAndActualRunStateProvideNominalPll48) {
    ASSERT_TRUE(nx_stm32_clock_profile_matches(&nx_entropy_model_rcc, &flash));
    ASSERT_EQ(nx_stm32f407_entropy_start(&state), NX_SUCCESS);
    uint32_t output = 99U;
    nx_entropy_model_rng.DR = 17U;
    nx_entropy_model_rng.SR = RNG_SR_DRDY;
    ASSERT_EQ(nx_entropy_port_try_read(&port, &output), NX_ERROR_BUSY);
    EXPECT_EQ(output, 99U);
    nx_entropy_model_rng.DR = 23U;
    nx_entropy_model_rng.SR = RNG_SR_DRDY;
    EXPECT_EQ(nx_entropy_port_try_read(&port, &output), NX_SUCCESS);
    EXPECT_EQ(output, 23U);
}

TEST_F(EntropyProfile, UnavailablePll48RejectsWithoutImplicitClockOwnership) {
    nx_entropy_model_rcc.CR &= ~(uint32_t)(RCC_CR_PLLON | RCC_CR_PLLRDY);
    EXPECT_EQ(nx_stm32f407_entropy_start(&state), NX_ERROR_STATE);
    EXPECT_EQ(nx_entropy_model_rcc.AHB2ENR, 0U);
    EXPECT_EQ(nx_entropy_model_rng.CR, 0U);
    EXPECT_FALSE(state.gate_owned);
    EXPECT_FALSE(state.rng_owned);
}

/** \brief Model only RNG_DR consume semantics, not provider clock admission.
 */
extern "C" uint32_t nx_entropy_model_read(const volatile uint32_t* address) {
    uint32_t value = *address;
    if (address == &nx_entropy_model_rng.DR)
        nx_entropy_model_rng.SR &= ~(uint32_t)RNG_SR_DRDY;
    return value;
}
/** \brief Model actual LL writes and the RNG status write-zero clear rule. */
extern "C" void nx_entropy_model_write(volatile uint32_t* address,
                                       uint32_t value) {
    if (address == &nx_entropy_model_rng.SR)
        *address &= value;
    else
        *address = value;
}
