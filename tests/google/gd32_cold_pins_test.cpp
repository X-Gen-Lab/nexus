/**
 * \file            gd32_cold_pins_test.cpp
 * \brief           Real C cold pin setup, rollback and retained bank contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_cold_pins.h"
#include "nexus/arch/arch.h"
NVIC_Type g_gd32_model_nvic;
}
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
bool in_isr;
bool masked;
bool privileged;
volatile uint32_t* refused;
uint32_t refused_bits;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* writes;

class Gd32ColdPins : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        observer;
    std::array<nx_gd32_cold_pin_config_t, 2> pins{{
        {GPIOB, 6U, 4U, 2U, 1U, true},
        {GPIOC, 7U, 4U, 2U, 1U, true},
    }};
    std::array<uint16_t, 2> saved{};
    nx_gd32_cold_pins_receipt_t receipt{};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        in_isr = false;
        masked = false;
        privileged = true;
        refused = nullptr;
        refused_bits = 0U;
        writes = &observer;
    }

    void TearDown() override {
        writes = nullptr;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }

    nx_result_t Prepare() {
        return nx_gd32_cold_pins_prepare(pins.data(), pins.size(), saved.data(),
                                         &receipt);
    }

    nx_result_t Rollback() {
        return nx_gd32_cold_pins_rollback(pins.data(), pins.size(),
                                          saved.data(), &receipt);
    }
};
} /* namespace */

/** \brief Inject only external MMIO refusal; execute the real C algorithm. */
extern "C" void nx_gd32_cold_model_write(volatile uint32_t* reg,
                                         uint32_t value) {
    if (writes != nullptr)
        writes->Call(reg, value);
    if (reg != refused || ((value ^ *reg) & refused_bits) == 0U)
        *reg = value;
}
/** \brief Model CPU context independently of the production transaction. */
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
/** \brief Observe the incoming interrupt mask without changing it. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
/** \brief Observe privilege before cold hardware acquisition. */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/** \brief Host ordering is not physical bus timing. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief The model does not execute a physical instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}

TEST_F(Gd32ColdPins, CompleteValidationPrecedesEveryHardwareEffect) {
    pins[1].pin = 16U;
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(Prepare(), NX_ERROR_INVALID);
    EXPECT_EQ(RCU_AHB1EN, 0U);
    EXPECT_EQ(GPIO_CTL(GPIOB), 0U);
    EXPECT_EQ(receipt.captured, 0U);
}

TEST_F(Gd32ColdPins, DuplicatePhysicalPadsAreRejectedBeforeClockAcquisition) {
    pins[1] = pins[0];
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(Prepare(), NX_ERROR_INVALID);
    EXPECT_EQ(RCU_AHB1EN, 0U);
}

TEST_F(Gd32ColdPins, ContextRejectionPreservesHardwareAndIncomingMasks) {
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    in_isr = true;
    EXPECT_EQ(Prepare(), NX_ERROR_CONTEXT);
    in_isr = false;
    masked = true;
    EXPECT_EQ(Prepare(), NX_ERROR_CONTEXT);
    EXPECT_TRUE(masked);
    masked = false;
    privileged = false;
    EXPECT_EQ(Prepare(), NX_ERROR_CONTEXT);
    EXPECT_EQ(RCU_AHB1EN, 0U);
}

TEST_F(Gd32ColdPins, PrepareAndRollbackPreserveUnrelatedPinsAndClockOwners) {
    RCU_AHB1EN = 1U << 8U;
    GPIO_PUD(GPIOB) = 2U << 12U;
    GPIO_CTL(GPIOB) = 1U << 2U;
    const uint16_t original = nx_gd32_pin_capture(GPIOB, 6U);
    ASSERT_EQ(Prepare(), NX_SUCCESS);
    EXPECT_EQ(receipt.captured, 3U);
    EXPECT_EQ((GPIO_CTL(GPIOB) >> 12U) & 3U, 2U);
    EXPECT_NE(GPIO_OMODE(GPIOB) & (1U << 6U), 0U);
    EXPECT_EQ((GPIO_AFSEL0(GPIOB) >> 24U) & 15U, 4U);
    ASSERT_EQ(Rollback(), NX_SUCCESS);
    EXPECT_EQ(nx_gd32_pin_capture(GPIOB, 6U), original);
    EXPECT_EQ(GPIO_CTL(GPIOB), 1U << 2U);
    EXPECT_EQ(RCU_AHB1EN, 1U << 8U);
    EXPECT_EQ(receipt.captured, 0U);
}

TEST_F(Gd32ColdPins, PartialClockFailureRollsBackOnlyAcquiredPads) {
    refused = &RCU_AHB1EN;
    refused_bits = 1U << 2U;
    EXPECT_EQ(Prepare(), NX_ERROR_IO);
    EXPECT_EQ(receipt.captured, 1U);
    ASSERT_EQ(Rollback(), NX_SUCCESS);
    EXPECT_EQ(GPIO_CTL(GPIOB), 0U);
    EXPECT_EQ(RCU_AHB1EN, 0U);
}

TEST_F(Gd32ColdPins, FailedWithdrawalRetainsBankForExplicitCleanupRetry) {
    ASSERT_EQ(Prepare(), NX_SUCCESS);
    refused = &GPIO_CTL(GPIOB);
    refused_bits = 3U << 12U;
    EXPECT_EQ(Rollback(), NX_ERROR_IO);
    EXPECT_EQ(receipt.captured, 1U);
    EXPECT_EQ(RCU_AHB1EN, 1U << 1U);
    refused = nullptr;
    ASSERT_EQ(Rollback(), NX_SUCCESS);
    EXPECT_EQ(receipt.captured, 0U);
    EXPECT_EQ(RCU_AHB1EN, 0U);
}

TEST_F(Gd32ColdPins, ReleaseWithdrawsPadsButLeavesPlatformClockLeaseIntact) {
    ASSERT_EQ(Prepare(), NX_SUCCESS);
    const uint32_t clocks = RCU_AHB1EN;
    ASSERT_EQ(nx_gd32_cold_pins_release(pins.data(), pins.size()), NX_SUCCESS);
    EXPECT_EQ(GPIO_CTL(GPIOB) & (3U << 12U), 0U);
    EXPECT_EQ(GPIO_CTL(GPIOC) & (3U << 14U), 0U);
    EXPECT_EQ(RCU_AHB1EN, clocks);
}

TEST_F(Gd32ColdPins, ReleaseFailureDoesNotPretendLaterPadsWereWithdrawn) {
    ASSERT_EQ(Prepare(), NX_SUCCESS);
    refused = &GPIO_CTL(GPIOB);
    refused_bits = 3U << 12U;
    EXPECT_EQ(nx_gd32_cold_pins_release(pins.data(), pins.size()), NX_ERROR_IO);
    EXPECT_EQ((GPIO_CTL(GPIOC) >> 14U) & 3U, 2U);
    refused = nullptr;
    EXPECT_EQ(nx_gd32_cold_pins_release(pins.data(), pins.size()), NX_SUCCESS);
}
