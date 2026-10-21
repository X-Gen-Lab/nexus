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
#include "nexus/io/usb_activation.h"
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
    nx_usb_request_t interrupt_request = {};
    uint8_t interrupt_bytes[64] = {1U, 2U, 3U};
    const nx_usb_device_port_t port = {&nx_usb_fs_device_ops, &controller};
    /** \brief The fixture owner explicitly selects its test directions. */
    nx_result_t ActivateSelected() {
        const nx_usb_activation_port_t activation = {&nx_usb_fs_activation_ops,
                                                     &controller};
        for (const auto& selected : endpoints) {
            if (selected.number == 0U) {
                continue;
            }
            const auto address = static_cast<uint8_t>(
                selected.number | (selected.in ? 0x80U : 0U));
            nx_result_t result = nx_usb_activation_set(
                &activation, controller.snapshot.epoch, address, true);
            if (result != NX_SUCCESS) {
                return result;
            }
        }
        return NX_SUCCESS;
    }
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
            endpoints[i].transfer = static_cast<uint8_t>(
                i >= 2 ? NX_USB_PROFILE_BULK : NX_USB_PROFILE_CONTROL);
            endpoints[i].max_packet = 64;
        }
        endpoints[0].fifo_start = 128;
        endpoints[0].fifo_words = 32;
        endpoints[2].fifo_start = 160;
        endpoints[2].fifo_words = 32;
        nx_request_initialize(&interrupt_request.base);
        ASSERT_EQ(DEVICE_INIT(&controller, &clock), NX_SUCCESS);
        ASSERT_EQ(ActivateSelected(), NX_SUCCESS);
    }
    nx_result_t StartInterrupt(uint8_t number, uint16_t maximum) {
        nx_result_t result = nx_usb_device_stop(&port);
        if (result != NX_SUCCESS) {
            return result;
        }
        endpoints[2].number = number;
        endpoints[2].transfer = NX_USB_PROFILE_INTERRUPT;
        endpoints[2].max_packet = maximum;
        endpoints[2].fifo_words = 16U;
        result = DEVICE_INIT(&controller, &clock);
        return result == NX_SUCCESS ? ActivateSelected() : result;
    }
    void TearDown() override {
        hardware->hold_flush = hardware->hold_disable = hardware->hold_clear =
            false;
        hardware->hold_irq_disable = hardware->hold_gonak = false;
        hardware->hold_bulk_pid = false;
        hardware->hold_stall_clear_mask = 0U;
        EXPECT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
        EXPECT_EQ(CLOCK_STOP(&clock, 200), NX_SUCCESS);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x50000000U), 0x10000U), 0);
        EXPECT_EQ(munmap(reinterpret_cast<void*>(0x40023000U), 0x1000U), 0);
    }
};
TEST_F(UsbSoc, InterruptDataAndZlpNeedXfrcAndDedicatedFifoWithdrawal) {
    struct Selection {
        uint8_t number;
        uint16_t maximum;
    };
    const Selection selections[] = {{1U, 1U}, {2U, 10U}, {3U, 64U}};
    for (const auto& selection : selections) {
        SCOPED_TRACE(testing::Message()
                     << "EP=" << static_cast<unsigned>(selection.number)
                     << " MPS=" << selection.maximum);
        ASSERT_EQ(StartInterrupt(selection.number, selection.maximum),
                  NX_SUCCESS);
        nx_usb_endpoint_port_t in = {&nx_usb_fs_endpoint_ops, &endpoints[2]};
        const uint32_t offset = 0x900U + selection.number * 32U;
        const uint32_t control = hardware->registers[offset / 4U];
        EXPECT_EQ((control >> 18U) & 3U, 3U);
        EXPECT_EQ(control & 0x7FFU, selection.maximum);
        EXPECT_EQ((control >> 22U) & 15U, selection.number);
        EXPECT_EQ(hardware->registers[(0x100U + selection.number * 4U) / 4U],
                  (16U << 16U) | 160U);
        EXPECT_EQ(hardware->registers[0x834U / 4U], 0U);
        const auto other_out = hardware->registers[0xB20U / 4U];
        for (size_t length : {size_t{selection.maximum}, size_t{0}}) {
            SCOPED_TRACE(testing::Message() << "length=" << length);
            ASSERT_EQ(
                nx_usb_in_prepare(&interrupt_request,
                                  length == 0U ? nullptr : interrupt_bytes,
                                  length, NX_USB_DATA, 0U, 200U),
                NX_SUCCESS);
            ASSERT_EQ(nx_usb_endpoint_submit(&in, &interrupt_request),
                      NX_SUCCESS);
            EXPECT_EQ(hardware->registers[(offset + 16U) / 4U],
                      (1U << 19U) | static_cast<uint32_t>(length));
            EXPECT_EQ(hardware->registers[0x834U / 4U], 0U);
            EXPECT_EQ(hardware->tx_count[selection.number], (length + 3U) / 4U);
            if (length != 0U) {
                EXPECT_EQ(hardware->tx[selection.number][0],
                          length == 1U ? 1U : 0x030201U);
            }
            EXPECT_EQ(nx_request_state(&interrupt_request.base),
                      NX_REQUEST_ACTIVE);
            EXPECT_EQ(endpoints[2].request, &interrupt_request);
            hardware->registers[(offset + 8U) / 4U] |= 16U;
            nx_usb_controller_irq(&controller);
            EXPECT_EQ(nx_request_state(&interrupt_request.base),
                      NX_REQUEST_ACTIVE);
            hardware->registers[(offset + 8U) / 4U] |= 1U;
            nx_usb_controller_irq(&controller);
            EXPECT_EQ(nx_request_state(&interrupt_request.base),
                      NX_REQUEST_ACTIVE);
            hardware->registers[offset / 4U] &= ~(1U << 31U);
            hardware->registers[(offset + 24U) / 4U] = 15U;
            nx_usb_controller_irq(&controller);
            EXPECT_EQ(nx_request_state(&interrupt_request.base),
                      NX_REQUEST_ACTIVE);
            EXPECT_EQ(endpoints[2].request, &interrupt_request);
            hardware->registers[(offset + 24U) / 4U] = 16U;
            hardware->tx_count[selection.number] = 0U;
            hardware->registers[(offset + 8U) / 4U] |= 16U;
            nx_usb_controller_irq(&controller);
            ASSERT_EQ(nx_request_state(&interrupt_request.base),
                      NX_REQUEST_SETTLED);
            EXPECT_EQ(interrupt_request.base.result, NX_SUCCESS);
            EXPECT_EQ(interrupt_request.base.transferred, length);
            EXPECT_EQ(endpoints[2].request, nullptr);
            EXPECT_EQ(hardware->registers[0x834U / 4U], 0U);
            EXPECT_EQ(hardware->registers[0xB20U / 4U], other_out);
            EXPECT_EQ(endpoints[0].request, nullptr);
            EXPECT_EQ(endpoints[1].request, nullptr);
            EXPECT_FALSE(hardware->reserved_ep0_pid_command);
        }
    }
}
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

extern "C" const nx_usb_activation_ops_t nx_usb_fs_activation_ops;

class UsbSocActivation : public UsbSoc {
  protected:
    const nx_usb_activation_port_t activation = {&nx_usb_fs_activation_ops,
                                                 &controller};
    nx_result_t Activate(uint8_t address, bool active) {
        return nx_usb_activation_set(&activation, controller.snapshot.epoch,
                                     address, active);
    }
};

TEST_F(UsbSocActivation, ResetLeafKeepsActiveUntilPlatformDrainsAndWithdraws) {
    ASSERT_EQ(Activate(0x81U, true), NX_SUCCESS);
    ASSERT_EQ(Activate(0x01U, true), NX_SUCCESS);
    nx_usb_endpoint_port_t in = {&nx_usb_fs_endpoint_ops, &endpoints[2]};
    ASSERT_EQ(nx_usb_in_prepare(&interrupt_request, interrupt_bytes, 1U,
                                NX_USB_DATA, 0U, 200U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &interrupt_request), NX_SUCCESS);
    const auto before_reset_mask = hardware->registers[0x81CU / 4U];
    hardware->hold_disable = true;
    /* This hardware leaf publishes reset without clearing active. It proves
     * platform behavior without asserting that GD silicon clears EPACT. */
    hardware->registers[0x14U / 4U] |= 1U << 12U;
    ASSERT_NE(hardware->registers[0x920U / 4U] & (1U << 15U), 0U);
    nx_usb_controller_irq(&controller);
    EXPECT_TRUE(controller.reset_pending);
    EXPECT_EQ(hardware->registers[0x81CU / 4U], before_reset_mask);
    EXPECT_NE(hardware->registers[0x920U / 4U] & (1U << 15U), 0U);
    EXPECT_EQ(endpoints[2].request, &interrupt_request);
    EXPECT_EQ(nx_request_state(&interrupt_request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(clock.consumers, 1U);
    const auto writes = hardware->mmio_writes;
    EXPECT_EQ(Activate(0x81U, true), NX_ERROR_STATE);
    EXPECT_EQ(hardware->mmio_writes, writes);
    hardware->hold_disable = false;
    ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&interrupt_request.base), NX_REQUEST_SETTLED);
    ASSERT_FALSE(controller.reset_pending);
    EXPECT_EQ(hardware->registers[0x920U / 4U] & (1U << 15U), 0U);
    EXPECT_EQ(hardware->registers[0xB20U / 4U] & (1U << 15U), 0U);
    EXPECT_EQ(hardware->registers[0x81CU / 4U], 0x00010001U);
    EXPECT_NE(hardware->registers[0x900U / 4U] & (1U << 15U), 0U);
    EXPECT_NE(hardware->registers[0xB00U / 4U] & (1U << 15U), 0U);
    ASSERT_EQ(nx_usb_in_prepare(&interrupt_request, interrupt_bytes, 1U,
                                NX_USB_DATA, 0U, 200U),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&in, &interrupt_request), NX_ERROR_STATE);
    EXPECT_EQ(endpoints[2].request, nullptr);
    EXPECT_EQ(nx_request_state(&interrupt_request.base), NX_REQUEST_READY);
    ASSERT_EQ(Activate(0x81U, true), NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &interrupt_request), NX_SUCCESS);
    /* Publish the same independent ACK facts as the existing SDK packet test.
     */
    hardware->registers[0x920U / 4U] &= ~(1U << 31U);
    hardware->registers[0x928U / 4U] |= 1U;
    hardware->registers[0x938U / 4U] = endpoints[2].fifo_words;
    hardware->tx_count[1] = 0U;
    nx_usb_controller_irq(&controller);
    EXPECT_EQ(nx_request_state(&interrupt_request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(interrupt_request.base.result, NX_SUCCESS);
}

TEST_F(UsbSocActivation, CorruptActiveGeometryClosesWithoutReplayingData0) {
    ASSERT_EQ(Activate(0x81U, true), NX_SUCCESS);
    hardware->registers[0x920U / 4U] =
        (hardware->registers[0x920U / 4U] & ~0x7FFU) | 63U | (1U << 16U);
    const auto corrupt = hardware->registers[0x920U / 4U];
    const auto writes = hardware->mmio_writes;
    EXPECT_EQ(Activate(0x81U, true), NX_ERROR_IO);
    EXPECT_EQ(hardware->registers[0x920U / 4U], corrupt);
    EXPECT_EQ(hardware->mmio_writes, writes);
    EXPECT_TRUE(controller.snapshot.closing);
    EXPECT_EQ(clock.consumers, 1U);
    EXPECT_EQ(endpoints[2].request, nullptr);
}

TEST_F(UsbSoc, LatchedRawResetRejectsActiveDirectionsBeforeIrqCollection) {
    for (unsigned index : {2U, 3U}) {
        SCOPED_TRACE(testing::Message() << "endpoint index=" << index);
        ASSERT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
        ASSERT_EQ(DEVICE_INIT(&controller, &clock), NX_SUCCESS);
        ASSERT_EQ(ActivateSelected(), NX_SUCCESS);
        const auto& selected = endpoints[index];
        const nx_usb_endpoint_port_t endpoint = {&nx_usb_fs_endpoint_ops,
                                                 &endpoints[index]};
        ASSERT_EQ(selected.in
                      ? nx_usb_in_prepare(&interrupt_request, interrupt_bytes,
                                          1U, NX_USB_DATA, 0U, 200U)
                      : nx_usb_out_prepare(&interrupt_request, interrupt_bytes,
                                           1U, NX_USB_DATA, 0U, 200U),
                  NX_SUCCESS);
        const uint32_t offset =
            (selected.in ? 0x900U : 0xB00U) + 32U * selected.number;
        const auto control = hardware->registers[offset / 4U];
        ASSERT_NE(control & (1U << 15U), 0U);
        ASSERT_EQ(endpoints[index].request, nullptr);
        ASSERT_FALSE(controller.reset_pending);
        const auto epoch = controller.snapshot.epoch;
        const auto size = hardware->registers[(offset + 16U) / 4U];
        const auto mask = hardware->registers[0x81CU / 4U];
        /* This shared SDK hardware leaf retains active after the raw reset
         * latch. Do not infer ST or GD silicon side effects from the model. */
        hardware->registers[0x14U / 4U] |= 1U << 12U;
        const auto writes = hardware->mmio_writes;
        const auto result =
            nx_usb_endpoint_submit(&endpoint, &interrupt_request);
        EXPECT_EQ(result, NX_ERROR_STATE);
        EXPECT_EQ(nx_request_state(&interrupt_request.base), NX_REQUEST_READY);
        EXPECT_EQ(endpoints[index].request, nullptr);
        EXPECT_EQ(hardware->mmio_writes, writes);
        EXPECT_EQ(hardware->registers[offset / 4U], control);
        EXPECT_EQ(hardware->registers[(offset + 16U) / 4U], size);
        EXPECT_EQ(hardware->registers[0x81CU / 4U], mask);
        EXPECT_EQ(hardware->tx_count[selected.number], 0U);
        EXPECT_NE(hardware->registers[0x14U / 4U] & (1U << 12U), 0U);
        EXPECT_EQ(controller.snapshot.epoch, epoch);
        EXPECT_FALSE(controller.reset_pending);
        EXPECT_FALSE(controller.snapshot.closing);
        EXPECT_EQ(clock.consumers, 1U);
        /* An accepted RED request remains fixture-owned through real stop. */
        ASSERT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
        if (result == NX_SUCCESS) {
            ASSERT_EQ(nx_request_state(&interrupt_request.base),
                      NX_REQUEST_SETTLED);
        }
    }
}
