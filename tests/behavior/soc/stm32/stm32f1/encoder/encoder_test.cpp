/**
 * \file            encoder_test.cpp
 * \brief           Exercise F103 TIM3 encoder configuration and withdrawal.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "instance.h"
#include "system/platform.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t peripheral_base = 0x40000000U;
constexpr size_t peripheral_bytes = 0x24000U;
constexpr uintptr_t system_base = 0xE000E000U;
constexpr size_t system_bytes = 0x1000U;
constexpr uint32_t original_pads = 0x82114444U;
constexpr uint32_t original_latch = 0xE0U;
constexpr uint32_t original_mapr = 0x02000000U;
constexpr uint32_t phase_enables = TIM_CCER_CC1E | TIM_CCER_CC2E;
constexpr uint32_t vector = 1U << static_cast<uint32_t>(TIM3_IRQn);
constexpr nx_time_us_t observed_time = 100U;
volatile uint32_t* refused;
unsigned mapr_writes;
bool system_live;

class F103Encoder : public testing::Test {
  protected:
    nx_stm32f1_encoder_t owner{};
    const nx_stm32f1_encoder_config_t config{3U, 14U, false, true};
    const nx_encoder_t port{&nx_stm32f1_encoder_ops, &owner};
    void* peripheral = MAP_FAILED;
    void* core = MAP_FAILED;

    void SetUp() override {
        const int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE;
        peripheral =
            mmap(reinterpret_cast<void*>(peripheral_base), peripheral_bytes,
                 PROT_READ | PROT_WRITE, flags, -1, 0);
        ASSERT_NE(peripheral, MAP_FAILED);
        core = mmap(reinterpret_cast<void*>(system_base), system_bytes,
                    PROT_READ | PROT_WRITE, flags, -1, 0);
        ASSERT_NE(core, MAP_FAILED);
        GPIOA->CRL = original_pads;
        GPIOA->ODR = original_latch;
        AFIO->MAPR = original_mapr;
        RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
        refused = nullptr;
        mapr_writes = 0U;
        system_live = true;
    }

    void TearDown() override {
        if (core != MAP_FAILED) {
            EXPECT_EQ(munmap(core, system_bytes), 0);
        }
        if (peripheral != MAP_FAILED) {
            EXPECT_EQ(munmap(peripheral, peripheral_bytes), 0);
        }
    }
};

TEST_F(F103Encoder, AppliesInputOnlyQuadratureAndDescribesBothFilters) {
    ASSERT_EQ(nx_stm32f1_encoder_prepare(&owner, &config), NX_SUCCESS);
    EXPECT_EQ(GPIOA->CRL, 0x44114444U);
    EXPECT_EQ(GPIOA->ODR, original_latch);
    EXPECT_EQ(AFIO->MAPR, original_mapr);
    EXPECT_EQ(mapr_writes, 0U);
    EXPECT_EQ(TIM3->CR1, 0U);
    EXPECT_EQ(TIM3->DIER, 0U);
    EXPECT_EQ(TIM3->SMCR, 3U);
    EXPECT_EQ(TIM3->CCMR1, 0xE131U);
    EXPECT_EQ(TIM3->CCER, TIM_CCER_CC2P);
    EXPECT_EQ(TIM3->ARR, UINT16_MAX);
    nx_encoder_state_t state{};
    ASSERT_EQ(nx_encoder_state(&port, &state), NX_SUCCESS);
    EXPECT_EQ(state.mode, NX_ENCODER_MODE_X4);
    EXPECT_EQ(state.counter_bits, 16U);
    EXPECT_EQ(state.filter_clock_hz, 8000000U);
    EXPECT_EQ(state.filter_a.clock_divisor, 1U);
    EXPECT_EQ(state.filter_a.consecutive_samples, 8U);
    EXPECT_EQ(state.filter_b.clock_divisor, 32U);
    EXPECT_EQ(state.filter_b.consecutive_samples, 6U);
    EXPECT_FALSE(state.running);
}

TEST_F(F103Encoder, StopsAndRestartsWithoutChangingTheModuloPosition) {
    ASSERT_EQ(nx_stm32f1_encoder_prepare(&owner, &config), NX_SUCCESS);
    ASSERT_EQ(nx_encoder_start(&port), NX_SUCCESS);
    EXPECT_EQ(TIM3->CCER & phase_enables, phase_enables);
    TIM3->CNT = UINT16_MAX;
    nx_encoder_sample_t sample{};
    ASSERT_EQ(nx_encoder_read(&port, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.count, UINT16_MAX);
    EXPECT_EQ(sample.observed_at, observed_time);
    TIM3->CNT = 0U;
    ASSERT_EQ(nx_encoder_read(&port, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.count, 0U);
    TIM3->CNT = 39U;
    ASSERT_EQ(nx_encoder_stop(&port), NX_SUCCESS);
    EXPECT_EQ(TIM3->CCER & phase_enables, 0U);
    EXPECT_EQ(TIM3->CR1 & TIM_CR1_CEN, 0U);
    ASSERT_EQ(nx_encoder_start(&port), NX_SUCCESS);
    ASSERT_EQ(nx_encoder_read(&port, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.count, 39U);
    ASSERT_EQ(nx_stm32f1_encoder_close(&owner), NX_SUCCESS);
    EXPECT_EQ(GPIOA->CRL, original_pads);
    EXPECT_EQ(GPIOA->ODR, original_latch);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
}

TEST_F(F103Encoder, RejectsForeignRemapVectorClockAndControllerWithoutWrites) {
    AFIO->MAPR = original_mapr | AFIO_MAPR_TIM3_REMAP_FULLREMAP;
    ASSERT_EQ(nx_stm32f1_encoder_prepare(&owner, &config),
              NX_ERROR_UNSUPPORTED);
    ASSERT_EQ(nx_stm32f1_encoder_close(&owner), NX_SUCCESS);
    ASSERT_EQ(AFIO->MAPR, original_mapr | AFIO_MAPR_TIM3_REMAP_FULLREMAP);
    EXPECT_EQ(mapr_writes, 0U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    AFIO->MAPR = original_mapr;
    NVIC->ISPR[0] = vector;
    EXPECT_EQ(nx_stm32f1_encoder_prepare(&owner, &config), NX_ERROR_BUSY);
    NVIC->ISPR[0] = 0U;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    EXPECT_EQ(nx_stm32f1_encoder_prepare(&owner, &config), NX_ERROR_BUSY);
    RCC->APB1ENR &= ~static_cast<uint32_t>(RCC_APB1ENR_TIM3EN);
    RCC->CFGR = RCC_CFGR_PPRE1_DIV2;
    EXPECT_EQ(nx_stm32f1_encoder_prepare(&owner, &config),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(GPIOA->CRL, original_pads);
    EXPECT_EQ(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
}

TEST_F(F103Encoder, FailedWithdrawalRetainsClockAndPadsUntilCloseRetry) {
    ASSERT_EQ(nx_stm32f1_encoder_prepare(&owner, &config), NX_SUCCESS);
    ASSERT_EQ(nx_encoder_start(&port), NX_SUCCESS);
    refused = &TIM3->CR1;
    EXPECT_EQ(nx_encoder_stop(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_stm32f1_encoder_close(&owner), NX_ERROR_IO);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_TIM3EN, 0U);
    EXPECT_EQ(GPIOA->CRL, 0x44114444U);
    refused = nullptr;
    ASSERT_EQ(nx_stm32f1_encoder_close(&owner), NX_SUCCESS);
    EXPECT_EQ(GPIOA->CRL, original_pads);
}

TEST_F(F103Encoder, FailingColdWriteCanBeUnwoundAndAllowsReprepare) {
    refused = &TIM3->CCMR1;
    ASSERT_EQ(nx_stm32f1_encoder_prepare(&owner, &config), NX_ERROR_IO);
    EXPECT_EQ(nx_encoder_start(&port), NX_ERROR_STATE);
    refused = nullptr;
    ASSERT_EQ(nx_stm32f1_encoder_close(&owner), NX_SUCCESS);
    EXPECT_EQ(GPIOA->CRL, original_pads);
    ASSERT_EQ(nx_stm32f1_encoder_prepare(&owner, &config), NX_SUCCESS);
}

TEST_F(F103Encoder, FailedColdResetStillAllowsTheOwnedSourcesToBeWithdrawn) {
    TIM3->DIER = TIM_DIER_UIE;
    refused = &RCC->APB1RSTR;
    ASSERT_EQ(nx_stm32f1_encoder_prepare(&owner, &config), NX_ERROR_IO);
    refused = &TIM3->DIER;
    EXPECT_EQ(nx_stm32f1_encoder_close(&owner), NX_ERROR_IO);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_TIM3EN, 0U);
    refused = nullptr;
    ASSERT_EQ(nx_stm32f1_encoder_close(&owner), NX_SUCCESS);
    EXPECT_EQ(TIM3->DIER, 0U);
    EXPECT_EQ(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
    EXPECT_EQ(GPIOA->CRL, original_pads);
}
} /* namespace */

extern "C" bool nx_stm32f1_system_live(void) {
    return system_live;
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    return observed_time;
}

extern "C" void nx_encoder_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &AFIO->MAPR) {
        ++mapr_writes;
    }
    if (reg == refused) {
        return;
    }
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_TIM3RST) != 0U) {
        std::memset(TIM3, 0, sizeof(*TIM3));
        TIM3->ARR = UINT16_MAX;
    }
    *reg = value;
}

extern "C" void nx_encoder_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    reg->ODR = (reg->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
}
