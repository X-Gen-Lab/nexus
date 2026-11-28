/**
 * \file            dac_test.cpp
 * \brief           Verify F100 digital code encoding, wake and release.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/dac/instance.h"
#include "system/platform.h"
#include <gtest/gtest.h>

DAC_TypeDef model_dac{};
RCC_TypeDef model_rcc{};
GPIO_TypeDef model_gpio{};
static nx_time_us_t now;
static nx_time_us_t enabled_at;
static bool hold_transfer;
static bool refuse_disable;

extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++now;
}

extern "C" void f100_dac_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &DAC->CR) {
        if ((DAC->CR & DAC_CR_EN1) == 0U && (value & DAC_CR_EN1) != 0U) {
            enabled_at = now;
        } else if ((DAC->CR & DAC_CR_EN1) != 0U && (value & DAC_CR_EN1) == 0U &&
                   refuse_disable) {
            return;
        }
    }
    *reg = value;
    if (!hold_transfer) {
        if (reg == &DAC->DHR12R1)
            *const_cast<volatile uint32_t*>(&DAC->DOR1) = value;
        if (reg == &DAC->DHR12L1)
            *const_cast<volatile uint32_t*>(&DAC->DOR1) = value >> 4U;
        if (reg == &DAC->DHR8R1)
            *const_cast<volatile uint32_t*>(&DAC->DOR1) = value << 4U;
    }
}

class F100Dac : public testing::Test {
  protected:
    nx_stm32f1_dac_t instance{};
    nx_dac_port_t port{&nx_stm32f1_dac_ops, &instance};

    void SetUp() override {
        *DAC = {};
        *RCC = {};
        *GPIOA = {};
        GPIOA->CRL = 0x44444444U;
        now = 0U;
        enabled_at = 0U;
        hold_transfer = false;
        refuse_disable = false;
    }
};

TEST_F(F100Dac, TakesOnlyAnalogPadAndWaitsAfterEnable) {
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_dac_prepare(
                              &instance, NX_DAC_FORMAT_U12_RIGHT, 123U, true));
    EXPECT_EQ(0x44404444U, GPIOA->CRL);
    nx_dac_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_info(&port, &info));
    EXPECT_EQ(4095U, info.maximum_code);
    EXPECT_EQ(1U, info.channel_mask);
    EXPECT_TRUE(info.output_buffer);
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(&port, 0U, 50U));
    EXPECT_GE(now - enabled_at, 12U);
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_dac_port_write(&port, NX_DAC_CHANNEL_2, 0U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_dac_port_write(&port, NX_DAC_CHANNEL_1, 4096U));
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_write(&port, NX_DAC_CHANNEL_1, 700U));
    EXPECT_EQ(700U, DAC->DOR1);
    ASSERT_EQ(NX_SUCCESS, nx_dac_port_stop(&port, 100U));
    EXPECT_EQ(123U, DAC->DOR1);
    EXPECT_EQ(DAC_CR_EN1, DAC->CR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_dac_close(&instance));
    EXPECT_EQ(0U, DAC->CR);
    EXPECT_EQ(RCC_APB1ENR_TIM2EN, RCC->APB1ENR);
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
}

TEST_F(F100Dac, NativePackingAndDigitalValueAgreeForByteAndLeft) {
    for (const auto format : {NX_DAC_FORMAT_U8, NX_DAC_FORMAT_U12_LEFT}) {
        const bool byte = format == NX_DAC_FORMAT_U8;
        const uint16_t code = byte ? 0xA5U : 0x7A5U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32f1_dac_prepare(&instance, format, 0U, false));
        nx_dac_state_t value{};
        ASSERT_EQ(NX_SUCCESS, nx_dac_port_state(&port, &value));
        EXPECT_EQ(0U, value.enabled_channels);
        ASSERT_EQ(NX_SUCCESS, nx_dac_port_start(&port, 0U, now + 50U));
        EXPECT_NE(0U, DAC->CR & DAC_CR_BOFF1);
        ASSERT_EQ(NX_SUCCESS, nx_dac_port_write(&port, NX_DAC_CHANNEL_1, code));
        EXPECT_EQ(byte ? code : static_cast<uint32_t>(code) << 4U,
                  byte ? DAC->DHR8R1 : DAC->DHR12L1);
        ASSERT_EQ(NX_SUCCESS, nx_dac_port_state(&port, &value));
        EXPECT_EQ(code, value.output_codes[0]);
        EXPECT_EQ(byte ? 8U : 12U, value.code_bits);
        EXPECT_EQ(NX_SUCCESS, nx_stm32f1_dac_close(&instance));
    }
}

TEST_F(F100Dac, AdmissionRejectsClockAndDeadlineBeforeEffects) {
    RCC->APB1ENR = RCC_APB1ENR_DACEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_dac_prepare(
                                 &instance, NX_DAC_FORMAT_U12_RIGHT, 0U, true));
    EXPECT_EQ(RCC_APB1ENR_DACEN, RCC->APB1ENR);
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
    RCC->APB1ENR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_dac_prepare(
                              &instance, NX_DAC_FORMAT_U12_RIGHT, 1U, true));
    EXPECT_EQ(NX_ERROR_INVALID, nx_dac_port_start(&port, 1U, 40U));
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_dac_port_start(&port, 0U, 8U));
    EXPECT_EQ(0U, DAC->CR);
    EXPECT_EQ(0U, DAC->DHR12R1);
}

TEST_F(F100Dac, FailedIdleAndDisableRetainTheOwnerUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_dac_prepare(
                              &instance, NX_DAC_FORMAT_U12_RIGHT, 123U, true));
    hold_transfer = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_dac_port_start(&port, 0U, 50U));
    EXPECT_NE(0U, DAC->CR & DAC_CR_EN1);
    EXPECT_EQ(NX_ERROR_STATE, nx_dac_port_write(&port, NX_DAC_CHANNEL_1, 1U));
    hold_transfer = false;
    EXPECT_EQ(NX_SUCCESS, nx_dac_port_stop(&port, 100U));
    EXPECT_EQ(123U, DAC->DOR1);
    refuse_disable = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_dac_close(&instance));
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_DACEN);
    EXPECT_EQ(0x44404444U, GPIOA->CRL);
    refuse_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_dac_close(&instance));
}
