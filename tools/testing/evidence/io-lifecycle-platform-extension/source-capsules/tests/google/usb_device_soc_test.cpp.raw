/**
 * \file            usb_device_soc_test.cpp
 * \brief           Both actual USB SDK bindings execute the production DCD
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "usb_device.h"
#ifdef NX_USB_GD_MODEL
#include "gd32f470_usb_device.h"
#include "gd32f4xx_rcu.h"
#else
#include "stm32f407_usb_device.h"
#include "stm32f407xx.h"
#endif
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#ifdef NX_USB_GD_MODEL
using ClockState = nx_gd32_usb_clock_state_t;
#define CLOCK_START nx_gd32_usb_clock_start
#define CLOCK_STOP  nx_gd32_usb_clock_stop
#define DEVICE_INIT nx_gd32_usb_device_initialize
#define IRQ_COUNT   91U
#else
using ClockState = nx_stm32_usb_clock_state_t;
#define CLOCK_START nx_stm32_usb_clock_start
#define CLOCK_STOP  nx_stm32_usb_clock_stop
#define DEVICE_INIT nx_stm32_usb_device_initialize
#define IRQ_COUNT   82U
#endif
extern "C" void nx_usb_clock_model_write(volatile uint32_t* address,
                                         uint32_t value) {
    *address = value;
#ifdef NX_USB_GD_MODEL
    if (address == &RCU_CTL) {
        if ((value & RCU_CTL_PLLSAIEN) != 0U) {
            RCU_CTL |= RCU_CTL_PLLSAISTB;
        } else {
            RCU_CTL &= ~static_cast<uint32_t>(RCU_CTL_PLLSAISTB);
        }
    }
#endif
}
extern "C" void nx_usb_clock_model_poll(void) {
}
class UsbSoc : public testing::Test {
  protected:
    nx_native_usb_hardware_t* hardware = nullptr;
    nx_usb_controller_t controller = {};
    nx_usb_endpoint_t endpoints[4] = {};
    nx_usb_event_t events[2] = {};
    ClockState clock = {};
    const nx_usb_device_port_t port = {&nx_usb_fs_device_ops, &controller};
    void SetUp() override {
        ASSERT_NE(mmap(reinterpret_cast<void*>(0x40023000U), 0x1000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0),
                  MAP_FAILED);
        void* mapped = mmap(reinterpret_cast<void*>(0x50000000U), 0x10000U,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_NE(mapped, MAP_FAILED);
        hardware = static_cast<nx_native_usb_hardware_t*>(mapped);
        hardware->registers[0x10U / 4U] = 1U << 31U;
        ASSERT_EQ(nx_native_clock_configure(true, 100), NX_SUCCESS);
#ifdef NX_USB_GD_MODEL
        RCU_PLL = 25U | (400U << 6U) | RCU_PLLSRC_HXTAL | (9U << 24U);
        RCU_CTL =
            RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_PLLEN | RCU_CTL_PLLSTB;
#else
        RCC->PLLCFGR = 8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
#endif
        ASSERT_EQ(CLOCK_START(&clock, true, 200), NX_SUCCESS);
        controller.endpoints = endpoints;
        controller.endpoint_count = 4;
        controller.events = events;
        controller.capacity = 2;
        controller.rx_fifo_words = 128;
        controller.poll_limit = 4;
        controller.drain_timeout_us = 20;
        controller.irq_policy = {NX_IRQ_KERNEL_BASEPRI, IRQ_COUNT, 4, 5};
        controller.irq = {67, 5, 3};
        for (unsigned i = 0; i < 4; ++i) {
            endpoints[i].number = static_cast<uint8_t>(i / 2U);
            endpoints[i].in = (i % 2U) == 0;
            endpoints[i].bulk = i >= 2;
            endpoints[i].max_packet = 64;
        }
        endpoints[0].fifo_start = 128;
        endpoints[0].fifo_words = 32;
        endpoints[2].fifo_start = 160;
        endpoints[2].fifo_words = 32;
        ASSERT_EQ(DEVICE_INIT(&controller, &clock), NX_SUCCESS);
    }
    void TearDown() override {
        hardware->hold_flush = hardware->hold_disable = hardware->hold_clear =
            false;
        hardware->hold_irq_disable = hardware->hold_gonak = false;
        EXPECT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
        EXPECT_EQ(CLOCK_STOP(&clock, 200), NX_SUCCESS);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x50000000U), 0x10000U), 0);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40023000U), 0x1000U), 0);
    }
};
TEST_F(UsbSoc, RealRegisterFactsAndClockConsumerSurviveUntilDetach) {
    EXPECT_EQ(controller.registers, 0x50000000U);
    EXPECT_EQ(controller.irq.irq_number, 67);
    EXPECT_EQ(clock.consumers, 1U);
    EXPECT_EQ(CLOCK_STOP(&clock, 200), NX_ERROR_BUSY);
    EXPECT_EQ(hardware->registers[0x24U / 4U], 128U);
    EXPECT_EQ(hardware->registers[0x28U / 4U], (32U << 16U) | 128U);
    EXPECT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    EXPECT_EQ(clock.consumers, 0U);
}
TEST_F(UsbSoc, ActualSdkEndpointOffsetWritesPioAndWaitsForHostXfrc) {
    uint8_t bytes[3] = {1, 2, 3};
    nx_usb_request_t request = {};
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_in_prepare(&request, bytes, 3, NX_USB_DATA, 0, 200),
              NX_SUCCESS);
    nx_usb_endpoint_port_t endpoint = {&nx_usb_fs_endpoint_ops, &endpoints[2]};
    ASSERT_EQ(nx_usb_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    EXPECT_EQ(hardware->registers[0x930U / 4U], (1U << 19U) | 3U);
    EXPECT_EQ(hardware->tx[1][0], 0x030201U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    hardware->registers[0x920U / 4U] &= ~(1U << 31U);
    hardware->registers[0x928U / 4U] |= 1U;
    hardware->registers[0x938U / 4U] = 32;
    nx_usb_controller_irq(&controller);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 3U);
}
TEST_F(UsbSoc, FalseCpuFactsRejectBeforeBorrowingTheClock) {
    EXPECT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    controller.irq_policy.priority_bits = 3;
    EXPECT_EQ(DEVICE_INIT(&controller, &clock), NX_ERROR_INVALID);
    EXPECT_EQ(clock.consumers, 0U);
}
TEST_F(UsbSoc, ForeignCoreIsNotReboundOrReset) {
    EXPECT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    controller.registers = 0x40040000U;
    EXPECT_EQ(DEVICE_INIT(&controller, &clock), NX_ERROR_INVALID);
    EXPECT_EQ(controller.registers, 0x40040000U);
    EXPECT_EQ(clock.consumers, 0U);
}
TEST_F(UsbSoc, FailedCoreResetKeepsClockAndOwnerStorage) {
    EXPECT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    hardware->hold_reset = true;
    EXPECT_EQ(DEVICE_INIT(&controller, &clock), NX_ERROR_IO);
    EXPECT_EQ(clock.consumers, 1U);
    EXPECT_TRUE(controller.snapshot.closing);
    EXPECT_EQ(CLOCK_STOP(&clock, 200), NX_ERROR_BUSY);
    hardware->hold_reset = false;
}

TEST_F(UsbSoc, FreshResetFailureCanBeStoppedWithoutDereferencingNullEndpoint) {
    ASSERT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    for (auto& endpoint : endpoints) {
        endpoint.controller = nullptr;
    }
    hardware->hold_reset = true;
    EXPECT_EQ(DEVICE_INIT(&controller, &clock), NX_ERROR_IO);
    EXPECT_EQ(clock.consumers, 1U);
    EXPECT_EXIT(
        {
            hardware->hold_reset = false;
            auto result = nx_usb_device_stop(&port);
            _exit(result == NX_SUCCESS && clock.consumers == 0U ? 0 : 1);
        },
        testing::ExitedWithCode(0), "");
    /* Keep the parent fixture cleanup valid even while executing the RED
     * revision.
     */
    for (auto& endpoint : endpoints) {
        endpoint.controller = &controller;
    }
    hardware->hold_reset = false;
}
TEST_F(UsbSoc, FreshFlushFailureRetainsClockUntilBoundedStopRetry) {
    ASSERT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    for (auto& endpoint : endpoints) {
        endpoint.controller = nullptr;
    }
    hardware->hold_flush = true;
    EXPECT_EQ(DEVICE_INIT(&controller, &clock), NX_ERROR_IO);
    EXPECT_EQ(clock.consumers, 1U);
    EXPECT_EXIT(
        {
            auto held = nx_usb_device_stop(&port);
            hardware->hold_flush = false;
            auto recovered = nx_usb_device_stop(&port);
            _exit(held == NX_ERROR_BUSY && recovered == NX_SUCCESS &&
                          clock.consumers == 0U
                      ? 0
                      : 1);
        },
        testing::ExitedWithCode(0), "");
    for (auto& endpoint : endpoints) {
        endpoint.controller = &controller;
    }
    hardware->hold_flush = false;
}
