/**
 * \file            connectivity_test.cpp
 * \brief           Actual F1 clock, VBUS, EP0 SETUP and stop contracts.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "connectivity_model.h"
#include "drivers/usb/connectivity_device.h"
#include "system/platform.h"
#include <gtest/gtest.h>

RCC_TypeDef nx_f1_dwc_model_rcc;
GPIO_TypeDef nx_f1_dwc_model_gpio;
nx_native_usb_device_t nx_f1_dwc_model;

namespace {
constexpr unsigned dctl = 0x804U / 4U;
constexpr unsigned gccfg = 0x38U / 4U;
constexpr uint32_t floating = 0x44444444U;
bool irq_busy;

class ConnectivityDwc : public testing::Test {
  protected:
    nx_native_usb_device_t& model = nx_f1_dwc_model;
    nx_stm32f1_dwc_device_owner_t owner{};
    nx_usb_endpoint_t endpoints[2]{};
    nx_usb_event_t events[8]{};
    const nx_usb_endpoint_config_t plan[2]{
        {0U, true, NX_USB_PROFILE_CONTROL, 64U, 64U, 16U},
        {0U, false, NX_USB_PROFILE_CONTROL, 64U, 0U, 0U}};
    nx_stm32f1_dwc_device_config_t config{};
    const nx_usb_device_port_t port{&nx_usb_fs_device_ops, &model.controller};

    void SetUp() override {
        nx_f1_dwc_model_rcc = {};
        nx_f1_dwc_model_gpio = {};
        model = {};
        irq_busy = false;
        RCC->CR = RCC_CR_PLLON | RCC_CR_PLLRDY | RCC_CR_PLL2ON |
                  RCC_CR_PLL2RDY | RCC_CR_HSEON | RCC_CR_HSERDY;
        RCC->CFGR = RCC_CFGR_SWS_PLL | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
        RCC->CFGR2 = RCC_CFGR2_PREDIV1_DIV5 | RCC_CFGR2_PREDIV2_DIV5 |
                     RCC_CFGR2_PLL2MUL8 | RCC_CFGR2_PREDIV1SRC_PLL2;
        GPIOA->CRH = floating;
        model.hardware.registers[0x10U / 4U] = 1U << 31U;
        model.hardware.registers[0] = USB_OTG_GOTGCTL_BSVLD;
        model.hardware.endpoint_numbers = 4U;
        config = {endpoints, plan,  events,
                  2U,        8U,    {NX_IRQ_KERNEL_NONE, 68U, 4U, 0U},
                  128U,      1000U, 5000U,
                  64U,       5U};
    }

    nx_result_t start() {
        return nx_stm32f1_dwc_device_configure(&model.controller, &config,
                                               &owner);
    }

    nx_result_t stop() {
        return nx_stm32f1_dwc_device_close(&model.controller, &owner);
    }
};
} /* namespace */

extern "C" bool nx_f1_dwc_model_irq_busy(void) {
    return irq_busy;
}
extern "C" void nx_f1_dwc_model_bsrr(GPIO_TypeDef* registers, uint32_t value) {
    registers->ODR = (registers->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
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
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    return {};
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t state) {
    (void)state;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" nx_result_t nx_native_clock_advance(uint64_t elapsed) {
    (void)elapsed;
    return NX_ERROR_UNSUPPORTED;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 100U;
}

TEST_F(ConnectivityDwc, SensedDeviceHandlesEp0SetupAndProvesDetach) {
    ASSERT_EQ(NX_SUCCESS, start());
    EXPECT_EQ(USB_OTG_GCCFG_PWRDWN | USB_OTG_GCCFG_VBUSBSEN,
              model.hardware.registers[gccfg]);
    EXPECT_EQ(0U, model.hardware.registers[dctl] & USB_OTG_DCTL_SDIS);
    EXPECT_TRUE(model.hardware.irq_enabled);
    EXPECT_EQ(OTG_FS_IRQn, model.controller.irq.irq_number);
    nx_native_usb_reset(&model);
    nx_usb_controller_irq(&model.controller);
    constexpr uint8_t setup[8] = {0x80U, 6U, 0U, 1U, 0U, 0U, 18U, 0U};
    ASSERT_EQ(NX_SUCCESS,
              nx_native_usb_receive(&model, 0U, setup, sizeof(setup), true));
    nx_usb_controller_irq(&model.controller);
    nx_usb_event_t observed[8]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_usb_device_read_events(&port, observed, 8U, &count));
    bool has_setup = false;
    for (size_t i = 0U; i < count; ++i)
        has_setup |= observed[i].kind == NX_USB_EVENT_SETUP;
    EXPECT_TRUE(has_setup);
    EXPECT_EQ(NX_SUCCESS, stop());
    EXPECT_NE(0U, model.hardware.registers[dctl] & USB_OTG_DCTL_SDIS);
    EXPECT_FALSE(model.hardware.irq_enabled);
    EXPECT_EQ(0U, model.hardware.registers[gccfg] &
                      (USB_OTG_GCCFG_PWRDWN | USB_OTG_GCCFG_VBUSBSEN));
    EXPECT_EQ(0U, RCC->AHBENR);
    EXPECT_EQ(floating, GPIOA->CRH);
}

TEST_F(ConnectivityDwc, MissingSensedVbusStaysSoftDisconnected) {
    model.hardware.registers[0] = 0U;
    EXPECT_EQ(NX_ERROR_STATE, start());
    EXPECT_NE(0U, model.hardware.registers[dctl] & USB_OTG_DCTL_SDIS);
    EXPECT_TRUE(owner.retained);
    EXPECT_EQ(NX_SUCCESS, stop());
    EXPECT_EQ(0U, RCC->AHBENR);
}

TEST_F(ConnectivityDwc, ClockIrqAndFailedDisconnectNeverGrantPads) {
    RCC->CFGR |= RCC_CFGR_OTGFSPRE;
    EXPECT_EQ(NX_ERROR_STATE, start());
    EXPECT_EQ(0U, RCC->AHBENR);
    RCC->CFGR &= ~static_cast<uint32_t>(RCC_CFGR_OTGFSPRE);
    irq_busy = true;
    EXPECT_EQ(NX_ERROR_BUSY, start());
    EXPECT_EQ(0U, RCC->AHBENR);
    irq_busy = false;
    model.hardware.hold_disconnect = true;
    EXPECT_EQ(NX_ERROR_IO, start());
    EXPECT_EQ(0U, RCC->APB2ENR);
    model.hardware.hold_disconnect = false;
    EXPECT_EQ(NX_SUCCESS, stop());
    EXPECT_EQ(0U, RCC->AHBENR);
}

TEST_F(ConnectivityDwc, AddressCommitAndResetObserveTheEs022ReadDelay) {
    ASSERT_EQ(NX_SUCCESS, start());
    model.hardware.delay_config_read = true;
    constexpr uint8_t setup[8] = {0U, 5U, 7U, 0U, 0U, 0U, 0U, 0U};
    ASSERT_EQ(NX_SUCCESS,
              nx_native_usb_receive(&model, 0U, setup, sizeof(setup), true));
    nx_usb_controller_irq(&model.controller);
    const uint64_t epoch = model.controller.snapshot.epoch;
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_address(&port, epoch, 7U));
    nx_usb_endpoint_port_t in{&nx_usb_fs_endpoint_ops, &endpoints[0]};
    nx_usb_request_t request{};
    nx_request_initialize(&request.base);
    ASSERT_EQ(NX_SUCCESS, nx_usb_in_prepare(&request, nullptr, 0U,
                                            NX_USB_STATUS, epoch, 200U));
    ASSERT_EQ(NX_SUCCESS, nx_usb_endpoint_submit(&in, &request));
    ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, true));
    nx_usb_controller_irq(&model.controller);
    nx_usb_device_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_state(&port, &state));
    EXPECT_EQ(7U, state.address);
    nx_native_usb_reset(&model);
    nx_usb_controller_irq(&model.controller);
    ASSERT_EQ(NX_SUCCESS, nx_usb_device_state(&port, &state));
    EXPECT_EQ(0U, state.address);
    EXPECT_FALSE(model.hardware.early_config_read);
    EXPECT_EQ(NX_SUCCESS, stop());
}
