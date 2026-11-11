/**
 * \file            stm32_silicon_identity_test.cpp
 * \brief           Real F407 observed-cut admission before Platform effects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f4_platform_lifecycle_impl.h"
nx_stm32_system_t g_nx_stm32_system{};
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class IdentityPort {
  public:
    MOCK_METHOD(uint32_t, Idcode, ());
    MOCK_METHOD(uint16_t, FlashKiB, ());
    MOCK_METHOD(nx_result_t, PinsAcquire, ());
};
IdentityPort* identity;

class SiliconIdentity : public testing::Test {
  protected:
    RCC_TypeDef rcc{};
    FLASH_TypeDef flash{};
    PWR_TypeDef power{};
    TIM_TypeDef timer{};
    testing::StrictMock<IdentityPort> hardware;

    void SetUp() override {
        identity = &hardware;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &timer;
        rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
    }
    void Read(uint32_t raw, uint16_t kib = 512U) {
        EXPECT_CALL(hardware, FlashKiB()).WillOnce(testing::Return(kib));
        EXPECT_CALL(hardware, Idcode()).WillOnce(testing::Return(raw));
    }
};
} /* namespace */

TEST_F(SiliconIdentity, AcceptsOnlyExplicitReviewedCutsForBothDensities) {
    for (uint32_t revision : {0x1001U, 0x1003U, 0x1007U, 0x100fU, 0x101fU}) {
        for (uint16_t kib : {uint16_t{512U}, uint16_t{1024U}}) {
            SCOPED_TRACE(revision);
            Read((revision << 16U) | 0x413U, kib);
            EXPECT_EQ(NX_SUCCESS, nx_stm32_validate_identity(
                                      static_cast<uint32_t>(kib) * 1024U));
        }
    }
}

TEST_F(SiliconIdentity, MissingLegacyAndUnknownCutsAreNotNewerCompatibility) {
    for (uint32_t revision : {0U, 0x1000U, 0x1002U, 0x1004U, 0x1008U, 0x1010U,
                              0x1020U, 0x2000U, 0xffffU}) {
        SCOPED_TRACE(revision);
        Read((revision << 16U) | 0x413U);
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32_validate_identity(524288U));
    }
}

TEST_F(SiliconIdentity, LegacyRevisionADeviceAndOtherDieGroupsRemainRejected) {
    for (uint32_t raw : {0x10000411U, 0x20000411U, 0x10010414U}) {
        SCOPED_TRACE(raw);
        Read(raw);
        EXPECT_EQ(NX_ERROR_IO, nx_stm32_validate_identity(524288U));
    }
}

TEST_F(SiliconIdentity, InvalidAuthoredDensityDoesNotReadSiliconOrTouchState) {
    EXPECT_CALL(hardware, Idcode()).Times(0);
    EXPECT_CALL(hardware, FlashKiB()).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_validate_identity(0U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_validate_identity(65536U));
}

TEST_F(SiliconIdentity,
       PhysicalDensityMismatchRetainsTheOriginalGroupContract) {
    EXPECT_CALL(hardware, FlashKiB()).WillOnce(testing::Return(1024U));
    EXPECT_CALL(hardware, Idcode()).Times(testing::AtMost(1));
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_validate_identity(524288U));
}

TEST_F(SiliconIdentity, UnreviewedObservedCutRejectsBeforePinsOrClockWrites) {
    Read(0x10020413U);
    EXPECT_CALL(hardware, PinsAcquire()).Times(0);
    nx_stm32_platform_config_t selected{};
    nx_stm32_platform_pin_receipt_t pins{};
    const nx_stm32f4_platform_pin_owner_t owner{
        [](const nx_stm32_platform_config_t*) { return NX_SUCCESS; },
        [](const nx_stm32_platform_config_t*,
           nx_stm32_platform_pin_receipt_t*) {
            return identity->PinsAcquire();
        },
        [](nx_stm32_platform_pin_receipt_t*) { return NX_SUCCESS; },
        [](nx_stm32_platform_pin_receipt_t*) { return NX_SUCCESS; },
        [](const nx_stm32_platform_config_t*,
           nx_stm32_platform_pin_receipt_t*) { return NX_SUCCESS; }};
    nx_stm32f4_platform_config_t config{};
    config.system = &g_nx_stm32_system;
    config.selected = &selected;
    config.pins = &pins;
    config.pin_owner = &owner;
    config.flash_bytes = 524288U;
    config.poll_limit = 8U;
    const auto original_rcc = rcc;
    const auto original_flash = flash;
    const auto original_power = power;
    const auto original_timer = timer;
    const auto original_state = g_nx_stm32_system;
    const auto result = nx_stm32f4_platform_system_start_local(&config);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0, std::memcmp(&rcc, &original_rcc, sizeof(rcc)));
    EXPECT_EQ(0, std::memcmp(&flash, &original_flash, sizeof(flash)));
    EXPECT_EQ(0, std::memcmp(&power, &original_power, sizeof(power)));
    EXPECT_EQ(0, std::memcmp(&timer, &original_timer, sizeof(timer)));
    EXPECT_EQ(0, std::memcmp(&g_nx_stm32_system, &original_state,
                             sizeof(original_state)));
}

/** \brief Read the actual modeled IDCODE once at the external MMIO boundary. */
extern "C" uint32_t nx_f407_identity_model_idcode(void) {
    return identity->Idcode();
}

/** \brief Read the physical Flash-density halfword without a default. */
extern "C" uint16_t nx_f407_identity_model_flash_kib(void) {
    return identity->FlashKiB();
}
