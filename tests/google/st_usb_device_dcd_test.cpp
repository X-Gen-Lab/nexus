/**
 * \file            st_usb_device_dcd_test.cpp
 * \brief           Official Core and real Native DCD control-stage boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "usb_device.h"
#include "usb_device_stack.h"
}
#include <gtest/gtest.h>

namespace {
uint8_t dcd_descriptor[18] = {18U, 1U, 0U, 2U, 0U, 0U, 0U, 64U};
uint8_t dcd_config[150] = {9U, 2U, 150U, 0U, 1U, 1U, 0U, 0x80U, 50U};
uint8_t* GetDevice(USBD_SpeedTypeDef, uint16_t* size) {
    *size = sizeof(dcd_descriptor);
    return dcd_descriptor;
}
uint8_t* GetConfig(uint16_t* size) {
    *size = sizeof(dcd_config);
    return dcd_config;
}
uint8_t ClassState(USBD_HandleTypeDef*, uint8_t) {
    return USBD_OK;
}
uint8_t RejectClass(USBD_HandleTypeDef* device, USBD_SetupReqTypedef* setup) {
    USBD_CtlError(device, setup);
    return USBD_FAIL;
}
} /* namespace */
static void RunCoreInput(bool premature) {
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_configure(true, 100U));
    nx_native_usb_device_t model{};
    nx_usb_endpoint_t endpoints[2]{};
    nx_usb_event_t events[4]{};
    auto& controller = model.controller;
    controller.endpoints = endpoints;
    controller.endpoint_count = 2U;
    controller.events = events;
    controller.capacity = 4U;
    controller.rx_fifo_words = 128U;
    controller.poll_limit = 4U;
    controller.drain_timeout_us = 20U;
    controller.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
    controller.irq = {67U, 5U, 3U};
    endpoints[0].in = true;
    endpoints[0].max_packet = endpoints[1].max_packet = 64U;
    endpoints[0].fifo_start = 128U;
    endpoints[0].fifo_words = 32U;
    model.hardware.clock_ready = true;
    ASSERT_EQ(NX_SUCCESS, nx_native_usb_initialize(&model));
    const nx_usb_device_port_t device{&nx_usb_fs_device_ops, &controller};
    const nx_usb_endpoint_port_t ports[2] = {
        {&nx_usb_fs_endpoint_ops, &endpoints[0]},
        {&nx_usb_fs_endpoint_ops, &endpoints[1]}};
    const nx_usb_endpoint_profile_t selected[2] = {
        {&ports[0], 64U, 0U, true, NX_USB_PROFILE_CONTROL},
        {&ports[1], 64U, 0U, false, NX_USB_PROFILE_CONTROL}};
    const nx_usb_device_profile_t profile = {
        &device,
        selected,
        2U,
        48000000U,
        320U,
        128U,
        4U,
        NX_USB_PROFILE_DEVICE,
        NX_USB_PROFILE_FULL_SPEED,
        NX_USB_PROFILE_INTERNAL_FS,
        NX_USB_PROFILE_PIO,
        {&nx_usb_fs_setup_decision_ops, &controller}};
    alignas(4) uint8_t storage[2][64]{};
    nx_st_usb_device_slot_t slots[2]{};
    for (size_t i = 0U; i < 2U; ++i) {
        slots[i].storage = storage[i];
        slots[i].capacity = sizeof(storage[i]);
    }
    const nx_st_usb_device_stack_config_t config{&profile, nullptr, slots,
                                                 2U,       1000U,   nullptr};
    nx_st_usb_device_stack_t stack{};
    USBD_DescriptorsTypeDef descriptors{};
    descriptors.GetDeviceDescriptor = GetDevice;
    USBD_ClassTypeDef callbacks{};
    callbacks.Init = callbacks.DeInit = ClassState;
    callbacks.Setup = RejectClass;
    callbacks.GetFSConfigDescriptor = GetConfig;
    nx_result_t initialized = nx_st_usb_device_stack_initialize(
        &stack, &config, &descriptors, &callbacks);
    EXPECT_EQ(NX_SUCCESS, initialized);
    if (initialized != NX_SUCCESS) {
        EXPECT_EQ(NX_SUCCESS, nx_usb_device_stop(&device));
        return;
    }
    struct Cleanup {
        nx_st_usb_device_stack_t* current;
        ~Cleanup() {
            if (current->config != nullptr) {
                EXPECT_EQ(NX_SUCCESS, nx_st_usb_device_stack_close(current));
            }
        }
    } cleanup{&stack};
    const uint8_t setup[8] = {0x80U, 6U, 0U, 2U, 0U, 0U, 150U, 0U};
    ASSERT_EQ(NX_SUCCESS, nx_native_usb_receive(&model, 0U, setup, 8U, true));
    nx_usb_controller_irq(&controller);
    ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
    if (premature) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_native_usb_receive(&model, 0U, nullptr, 0U, false));
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, false));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        EXPECT_EQ(nullptr, endpoints[1].request);
        ASSERT_NE(nullptr, endpoints[0].request);
        EXPECT_EQ(USBD_EP0_DATA_IN, stack.middleware.ep0_state);
        model.hardware.hold_flush = true;
        const uint8_t next[8] = {0x80U, 6U, 0U, 1U, 0U, 0U, 18U, 0U};
        ASSERT_EQ(NX_SUCCESS,
                  nx_native_usb_receive(&model, 0U, next, 8U, true));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        EXPECT_TRUE(slots[0].pending);
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(21U));
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        EXPECT_EQ(NX_REQUEST_QUARANTINED,
                  nx_request_state(&slots[0].request.base));
        model.hardware.hold_flush = false;
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        ASSERT_NE(nullptr, endpoints[0].request);
        EXPECT_EQ(18U, endpoints[0].request->length);
        EXPECT_EQ(NX_SUCCESS, stack.failure);
        return;
    }
    for (size_t size : {size_t{64U}, size_t{64U}, size_t{22U}}) {
        ASSERT_NE(nullptr, endpoints[0].request);
        EXPECT_EQ(size, endpoints[0].request->length);
        EXPECT_EQ(nullptr, endpoints[1].request);
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, true));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
    }
    ASSERT_NE(nullptr, endpoints[1].request);
    EXPECT_EQ(NX_USB_STATUS, endpoints[1].request->stage);
    EXPECT_EQ(0U, endpoints[1].request->length);
    ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_close(&stack));
}

TEST(StUsbDeviceDcd, CoreDefersStatusOutUntilRealDcdConfirmsAllInPackets) {
    RunCoreInput(false);
}
TEST(StUsbDeviceDcd, PrematureStatusNeedsTheExplicitNextSetupDrainBoundary) {
    RunCoreInput(true);
}
