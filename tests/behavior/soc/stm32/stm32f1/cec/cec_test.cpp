/**
 * \file            cec_test.cpp
 * \brief           F100 CEC raw frames and safe withdrawal using actual C.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/cec/instance.h"

#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <gtest/gtest.h>

CEC_TypeDef nx_cec_registers;
AFIO_TypeDef nx_cec_afio;
GPIO_TypeDef nx_cec_gpio;
RCC_TypeDef nx_cec_rcc;
bool nx_cec_irq_enable;
bool nx_cec_irq_pending;
uint32_t nx_cec_irq_priority;

namespace {
constexpr uint32_t flags = CEC_CSR_RBTF | CEC_CSR_RERR | CEC_CSR_REOM |
                           CEC_CSR_RSOM | CEC_CSR_TBTRF | CEC_CSR_TERR;
constexpr uint32_t tx_errors = CEC_ESR_TBTFE | CEC_ESR_LINE | CEC_ESR_ACKE;
uint32_t register_writes;
uint32_t written[16];
size_t written_size;
bool defer_disable;
nx_time_us_t now;

class F1Cec : public testing::Test {
  protected:
    nx_stm32f1_cec_t state{};

    void SetUp() override {
        nx_cec_registers = {};
        nx_cec_afio = {};
        nx_cec_gpio = {};
        nx_cec_rcc = {};
        nx_cec_irq_enable = nx_cec_irq_pending = false;
        nx_cec_irq_priority = 2U;
        GPIOB->CRH = 0x44444444U;
        GPIOB->ODR = 1U << 9U;
        now = 0;
        defer_disable = false;
        register_writes = 0U;
        written_size = 0U;
    }

    void receive_byte(uint32_t status, uint8_t data) {
        CEC->RXD = data;
        CEC->CSR |= status | CEC_CSR_RBTF;
        nx_stm32f1_cec_irq(&state);
    }

    void transmit_event(uint32_t status) {
        CEC->CSR &= ~static_cast<uint32_t>(CEC_CSR_TSOM);
        CEC->CSR |= status;
        nx_stm32f1_cec_irq(&state);
    }
};

TEST_F(F1Cec, FixedRouteOwnsOnlyPb8AndRestoresInheritedConfiguration) {
    RCC->APB2ENR = RCC_APB2ENR_AFIOEN;
    AFIO->MAPR2 = AFIO_MAPR2_TIM16_REMAP;
    ASSERT_EQ(nx_stm32f1_cec_prepare(&state, 4U, 5U), NX_SUCCESS);
    EXPECT_EQ(CEC->PRES, 399U);
    EXPECT_EQ(CEC->OAR, 4U);
    EXPECT_EQ(GPIOB->CRH, 0x4444444eU);
    EXPECT_EQ(GPIOB->ODR, (1U << 8U) | (1U << 9U));
    EXPECT_EQ(nx_cec_irq_priority, 5U);
    EXPECT_TRUE(nx_cec_irq_enable);
    ASSERT_EQ(nx_stm32f1_cec_close(&state, 50U), NX_SUCCESS);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    EXPECT_EQ(RCC->APB2ENR, RCC_APB2ENR_AFIOEN);
    EXPECT_EQ(AFIO->MAPR2, AFIO_MAPR2_TIM16_REMAP);
    EXPECT_EQ(GPIOB->CRH, 0x44444444U);
    EXPECT_EQ(GPIOB->ODR, 1U << 9U);
    EXPECT_EQ(nx_cec_irq_priority, 2U);
    EXPECT_FALSE(nx_cec_irq_enable);
}

TEST_F(F1Cec, RejectsInheritedRemapAndForeignVectorBeforePinChange) {
    AFIO->MAPR2 = AFIO_MAPR2_CEC_REMAP;
    EXPECT_EQ(nx_stm32f1_cec_prepare(&state, 4U, 5U), NX_ERROR_BUSY);
    EXPECT_EQ(AFIO->MAPR2, AFIO_MAPR2_CEC_REMAP);
    EXPECT_EQ(RCC->APB2ENR, 0U);
    EXPECT_EQ(RCC->APB1ENR, 0U);
    nx_cec_irq_pending = true;
    const uint32_t writes = register_writes;
    EXPECT_EQ(nx_stm32f1_cec_prepare(&state, 4U, 5U), NX_ERROR_BUSY);
    EXPECT_EQ(register_writes, writes);
}

TEST_F(F1Cec, BuffersPayloadUntilFinalBlockAndReportsNackWithoutReplay) {
    ASSERT_EQ(nx_stm32f1_cec_prepare(&state, 4U, 5U), NX_SUCCESS);
    uint8_t payload[] = {0x8fU, 0x01U};
    ASSERT_EQ(nx_stm32f1_cec_send(&state, 0U, payload, sizeof(payload)),
              NX_SUCCESS);
    payload[0] = 0U;
    EXPECT_EQ(written[0], 0x40U);
    EXPECT_EQ(CEC->CSR, CEC_CSR_TSOM);
    transmit_event(CEC_CSR_TBTRF);
    EXPECT_EQ(written[1], 0x8fU);
    EXPECT_EQ(CEC->CSR, 0U);
    transmit_event(CEC_CSR_TBTRF);
    EXPECT_EQ(written[2], 0x01U);
    EXPECT_EQ(CEC->CSR, CEC_CSR_TEOM);
    EXPECT_EQ(nx_stm32f1_cec_transmit_status(&state), NX_ERROR_BUSY);
    transmit_event(CEC_CSR_TBTRF);
    EXPECT_EQ(nx_stm32f1_cec_transmit_status(&state), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_cec_send(&state, 0U, nullptr, 0U), NX_SUCCESS);
    EXPECT_EQ(CEC->CSR, CEC_CSR_TSOM | CEC_CSR_TEOM);
    CEC->ESR = CEC_ESR_ACKE;
    transmit_event(CEC_CSR_TERR | CEC_CSR_TBTRF);
    EXPECT_EQ(nx_stm32f1_cec_transmit_status(&state), NX_ERROR_NACK);
    EXPECT_EQ(written_size, 4U);
    EXPECT_EQ(CEC->CSR, 0U);
}

TEST_F(F1Cec, RetainsTheFirstCompleteFrameUntilRead) {
    ASSERT_EQ(nx_stm32f1_cec_prepare(&state, 4U, 5U), NX_SUCCESS);
    receive_byte(CEC_CSR_RSOM, 0x04U);
    receive_byte(CEC_CSR_REOM, 0x90U);
    receive_byte(CEC_CSR_RSOM | CEC_CSR_REOM, 0x24U);
    uint8_t frame[16]{};
    size_t bytes = 0U;
    EXPECT_EQ(nx_stm32f1_cec_receive(&state, frame, 1U, &bytes),
              NX_ERROR_OVERFLOW);
    ASSERT_EQ(nx_stm32f1_cec_receive(&state, frame, sizeof(frame), &bytes),
              NX_SUCCESS);
    ASSERT_EQ(bytes, 2U);
    EXPECT_EQ(frame[0], 0x04U);
    EXPECT_EQ(frame[1], 0x90U);
    EXPECT_EQ(nx_stm32f1_cec_receive(&state, frame, sizeof(frame), &bytes),
              NX_ERROR_EMPTY);
    receive_byte(CEC_CSR_RSOM, 0x04U);
    CEC->ESR = CEC_ESR_BTE;
    CEC->CSR |= CEC_CSR_RERR;
    nx_stm32f1_cec_irq(&state);
    EXPECT_EQ(nx_stm32f1_cec_receive(&state, frame, sizeof(frame), &bytes),
              NX_ERROR_IO);
    EXPECT_EQ(bytes, 0U);
}

TEST_F(F1Cec, TimeoutKeepsWireOwnershipUntilHardwareConfirmsDrain) {
    ASSERT_EQ(nx_stm32f1_cec_prepare(&state, 4U, 5U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_cec_send(&state, 0U, nullptr, 0U), NX_SUCCESS);
    defer_disable = true;
    EXPECT_EQ(nx_stm32f1_cec_close(&state, 10U), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(nx_cec_irq_enable);
    EXPECT_NE(CEC->CFGR & CEC_CFGR_PE, 0U);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_CECEN, 0U);
    EXPECT_EQ(nx_stm32f1_cec_send(&state, 0U, nullptr, 0U), NX_ERROR_STATE);
    defer_disable = false;
    EXPECT_EQ(nx_stm32f1_cec_close(&state, 30U), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f1_cec_transmit_status(&state), NX_ERROR_CANCELLED);
    EXPECT_FALSE(nx_cec_irq_enable);
    EXPECT_EQ(RCC->APB1ENR, 0U);
}
} /* namespace */

extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++now;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}

extern "C" void nx_cec_model_write(volatile uint32_t* reg, uint32_t value) {
    ++register_writes;
    if (reg == &CEC->CSR) {
        const uint32_t before = CEC->CSR;
        const uint32_t teom = (before & (CEC_CSR_RBTF | CEC_CSR_RERR)) != 0U
                                  ? before & CEC_CSR_TEOM
                                  : value & CEC_CSR_TEOM;
        if ((value & CEC_CSR_TERR) == 0U)
            CEC->ESR &= ~tx_errors;
        *reg =
            (before & flags & value) | teom | ((before | value) & CEC_CSR_TSOM);
        return;
    }
    if (reg == &RCC->APB1RSTR && (value & RCC_APB1RSTR_CECRST) != 0U) {
        EXPECT_EQ(CEC->CFGR & CEC_CFGR_PE, 0U);
        nx_cec_registers = {};
    }
    if (reg == &CEC->TXD) {
        ASSERT_LT(written_size, 16U);
        written[written_size++] = value;
    }
    if (reg == &CEC->CFGR && defer_disable && (value & CEC_CFGR_PE) == 0U)
        value |= CEC->CFGR & static_cast<uint32_t>(CEC_CFGR_PE);
    *reg = value;
}

extern "C" void nx_cec_model_bsrr(GPIO_TypeDef* gpio, uint32_t value) {
    gpio->ODR = (gpio->ODR | (value & 0xffffU)) & ~(value >> 16U);
}
