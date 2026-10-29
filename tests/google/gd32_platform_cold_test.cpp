/**
 * \file            gd32_platform_cold_test.cpp
 * \brief           Concrete selected resource admission and retryable GPIO/DMA
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_platform_cold.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_exti.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
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

class Gd32PlatformCold : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        observer;
    std::array<uint16_t, 2> saved{{0xFFFFU, 0xFFFFU}};
    std::array<nx_gd32_platform_pin_config_t, 2> pins{{
        {GPIOB, &saved[0], 1U, 6U},
        {GPIOC, &saved[1], 2U, 7U},
    }};
    std::array<nx_gd32_platform_dma_lane_t, 1> lanes{{{DMA0, 6U}}};
    nx_gd32_platform_config_t config{};
    nx_gd32_platform_pin_receipt_t receipt{};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        g_gd32_model_nvic = {};
        in_isr = false;
        masked = false;
        privileged = true;
        refused = nullptr;
        refused_bits = 0U;
        writes = &observer;
        config.pins = pins.data();
        config.pin_count = pins.size();
        config.dma_lanes = lanes.data();
        config.dma_lane_count = lanes.size();
        config.apb2_clocks = RCU_APB2EN_USART0EN;
        config.irq_masks[1] = UINT32_C(1) << (USART0_IRQn % 32U);
        config.exti_lines = 1U << 3U;
    }

    void TearDown() override {
        writes = nullptr;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }

    nx_result_t Acquire() {
        return nx_gd32_platform_pins_acquire(&config, &receipt);
    }
    nx_result_t Release() {
        return nx_gd32_platform_pins_release(&config, &receipt);
    }
    nx_result_t Admit() {
        return nx_gd32_platform_selected_preflight(&config);
    }
};
} /* namespace */

/** \brief Observe external writes and model a refused register effect. */
extern "C" void nx_gd32_cold_model_write(volatile uint32_t* reg,
                                         uint32_t value) {
    if (writes != nullptr)
        writes->Call(reg, value);
    if (reg != refused || ((*reg ^ value) & refused_bits) == 0U)
        *reg = value;
}
/** \brief Model only the incoming hardware context. */
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
/** \brief A rejected context must preserve the incoming mask. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
/** \brief Privilege is an independent external CPU observation. */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/** \brief Host ordering does not establish physical bus timing. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief The host model does not execute the MCU pipeline. */
extern "C" void nx_arch_isb(void) {
}

TEST_F(Gd32PlatformCold, InvalidPhysicalIdentityRejectsBeforeEveryEffect) {
    pins[1].port_index = 1U;
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(Admit(), NX_ERROR_INVALID);
    EXPECT_EQ(Acquire(), NX_ERROR_INVALID);
    EXPECT_EQ(saved[0], 0xFFFFU);
    EXPECT_EQ(receipt.clocks_added, 0U);
}

TEST_F(Gd32PlatformCold, ForeignExclusiveClockRejectsWithoutChangingItsOwner) {
    RCU_APB2EN = RCU_APB2EN_USART0EN;
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(Admit(), NX_ERROR_BUSY);
    EXPECT_EQ(RCU_APB2EN, RCU_APB2EN_USART0EN);
}

TEST_F(Gd32PlatformCold, EverySelectedPublisherStateRejectsWithoutMutation) {
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    const auto mask = config.irq_masks[1];
    NVIC->ISER[1] = mask;
    EXPECT_EQ(Admit(), NX_ERROR_BUSY);
    NVIC->ISER[1] = 0U;
    NVIC->ISPR[1] = mask;
    EXPECT_EQ(Admit(), NX_ERROR_BUSY);
    NVIC->ISPR[1] = 0U;
    NVIC->IABR[1] = mask;
    EXPECT_EQ(Admit(), NX_ERROR_BUSY);
    EXPECT_EQ(NVIC->IABR[1], mask);
}

TEST_F(Gd32PlatformCold, SelectedDmaAndExtiOwnersAreObservedWithoutStores) {
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    DMA_CHCTL(DMA0, DMA_CH6) = DMA_CHXCTL_CHEN;
    EXPECT_EQ(Admit(), NX_ERROR_BUSY);
    DMA_CHCTL(DMA0, DMA_CH6) = 0U;
    EXTI_PD = config.exti_lines;
    EXPECT_EQ(Admit(), NX_ERROR_BUSY);
    EXPECT_EQ(EXTI_PD, config.exti_lines);
}

TEST_F(Gd32PlatformCold, PoweredForeignPadRefusesBeforeSnapshotsOrGateEffects) {
    RCU_AHB1EN = 1U << 1U;
    GPIO_CTL(GPIOB) = 1U << 12U;
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(Admit(), NX_ERROR_BUSY);
    EXPECT_EQ(saved[0], 0xFFFFU);
    EXPECT_EQ(RCU_AHB1EN, 1U << 1U);
}

TEST_F(Gd32PlatformCold, PoweredLockedPadRejectsWithoutChangingTheLockOwner) {
    RCU_AHB1EN = 1U << 2U;
    GPIO_LOCK(GPIOC) = 1U << 7U;
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(Admit(), NX_ERROR_BUSY);
    EXPECT_EQ(GPIO_LOCK(GPIOC), 1U << 7U);
    EXPECT_EQ(saved[1], 0xFFFFU);
}

TEST_F(Gd32PlatformCold, UnpoweredPadIsRecheckedBeforeAnySnapshotIsAccepted) {
    GPIO_CTL(GPIOC) = 2U << 14U;
    ASSERT_EQ(Admit(), NX_SUCCESS);
    EXPECT_EQ(Acquire(), NX_ERROR_BUSY);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(saved[0], 0xFFFFU);
    EXPECT_EQ(saved[1], 0xFFFFU);
    EXPECT_EQ(nx_gd32_platform_gpio_clocks_release(&receipt), NX_SUCCESS);
    EXPECT_EQ(RCU_AHB1EN, 0U);
    EXPECT_EQ(GPIO_CTL(GPIOC), 2U << 14U);
}

TEST_F(Gd32PlatformCold, ClockRefusalDoesNotCapturePartlyAdmittedPinValues) {
    refused = &RCU_AHB1EN;
    refused_bits = 1U << 2U;
    EXPECT_EQ(Acquire(), NX_ERROR_IO);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(saved[0], 0xFFFFU);
    EXPECT_EQ(receipt.clocks_added, (1U << 1U) | (1U << 2U));
    refused = nullptr;
    EXPECT_EQ(nx_gd32_platform_gpio_clocks_release(&receipt), NX_SUCCESS);
}

TEST_F(Gd32PlatformCold, WithdrawsAllDriversBeforeReleasingSharedClockOwners) {
    RCU_AHB1EN = 1U << 8U;
    GPIO_PUD(GPIOB) = 2U << 12U;
    const auto original = nx_gd32_pin_capture(GPIOB, 6U);
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    EXPECT_EQ(saved[0], original);
    GPIO_CTL(GPIOB) |= 2U << 12U;
    GPIO_CTL(GPIOC) |= 1U << 14U;
    EXPECT_CALL(observer, Call(testing::_, testing::_))
        .Times(testing::AnyNumber());
    testing::Expectation c =
        EXPECT_CALL(observer, Call(&GPIO_CTL(GPIOC), 3U << 14U));
    testing::Expectation b =
        EXPECT_CALL(observer, Call(&GPIO_CTL(GPIOB), 3U << 12U)).After(c);
    EXPECT_CALL(observer, Call(&RCU_AHB1EN, 1U << 8U)).After(b);
    EXPECT_EQ(Release(), NX_SUCCESS);
    EXPECT_EQ(nx_gd32_pin_capture(GPIOB, 6U), original);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(RCU_AHB1EN, 1U << 8U);
}

TEST_F(Gd32PlatformCold,
       RefusedWithdrawalRetainsEverySnapshotAndClockForRetry) {
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    GPIO_CTL(GPIOB) = 2U << 12U;
    refused = &GPIO_CTL(GPIOB);
    refused_bits = 3U << 12U;
    EXPECT_EQ(Release(), NX_ERROR_IO);
    EXPECT_TRUE(receipt.captured);
    EXPECT_EQ(RCU_AHB1EN, 6U);
    EXPECT_EQ(nx_gd32_platform_gpio_clocks_release(&receipt), NX_ERROR_BUSY);
    refused = nullptr;
    EXPECT_EQ(Release(), NX_SUCCESS);
    EXPECT_EQ(RCU_AHB1EN, 0U);
}

TEST_F(Gd32PlatformCold,
       RefusedGateReleaseKeepsOnlyTheOutstandingClockReceipt) {
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    refused = &RCU_AHB1EN;
    refused_bits = 6U;
    EXPECT_EQ(Release(), NX_ERROR_IO);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(receipt.clocks_added, 6U);
    refused = nullptr;
    EXPECT_EQ(Release(), NX_SUCCESS);
    EXPECT_EQ(receipt.clocks_added, 0U);
}

TEST_F(Gd32PlatformCold,
       RefusedPinRestorationRetainsItsOriginalSnapshotForRetry) {
    GPIO_PUD(GPIOB) = 2U << 12U;
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    const uint16_t original = saved[0];
    GPIO_PUD(GPIOB) = 1U << 12U;
    refused = &GPIO_PUD(GPIOB);
    refused_bits = 3U << 12U;
    EXPECT_EQ(Release(), NX_ERROR_IO);
    EXPECT_TRUE(receipt.captured);
    EXPECT_EQ(saved[0], original);
    EXPECT_EQ(GPIO_PUD(GPIOB), 1U << 12U);
    EXPECT_EQ(RCU_AHB1EN, 6U);
    refused = nullptr;
    EXPECT_EQ(Release(), NX_SUCCESS);
    EXPECT_EQ(GPIO_PUD(GPIOB), 2U << 12U);
    EXPECT_EQ(RCU_AHB1EN, 0U);
}

TEST_F(Gd32PlatformCold, FailedSystemAbortDiscardsOnlyUntouchedSnapshots) {
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    EXPECT_EQ(nx_gd32_platform_pins_abort(&receipt), NX_SUCCESS);
    EXPECT_FALSE(receipt.captured);
    EXPECT_EQ(RCU_AHB1EN, 0U);
}

TEST_F(Gd32PlatformCold, DmaClockRefusalAndWithdrawalAreExplicitlyRetryable) {
    uint32_t added = 0U;
    RCU_AHB1EN = RCU_AHB1EN_DMA0EN | (1U << 8U);
    refused = &RCU_AHB1EN;
    refused_bits = RCU_AHB1EN_DMA1EN;
    EXPECT_EQ(nx_gd32_platform_dma_acquire(
                  RCU_AHB1EN_DMA0EN | RCU_AHB1EN_DMA1EN, &added),
              NX_ERROR_IO);
    EXPECT_EQ(added, RCU_AHB1EN_DMA1EN);
    refused = nullptr;
    ASSERT_EQ(nx_gd32_platform_dma_release(&added), NX_SUCCESS);
    ASSERT_EQ(nx_gd32_platform_dma_acquire(RCU_AHB1EN_DMA1EN, &added),
              NX_SUCCESS);
    refused = &RCU_AHB1EN;
    refused_bits = RCU_AHB1EN_DMA1EN;
    EXPECT_EQ(nx_gd32_platform_dma_release(&added), NX_ERROR_IO);
    EXPECT_EQ(added, RCU_AHB1EN_DMA1EN);
    refused = nullptr;
    EXPECT_EQ(nx_gd32_platform_dma_release(&added), NX_SUCCESS);
    EXPECT_EQ(RCU_AHB1EN, RCU_AHB1EN_DMA0EN | (1U << 8U));
}

TEST_F(Gd32PlatformCold, InvalidContextLeavesHardwareAndIncomingMaskUnchanged) {
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    in_isr = true;
    EXPECT_EQ(Admit(), NX_ERROR_CONTEXT);
    in_isr = false;
    masked = true;
    EXPECT_EQ(Acquire(), NX_ERROR_CONTEXT);
    EXPECT_TRUE(masked);
    masked = false;
    privileged = false;
    uint32_t added = 0U;
    EXPECT_EQ(nx_gd32_platform_dma_acquire(RCU_AHB1EN_DMA0EN, &added),
              NX_ERROR_CONTEXT);
    EXPECT_EQ(RCU_AHB1EN, 0U);
    EXPECT_EQ(added, 0U);
}
