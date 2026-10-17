/**
 * \file            h743_flash_execution_test.cpp
 * \brief           H743 physical image and complete busy-flag boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_execution_model.h"
#include "h743_flash_fixture.h"
#include "nexus/io/flash.h"
#include "stm32h743_flash_execution.h"
#include "stm32h743_provider.h"
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>

namespace {
/* Actual development ELF bounds, expressed as physical addresses. Host Flash
 * backing storage is deliberately unrelated to these integer addresses. */
constexpr nx_stm32h743_flash_image_t image{0x08000000U, 0x08005c20U,
                                           0x08000000U, 0x08005c6cU};
class H743FlashImage : public testing::Test {
  protected:
    void SetUp() override {
        nx_h743_model_reset();
    }
    void TearDown() override {
        EXPECT_EQ(0U, g_h743_model.reads);
        EXPECT_EQ(0U, g_h743_model.writes);
    }
};
TEST_F(H743FlashImage, BankOneSpareSectorIsDisjointFromTheRunningImage) {
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32h743_flash_image_check(&image, 131072U, 131072U));
}
TEST_F(H743FlashImage, BankTwoRemainsPartOfThePhysicalDevice) {
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32h743_flash_image_check(&image, 1048576U, 1048576U));
}
TEST_F(H743FlashImage, LiveVectorAndFirstInstructionCannotBeOverwritten) {
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_image_check(&image, 0U, 32U));
}
TEST_F(H743FlashImage, ReadOnlyDependenciesBelongToTheProtectedImage) {
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32h743_flash_image_check(&image, 0x5c00U, 32U));
}
TEST_F(H743FlashImage, InitializedRamLoadBytesRemainProtectedAfterCopy) {
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32h743_flash_image_check(&image, 0x5c20U, 32U));
}
TEST_F(H743FlashImage, EraseCannotRoundAUserRangeAcrossTheLiveImage) {
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32h743_flash_image_check(&image, 0U, 131072U));
}
TEST_F(H743FlashImage, HalfOpenLoadEndDoesNotReserveTheFollowingBytes) {
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_image_check(&image, 0x5c6cU, 32U));
}
TEST_F(H743FlashImage, CrossBankSpareRangeIsNotAnImageAlias) {
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32h743_flash_image_check(&image, 0xfffe0U, 64U));
}
TEST_F(H743FlashImage, ZeroOutsideAndOverflowRangesRejectWithoutMmio) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_flash_image_check(&image, 0x20000U, 0U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_flash_image_check(&image, 0x200000U, 1U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_flash_image_check(&image, UINT32_MAX, 32U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_flash_image_check(
                  &image, 0x20000U, std::numeric_limits<size_t>::max()));
}
TEST_F(H743FlashImage, MissingInvertedOrNonFlashImageBoundsReject) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_flash_image_check(nullptr, 0x20000U, 32U));
    for (const auto& invalid : std::array<nx_stm32h743_flash_image_t, 5U>{
             {{0x08000000U, 0x08000000U, 0x08000000U, 0x08005c6cU},
              {0x08005c20U, 0x08000000U, 0x08000000U, 0x08005c6cU},
              {0x08000000U, 0x08005c20U, 0x08005c6cU, 0x08000000U},
              {0x07000000U, 0x07005c20U, 0x07000000U, 0x07005c6cU},
              {0x08000000U, 0x08005c20U, 0x08000000U, 0x08200001U}}}) {
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32h743_flash_image_check(&invalid, 0x20000U, 32U));
    }
}
TEST_F(H743FlashImage, LiveBytesCannotEscapeTheDeclaredLoadImage) {
    const nx_stm32h743_flash_image_t invalid{0x08000000U, 0x08005c20U,
                                             0x08000020U, 0x08005c6cU};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_flash_image_check(&invalid, 0x20000U, 32U));
}
TEST_F(H743FlashImage, ImageInBankTwoDoesNotImplyAnInventedBankOnePartition) {
    const nx_stm32h743_flash_image_t bank_two{0x08100000U, 0x08105c20U,
                                              0x08100000U, 0x08105c6cU};
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32h743_flash_image_check(&bank_two, 0U, 131072U));
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32h743_flash_image_check(&bank_two, 1048576U, 131072U));
}
class FlashPulseBoundary {
  public:
    MOCK_METHOD(void, Observe, (uint32_t pending, bool active, bool locked));
};
} /* namespace */

class H743FlashPulseDrain : public testing::Test {
  protected:
    std::array<uint32_t, 524288U> memory{};
    nx_stm32h743_flash_state_t state{};
    nx_flash_port_t face{&nx_stm32h743_flash_ops, &state};
    testing::StrictMock<FlashPulseBoundary> hardware;
    testing::NiceMock<nexus_test::h743_ram::RamHardwareBoundary> ram_hardware;
    alignas(1024) nx_stm32h743_flash_ram_workspace_t execution{};
    void SetUp() override {
        nx_h743_model_reset();
        memory.fill(UINT32_MAX);
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
        g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
        state.registers = &g_h743_model.flash;
        state.memory = reinterpret_cast<uint8_t*>(memory.data());
        state.registers->ACR = FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0;
        state.registers->CR1 = state.registers->CR2 = FLASH_CR_LOCK;
        state.supply_mv = 3300U;
        state.poll_limit = 8U;
        g_h743_model.physical_flash = &state;
        nexus_test::h743_fixture::ResetExternalCpu();
        nexus_test::h743_fixture::SetIsolatedTimer(64000000U);
        ASSERT_EQ(NX_SUCCESS,
                  nexus_test::h743_fixture::Prepare(&state, &execution));
        nexus_test::h743_ram::cpu.observer = &ram_hardware;
        EXPECT_CALL(ram_hardware, Poll(testing::_, true, true))
            .WillRepeatedly([&](uint32_t pending, bool active, bool) {
                hardware.Observe(pending, active,
                                 (state.registers->CR1 & FLASH_CR_LOCK) != 0U);
            });
    }
    void TearDown() override {
        nexus_test::h743_fixture::CheckedStop(&execution);
        nexus_test::h743_fixture::StopIsolatedTimer();
    }
    void ProgramWithTail(uint32_t pending) {
        nexus_test::h743_ram::cpu.pending = pending;
        nexus_test::h743_ram::cpu.clear_after = 3U;
        std::array<uint8_t, 32U> data{};
        data.fill(0x55U);
        EXPECT_CALL(hardware, Observe(pending, true, false)).Times(2);
        EXPECT_CALL(hardware, Observe(0U, true, false)).Times(1);
        EXPECT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 131072U, data.data(),
                                                    data.size(), 10000U));
        EXPECT_EQ(3U, nexus_test::h743_ram::cpu.polls);
        EXPECT_FALSE(state.active);
        EXPECT_EQ(FLASH_CR_LOCK, state.registers->CR1);
        EXPECT_EQ(8U, g_h743_model.flash_store_count);
    }
};
TEST_F(H743FlashPulseDrain, QueueWritePendingAfterBsyClearRetainsTheCaller) {
    ProgramWithTail(FLASH_SR_QW);
}
TEST_F(H743FlashPulseDrain, WriteBufferPendingAfterBsyClearRetainsTheCaller) {
    ProgramWithTail(FLASH_SR_WBNE);
}
