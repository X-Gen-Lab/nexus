/**
 * \file            legacy_devices_factory_test.cpp
 * \brief           Actual generated F103/GD303 physical-device lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifdef NX_LEGACY_FACTORY_F103
#include "stm32f103_provider.h"
#else
#include "gd32f303_provider.h"
#endif
#include "model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
class LegacyDeviceFactory : public testing::Test {
  protected:
#ifndef NX_LEGACY_FACTORY_F103
    void* mapping = MAP_FAILED;
#endif
    void SetUp() override {
#ifdef NX_LEGACY_FACTORY_F103
        nx_f103_model_reset();
#else
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        std::memset(g_gd303_irq, 0, sizeof(g_gd303_irq));
        std::memset(g_gd303_pending, 0, sizeof(g_gd303_pending));
        g_gd303_ignore = nullptr;
        g_gd303_writes = 0U;
        g_gd303_hold_update = g_gd303_hold_irq_off = false;
        g_gd303_pend_on_timer_withdraw = g_gd303_hold_late_irq_off = false;
        g_gd303_exti_hold_pending = g_gd303_exti_reassert_pending = false;
        g_gd303_carry_on_low_read = false;
        g_nx_gd32f303_system = {};
        RCU_CTL = RCU_CTL_IRC8MEN | RCU_CTL_IRC8MSTB;
        GPIO_CTL0(GPIOA) = GPIO_CTL1(GPIOA) = 0x44444444U;
#endif
    }
    void TearDown() override {
#ifndef NX_LEGACY_FACTORY_F103
        if (mapping != MAP_FAILED)
            munmap(mapping, 0x30000U);
#endif
    }
};
TEST_F(LegacyDeviceFactory, ColdLookupPreservesHardwareAndReportsHalfwordUnit) {
    ASSERT_EQ(1U, NX_FLASH_ID_COUNT);
    auto* face = nx_factory_flash(NX_FLASH_ID_PHYSICAL);
    ASSERT_NE(nullptr, face);
    EXPECT_EQ(face, nx_factory_flash(NX_FLASH_ID_PHYSICAL));
    const auto* cap = nx_factory_flash_capability(NX_FLASH_ID_PHYSICAL);
    ASSERT_NE(nullptr, cap);
    EXPECT_EQ(2U, cap->unit_bytes);
    EXPECT_EQ(1U, cap->buffer_alignment);
#ifdef NX_LEGACY_FACTORY_F103
    EXPECT_EQ(0U, g_f103_model.writes);
#else
    EXPECT_EQ(0U, g_gd303_writes);
#endif
}
TEST_F(LegacyDeviceFactory, ExactStaticAssemblyStartsAndRestoresEveryLease) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    const auto* geometry =
        nx_flash_port_geometry(nx_factory_flash(NX_FLASH_ID_PHYSICAL));
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(2U, geometry->program_unit);
    EXPECT_EQ(128U, geometry->sector_count);
    nx_adc_info_t adc{};
    ASSERT_EQ(NX_SUCCESS,
              nx_adc_port_info(nx_factory_adc(NX_ADC_ID_INPUT), &adc));
    EXPECT_EQ(12U, adc.resolution_bits);
    nx_pwm_state_t pwm{};
    ASSERT_EQ(NX_SUCCESS,
              nx_pwm_port_state(nx_factory_pwm(NX_PWM_ID_OUTPUT), &pwm));
    EXPECT_FALSE(pwm.running);
    EXPECT_EQ(1000U, pwm.period_ticks);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
#ifdef NX_LEGACY_FACTORY_F103
    EXPECT_EQ(0x44444444U, GPIOA->CRL);
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->APB2ENR);
#else
    EXPECT_EQ(0x44444444U, GPIO_CTL0(GPIOA));
    EXPECT_EQ(0U, RCU_APB1EN);
    EXPECT_EQ(0U, RCU_APB2EN);
#endif
}
TEST_F(LegacyDeviceFactory, FailedConverterCloseRetainsPlatformUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
#ifdef NX_LEGACY_FACTORY_F103
    g_f103_model.ignore_write = &ADC1->CR2;
#else
    g_gd303_ignore = &ADC_CTL1(ADC0);
#endif
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
#ifdef NX_LEGACY_FACTORY_F103
    g_f103_model.ignore_write = nullptr;
#else
    g_gd303_ignore = nullptr;
#endif
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
}
