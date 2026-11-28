/**
 * \file            pwm_test.cpp
 * \brief           Production F103 PWM cold ownership and wire sequencing.
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
constexpr uint32_t pwm_pin = UINT32_C(1) << 6U;
constexpr uint32_t pin_mask = GPIO_CRL_MODE6 | GPIO_CRL_CNF6;
constexpr uint32_t idle_pin = GPIO_CRL_MODE6_1;
constexpr uint32_t alternate_pin = GPIO_CRL_MODE6_1 | GPIO_CRL_CNF6_1;
constexpr uint32_t original_pads = 0x44644444U;
constexpr uint32_t original_latch = pwm_pin | (1U << 5U);
uint32_t active_compare;
unsigned updates;
unsigned update_delay_reads;
unsigned pending_update_reads;
bool system_live;
bool refuse_compare;
bool refuse_pin_change;
bool refuse_reset;

bool wire_high() {
    if ((GPIOA->CRL & pin_mask) == alternate_pin) {
        return (TIM3->CCER & TIM_CCER_CC1E) != 0U && TIM3->CNT < active_compare;
    }
    return (GPIOA->ODR & pwm_pin) != 0U;
}

void tick() {
    if ((TIM3->CR1 & TIM_CR1_CEN) == 0U) {
        return;
    }
    ++TIM3->CNT;
    if (TIM3->CNT > TIM3->ARR) {
        TIM3->CNT = 0U;
        active_compare = TIM3->CCR1;
    }
}

class F1Pwm : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    nx_stm32f1_pwm_t owner{};
    const nx_stm32f1_pwm_config_t config{TIM3,     GPIOA, 8000000U, 10U, 3U,
                                         1000000U, 0U,    6U,       1U};
    const nx_pwm_port_t port{&nx_stm32f1_pwm_ops, &owner};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(peripheral_base), peripheral_bytes,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(peripheral_base));
        GPIOA->CRL = original_pads;
        GPIOA->ODR = original_latch;
        active_compare = updates = 0U;
        update_delay_reads = pending_update_reads = 0U;
        system_live = true;
        refuse_compare = refuse_pin_change = refuse_reset = false;
    }

    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, peripheral_bytes), 0);
        }
    }
};

TEST_F(F1Pwm, FixedPeriodStagesDutyOnUpdateAndReturnsBank) {
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
    ASSERT_EQ(nx_stm32f1_pwm_prepare(&owner, &config), NX_SUCCESS);
    EXPECT_EQ(GPIOA->CRL & pin_mask, idle_pin);
    EXPECT_FALSE(wire_high());
    EXPECT_EQ(TIM3->PSC, 7U);
    EXPECT_EQ(TIM3->ARR, 9U);
    EXPECT_EQ(TIM3->DIER, 0U);
    ASSERT_EQ(nx_pwm_port_start(&port), NX_SUCCESS);
    EXPECT_EQ(GPIOA->CRL & pin_mask, alternate_pin);
    EXPECT_FALSE(wire_high());
    tick();
    EXPECT_TRUE(wire_high());
    EXPECT_EQ(nx_pwm_port_set(&port, 11U, 2U), NX_ERROR_INVALID);
    ASSERT_EQ(nx_pwm_port_set(&port, 10U, 0U), NX_SUCCESS);
    EXPECT_EQ(active_compare, 3U);
    EXPECT_TRUE(wire_high());
    for (unsigned i = 0U; i < 10U; ++i) {
        tick();
    }
    EXPECT_FALSE(wire_high());
    ASSERT_EQ(nx_pwm_port_set(&port, 10U, 10U), NX_SUCCESS);
    for (unsigned i = 0U; i < 10U; ++i) {
        tick();
    }
    EXPECT_TRUE(wire_high());
    nx_pwm_state_t state{};
    ASSERT_EQ(nx_pwm_port_state(&port, &state), NX_SUCCESS);
    EXPECT_EQ(state.duty_ticks, 10U);
    EXPECT_TRUE(state.running);
    ASSERT_EQ(nx_pwm_port_stop(&port), NX_SUCCESS);
    EXPECT_FALSE(wire_high());
    EXPECT_EQ(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(TIM3->CCER & TIM_CCER_CC1E, 0U);
    ASSERT_EQ(nx_stm32f1_pwm_close(&owner), NX_SUCCESS);
    EXPECT_EQ(GPIOA->CRL, original_pads);
    EXPECT_EQ(GPIOA->ODR, original_latch);
    EXPECT_EQ(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(TIM3->CCMR1, 0U);
}

TEST_F(F1Pwm, RefusesRemapAndInvalidClockBeforeChangingTheOwner) {
    AFIO->MAPR = AFIO_MAPR_TIM3_REMAP_PARTIALREMAP;
    EXPECT_EQ(nx_stm32f1_pwm_prepare(&owner, &config), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(GPIOA->CRL, original_pads);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    ASSERT_EQ(nx_stm32f1_pwm_close(&owner), NX_SUCCESS);
    AFIO->MAPR = 0U;
    RCC->CFGR = RCC_CFGR_PPRE1_DIV2;
    EXPECT_EQ(nx_stm32f1_pwm_prepare(&owner, &config), NX_ERROR_UNSUPPORTED);
    RCC->CFGR = 0U;
    auto wrong = config;
    wrong.tick_hz = 3000000U;
    EXPECT_EQ(nx_stm32f1_pwm_prepare(&owner, &wrong), NX_ERROR_INVALID);
    wrong = config;
    wrong.period_ticks = 1U;
    wrong.duty_ticks = 1U;
    EXPECT_EQ(nx_stm32f1_pwm_prepare(&owner, &wrong), NX_ERROR_INVALID);
    wrong = config;
    wrong.period_ticks = 65536U;
    EXPECT_EQ(nx_stm32f1_pwm_prepare(&owner, &wrong), NX_ERROR_INVALID);
    EXPECT_EQ(updates, 0U);
    EXPECT_EQ(GPIOA->CRL, original_pads);
}

TEST_F(F1Pwm, FaultRetainsHardwareUntilStopAndBankResetAreVerified) {
    ASSERT_EQ(nx_stm32f1_pwm_prepare(&owner, &config), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_start(&port), NX_SUCCESS);
    refuse_compare = true;
    EXPECT_EQ(nx_pwm_port_set(&port, 10U, 8U), NX_ERROR_IO);
    nx_pwm_state_t view{};
    EXPECT_EQ(nx_pwm_port_state(&port, &view), NX_ERROR_STATE);
    refuse_compare = false;
    tick();
    EXPECT_TRUE(wire_high());
    refuse_pin_change = true;
    EXPECT_EQ(nx_stm32f1_pwm_close(&owner), NX_ERROR_IO);
    EXPECT_NE(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_TIM3EN, 0U);
    refuse_pin_change = false;
    refuse_reset = true;
    EXPECT_EQ(nx_stm32f1_pwm_close(&owner), NX_ERROR_IO);
    EXPECT_EQ(TIM3->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_FALSE(wire_high());
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_TIM3EN, 0U);
    refuse_reset = false;
    ASSERT_EQ(nx_stm32f1_pwm_close(&owner), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    EXPECT_EQ(GPIOA->CRL, original_pads);
}

TEST_F(F1Pwm, MaximumPeriodCanStageARepresentableConstantHigh) {
    auto maximum = config;
    maximum.period_ticks = UINT16_MAX;
    maximum.duty_ticks = UINT16_MAX;
    ASSERT_EQ(nx_stm32f1_pwm_prepare(&owner, &maximum), NX_SUCCESS);
    ASSERT_EQ(nx_pwm_port_start(&port), NX_SUCCESS);
    EXPECT_EQ(TIM3->ARR, UINT16_MAX - 1U);
    EXPECT_EQ(TIM3->CCR1, UINT16_MAX);
    EXPECT_TRUE(wire_high());
    tick();
    EXPECT_TRUE(wire_high());
    ASSERT_EQ(nx_stm32f1_pwm_close(&owner), NX_SUCCESS);
}

TEST_F(F1Pwm, AcceptsDelayedTimerUpdateBeforeEnablingTheWire) {
    ASSERT_EQ(nx_stm32f1_pwm_prepare(&owner, &config), NX_SUCCESS);
    update_delay_reads = 3U;
    ASSERT_EQ(nx_pwm_port_start(&port), NX_SUCCESS);
    EXPECT_EQ(pending_update_reads, 0U);
    EXPECT_EQ(active_compare, 3U);
    EXPECT_FALSE(wire_high());
    tick();
    EXPECT_TRUE(wire_high());
    ASSERT_EQ(nx_stm32f1_pwm_close(&owner), NX_SUCCESS);
}
} /* namespace */

extern "C" bool nx_stm32f1_system_live(void) {
    return system_live;
}

extern "C" void nx_pwm_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &TIM3->CCR1 && refuse_compare) {
        return;
    }
    if (reg == &GPIOA->CRL && refuse_pin_change) {
        return;
    }
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_TIM3RST) != 0U) {
        if (refuse_reset) {
            return;
        }
        std::memset(TIM3, 0, sizeof(*TIM3));
        TIM3->ARR = UINT16_MAX;
    }
    if (reg == &TIM3->SR) {
        *reg &= value;
        return;
    }
    *reg = value;
    if (reg == &TIM3->EGR && (value & TIM_EGR_UG) != 0U) {
        ++updates;
        pending_update_reads = update_delay_reads;
        if (pending_update_reads == 0U) {
            active_compare = TIM3->CCR1;
            TIM3->CNT = 0U;
            TIM3->SR |= TIM_SR_UIF;
        }
    }
}

extern "C" uint32_t nx_pwm_model_read(volatile const uint32_t* reg) {
    if (reg == &TIM3->SR && pending_update_reads != 0U &&
        --pending_update_reads == 0U) {
        active_compare = TIM3->CCR1;
        TIM3->CNT = 0U;
        TIM3->SR |= TIM_SR_UIF;
    }
    return *reg;
}

extern "C" void nx_pwm_model_bsrr(GPIO_TypeDef* reg, uint32_t value) {
    reg->ODR = (reg->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
}
