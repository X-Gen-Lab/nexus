/**
 * \file            pvd_test.cpp
 * \brief           F103xB polling comparator ownership and fault contracts.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "register_model.h"
#include <gtest/gtest.h>

extern "C" {
#include "drivers/pvd/instance.h"
#include "nexus/arch/arch.h"
}

RCC_TypeDef nx_pvd_model_rcc;
PWR_TypeDef nx_pvd_model_pwr;
EXTI_TypeDef nx_pvd_model_exti;

namespace {
constexpr uint32_t line = 1U << 16U;
unsigned gate_writes;
unsigned pwr_writes;
unsigned unexpected_writes;
volatile uint32_t* dropped;
bool privileged;
bool isr;
bool masked;
uint32_t vector_state;

class Pvd : public testing::Test {
  protected:
    nx_stm32f1_pvd_t state{};
    nx_exti_port_t face{&nx_stm32f1_pvd_ops, &state};

    void SetUp() override {
        nx_pvd_model_rcc = {};
        nx_pvd_model_pwr = {};
        nx_pvd_model_exti = {};
        gate_writes = pwr_writes = unexpected_writes = 0U;
        dropped = nullptr;
        privileged = true;
        isr = masked = false;
        vector_state = 0U;
    }
};
} /* namespace */

extern "C" void nx_pvd_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &RCC->APB1ENR) {
        ++gate_writes;
    } else if (reg == &PWR->CR) {
        ++pwr_writes;
        EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_PWREN);
        EXPECT_EQ(0U, value & (PWR_CR_CSBF | PWR_CR_CWUF));
    } else {
        ++unexpected_writes;
    }
    if (reg != dropped) {
        *reg = value;
    }
}

extern "C" uint32_t nx_pvd_model_enabled(void) {
    return vector_state & 1U;
}
extern "C" uint32_t nx_pvd_model_pending(void) {
    return vector_state & 2U;
}
extern "C" uint32_t nx_pvd_model_active(void) {
    return vector_state & 4U;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
extern "C" void nx_arch_dsb(void) {
}

TEST_F(Pvd, RawCodesPreserveInactiveThresholdAndUnrelatedGate) {
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
    PWR->CR = PWR_CR_PLS_LEV2;
    EXTI->IMR = 1U << 13U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_pvd_admit(&state, 7U));
    EXPECT_EQ(0U, gate_writes);
    for (uint8_t code = 0U; code < 8U; ++code) {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pvd_prepare(&state, code));
        EXPECT_EQ(static_cast<uint32_t>(code) << PWR_CR_PLS_Pos,
                  PWR->CR & PWR_CR_PLS);
        EXPECT_NE(0U, PWR->CR & PWR_CR_PVDE);
        bool high = true;
        PWR->CSR = 0U;
        EXPECT_EQ(NX_SUCCESS, nx_exti_port_level(&face, &high));
        EXPECT_FALSE(high);
        isr = true;
        PWR->CSR = PWR_CSR_PVDO;
        EXPECT_EQ(NX_SUCCESS, nx_exti_port_level(&face, &high));
        EXPECT_TRUE(high);
        isr = false;
        EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
        EXPECT_EQ(PWR_CR_PLS_LEV2, PWR->CR);
        EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
        EXPECT_EQ(1U << 13U, EXTI->IMR);
        EXPECT_EQ(NX_ERROR_STATE, nx_exti_port_level(&face, &high));
        EXPECT_TRUE(high);
    }
    EXPECT_EQ(0U, unexpected_writes);
}

TEST_F(Pvd, RefusesForeignGateLineOrVectorWithoutWrites) {
    RCC->APB1ENR = RCC_APB1ENR_PWREN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_pvd_prepare(&state, 0U));
    RCC->APB1ENR = 0U;
    volatile uint32_t* fields[] = {&EXTI->IMR,  &EXTI->EMR,   &EXTI->RTSR,
                                   &EXTI->FTSR, &EXTI->SWIER, &EXTI->PR};
    for (auto* field : fields) {
        *field = line;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_pvd_prepare(&state, 0U));
        *field = 0U;
    }
    for (auto value : {1U, 2U, 4U}) {
        vector_state = value;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_pvd_prepare(&state, 0U));
    }
    EXPECT_EQ(0U, gate_writes);
    EXPECT_EQ(0U, pwr_writes);
}

TEST_F(Pvd, RejectsForeignPwrControlOrFlagsAndRestoresProbeGate) {
    constexpr uint32_t controls[] = {PWR_CR_PVDE, PWR_CR_DBP, PWR_CR_PDDS,
                                     PWR_CR_LPDS};
    for (uint32_t bit : controls) {
        PWR->CR = bit;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_pvd_prepare(&state, 1U));
        EXPECT_EQ(bit, PWR->CR);
        EXPECT_EQ(0U, RCC->APB1ENR);
        EXPECT_EQ(0U, state.effects);
    }
    PWR->CR = 0U;
    constexpr uint32_t flags[] = {PWR_CSR_WUF, PWR_CSR_SBF, PWR_CSR_EWUP};
    for (uint32_t flag : flags) {
        PWR->CSR = flag;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_pvd_prepare(&state, 1U));
        EXPECT_EQ(flag, PWR->CSR);
        EXPECT_EQ(0U, RCC->APB1ENR);
    }
    EXPECT_EQ(0U, pwr_writes);
    EXPECT_EQ(0U, unexpected_writes);
}

TEST_F(Pvd, FailedStartOrStopRetainsOwnershipForRetry) {
    PWR->CR = PWR_CR_PLS_LEV3;
    dropped = &PWR->CR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_pvd_prepare(&state, 6U));
    EXPECT_NE(0U, state.effects);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pvd_close(&state));
    dropped = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pvd_prepare(&state, 6U));
    dropped = &PWR->CR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_pvd_close(&state));
    EXPECT_NE(0U, state.effects);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_PWREN);
    bool high = true;
    EXPECT_EQ(NX_ERROR_STATE, nx_exti_port_level(&face, &high));
    EXPECT_TRUE(high);
    dropped = &RCC->APB1ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_pvd_close(&state));
    EXPECT_EQ(PWR_CR_PLS_LEV3, PWR->CR);
    EXPECT_NE(0U, state.effects);
    dropped = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_pvd_close(&state));
    EXPECT_EQ(0U, state.effects);
    EXPECT_EQ(0U, RCC->APB1ENR);
}

TEST_F(Pvd, WithdrawingNeverConsumesAForeignLineOrWakeFlag) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pvd_prepare(&state, 4U));
    EXTI->PR = line | (1U << 13U);
    PWR->CSR = PWR_CSR_WUF;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_pvd_close(&state));
    EXPECT_NE(0U, PWR->CR & PWR_CR_PVDE);
    EXPECT_EQ(line | (1U << 13U), EXTI->PR);
    EXTI->PR &= ~line;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_pvd_close(&state));
    EXPECT_EQ(PWR_CSR_WUF, PWR->CSR);
    EXPECT_EQ(1U << 13U, EXTI->PR);
    EXPECT_EQ(0U, unexpected_writes);
}

TEST_F(Pvd, ProvidesLevelOnlyAndRejectsInvalidContextOrStorage) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_pvd_prepare(&state, 8U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32f1_pvd_prepare(nullptr, 0U));
    masked = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32f1_pvd_prepare(&state, 0U));
    masked = false;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_pvd_prepare(&state, 5U));
    bool high = true;
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_exti_port_level(&face, &high));
    privileged = true;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_exti_port_level(&face, reinterpret_cast<bool*>(&state)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_exti_port_level(
                  &face, reinterpret_cast<bool*>(UINT32_C(0x40007000))));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_exti_port_read(&face, nullptr, 0U, nullptr));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_exti_port_attach_wake(&face, nullptr, 0U));
    PWR->CR = PWR_CR_PVDE | PWR_CR_PLS_LEV6;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_level(&face, &high));
    EXPECT_TRUE(high);
}
