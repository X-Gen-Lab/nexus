/**
 * \file            usb_hs_clock_test.cpp
 * \brief           HS/ULPI gate handover and retained clock consumers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
#include "stm32f407_usb_hs_clock.h"
}
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

static nx_time_us_t s_now;
static bool s_refuse_enable;
static bool s_refuse_disable;
static uint32_t s_writes;
extern "C" nx_time_us_t nx_time_now_us(void) {
    return s_now++;
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_usb_clock_model_poll(void) {
}
extern "C" void nx_usb_clock_model_write(volatile uint32_t* address,
                                         uint32_t value) {
    ++s_writes;
    uint32_t mask = RCC_AHB1ENR_OTGHSEN | RCC_AHB1ENR_OTGHSULPIEN;
    if ((s_refuse_enable && (value & mask) != 0U) ||
        (s_refuse_disable && (value & mask) == 0U)) {
        return;
    }
    *address = value;
}

class UsbHsClock : public testing::Test {
  protected:
    nx_stm32_usb_hs_clock_state_t state = {};
    void SetUp() override {
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x40023000U), 0x1000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        s_now = 10U;
        s_refuse_enable = s_refuse_disable = false;
        s_writes = 0U;
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        RCC->CFGR = NEXUS_CLOCK_CFGR | NEXUS_CLOCK_SW | (NEXUS_CLOCK_SW << 2U);
    }
    void TearDown() override {
        s_refuse_enable = s_refuse_disable = false;
        if (state.phase != 0U && state.consumers == 0U) {
            EXPECT_EQ(nx_stm32_usb_hs_clock_stop(&state, s_now + 100U),
                      NX_SUCCESS);
        }
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40023000U), 0x1000U), 0);
    }
};

TEST_F(UsbHsClock, InternalFsRejectsAChangedActualAhbOrApbPlan) {
    RCC->CFGR ^= RCC_CFGR_PPRE1_0;
    EXPECT_EQ(nx_stm32_usb_hs_clock_start(&state, false, 0U, 100U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(UsbHsClock, UlpiStillRequiresTheSelectedActualCoreClockPlan) {
    RCC->CFGR ^= RCC_CFGR_HPRE_3;
    EXPECT_EQ(nx_stm32_usb_hs_clock_start(&state, true, 60000000U, 100U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(UsbHsClock, InternalFsAndUlpiOwnDifferentExactGateSets) {
    ASSERT_EQ(nx_stm32_usb_hs_clock_start(&state, false, 0U, 100U), NX_SUCCESS);
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_OTGHSEN);
    ASSERT_NE(nx_stm32_usb_hs_clock_ops.validate, nullptr);
    ASSERT_NE(nx_stm32_usb_hs_clock_ops.retain, nullptr);
    ASSERT_NE(nx_stm32_usb_hs_clock_ops.release, nullptr);
    EXPECT_EQ(nx_stm32_usb_hs_clock_ops.retain(&state), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_usb_hs_clock_stop(&state, 100U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32_usb_hs_clock_ops.release(&state), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_usb_hs_clock_stop(&state, 100U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_usb_hs_clock_start(&state, true, 60000000U, 100U),
              NX_SUCCESS);
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_OTGHSEN | RCC_AHB1ENR_OTGHSULPIEN);
    EXPECT_EQ(RCC->AHB2ENR, 0U);
    EXPECT_EQ(nx_stm32_usb_hs_clock_stop(&state, 100U), NX_SUCCESS);
}

TEST_F(UsbHsClock, ForeignGateAndWrongExternalClockRejectWithoutWrites) {
    EXPECT_EQ(nx_stm32_usb_hs_clock_start(&state, true, 48000000U, 100U),
              NX_ERROR_INVALID);
    RCC->AHB1ENR = RCC_AHB1ENR_OTGHSULPIEN;
    EXPECT_EQ(nx_stm32_usb_hs_clock_start(&state, true, 60000000U, 100U),
              NX_ERROR_BUSY);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(UsbHsClock, FailedWithdrawalKeepsTheOriginalPhyAndCleanupReceipt) {
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
    ASSERT_EQ(nx_stm32_usb_hs_clock_start(&state, true, 60000000U, 100U),
              NX_SUCCESS);
    s_refuse_disable = true;
    EXPECT_EQ(nx_stm32_usb_hs_clock_stop(&state, 100U), NX_ERROR_IO);
    EXPECT_NE(state.phase, 0U);
    EXPECT_TRUE(state.ulpi);
    EXPECT_EQ(nx_stm32_usb_hs_clock_start(&state, false, 0U, 100U),
              NX_ERROR_BUSY);
    s_refuse_disable = false;
    EXPECT_EQ(nx_stm32_usb_hs_clock_stop(&state, 100U), NX_SUCCESS);
    EXPECT_EQ(RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN);
}

TEST_F(UsbHsClock, FailedAcquisitionCannotPublishReadyOrRetainConsumer) {
    s_refuse_enable = true;
    EXPECT_EQ(nx_stm32_usb_hs_clock_start(&state, false, 0U, 100U),
              NX_ERROR_IO);
    ASSERT_NE(nx_stm32_usb_hs_clock_ops.retain, nullptr);
    EXPECT_EQ(nx_stm32_usb_hs_clock_ops.retain(&state), NX_ERROR_STATE);
    EXPECT_EQ(state.consumers, 0U);
    EXPECT_EQ(nx_stm32_usb_hs_clock_stop(&state, 100U), NX_SUCCESS);
}
