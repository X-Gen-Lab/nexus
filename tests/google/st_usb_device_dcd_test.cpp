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
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

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
unsigned dcd_tx_sent;
uint8_t DcdTxSent(USBD_HandleTypeDef*) {
    ++dcd_tx_sent;
    return USBD_OK;
}
uint8_t RejectClass(USBD_HandleTypeDef* device, USBD_SetupReqTypedef* setup) {
    USBD_CtlError(device, setup);
    return USBD_FAIL;
}
} /* namespace */
enum class ControlTrace { Complete, Premature, ConcurrentAck, DmaStatusHeld };
static void RunCoreInput(ControlTrace trace, bool reverse_slots) {
    const bool premature = trace != ControlTrace::Complete;
    const bool concurrent_ack = trace == ControlTrace::ConcurrentAck;
    const bool dma = trace == ControlTrace::DmaStatusHeld;
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_configure(true, 100U));
    nx_native_usb_device_t model{};
    nx_usb_endpoint_t directions[2]{};
    auto& input = directions[reverse_slots ? 1U : 0U];
    auto& output = directions[reverse_slots ? 0U : 1U];
    nx_usb_event_t events[4]{};
    nx_usb_dma_storage_t dma_staging[2]{};
    nx_dma_memory_region_t dma_region = {
        0x20008000U, 4096U, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    if (dma) {
        ASSERT_NE(MAP_FAILED,
                  mmap(reinterpret_cast<void*>(dma_region.begin),
                       dma_region.size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0));
        for (size_t i = 0U; i < 2U; ++i)
            dma_staging[i] = {
                reinterpret_cast<uint8_t*>(dma_region.begin + i * 1024U),
                1024U};
    }
    auto& controller = model.controller;
    if (dma) {
        controller.core = NX_USB_CORE_HS_INTERNAL_FS;
        controller.dma = true;
        controller.dma_storage = dma_staging;
        controller.dma_regions = &dma_region;
        controller.dma_region_count = 1U;
        model.hardware.endpoint_numbers = 6U;
    }
    controller.endpoints = directions;
    controller.endpoint_count = 2U;
    controller.events = events;
    controller.capacity = 4U;
    controller.rx_fifo_words = 128U;
    controller.poll_limit = 4U;
    controller.drain_timeout_us = 20U;
    controller.irq_policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
    controller.irq = {static_cast<int16_t>(dma ? 77U : 67U), 5U, 3U};
    input.in = true;
    input.max_packet = output.max_packet = 64U;
    input.fifo_start = 128U;
    input.fifo_words = 32U;
    model.hardware.clock_ready = true;
    ASSERT_EQ(NX_SUCCESS, nx_native_usb_initialize(&model));
    const nx_usb_device_port_t device{&nx_usb_fs_device_ops, &controller};
    const nx_usb_endpoint_port_t ports[2] = {
        {&nx_usb_fs_endpoint_ops, &input}, {&nx_usb_fs_endpoint_ops, &output}};
    const nx_usb_endpoint_profile_t input_profile = {&ports[0], 64U, 0U, true,
                                                     NX_USB_PROFILE_CONTROL};
    const nx_usb_endpoint_profile_t output_profile = {&ports[1], 64U, 0U, false,
                                                      NX_USB_PROFILE_CONTROL};
    const nx_usb_endpoint_profile_t selected[2] = {
        reverse_slots ? output_profile : input_profile,
        reverse_slots ? input_profile : output_profile};
    const nx_usb_device_profile_t profile = {
        &device,
        selected,
        2U,
        48000000U,
        dma ? uint16_t{1024U} : uint16_t{320U},
        128U,
        dma ? uint8_t{6U} : uint8_t{4U},
        NX_USB_PROFILE_DEVICE,
        NX_USB_PROFILE_FULL_SPEED,
        NX_USB_PROFILE_INTERNAL_FS,
        dma ? NX_USB_PROFILE_AHB_DMA : NX_USB_PROFILE_PIO,
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
    callbacks.EP0_TxSent = DcdTxSent;
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
        nx_native_usb_hardware_t* hardware;
        bool dma;
        ~Cleanup() {
            hardware->refused_dma_address_offset = 0U;
            hardware->hold_flush = hardware->hold_disable = false;
            hardware->registers[0x10U / 4U] |= 1U << 31U;
            if (current->config != nullptr) {
                EXPECT_EQ(NX_SUCCESS, nx_st_usb_device_stack_close(current));
            }
            if (dma) {
                EXPECT_EQ(0,
                          munmap(reinterpret_cast<void*>(0x20008000U), 4096U));
            }
        }
    } cleanup{&stack, &model.hardware, dma};
    auto deliver_setup = [&](const uint8_t* command) {
        if (dma) {
            std::memcpy(dma_staging[reverse_slots ? 0U : 1U].bytes, command,
                        8U);
            model.hardware.registers[0xB00U / 4U] &= ~(1U << 31U);
            model.hardware.registers[0xB08U / 4U] |= 8U;
        } else {
            ASSERT_EQ(NX_SUCCESS,
                      nx_native_usb_receive(&model, 0U, command, 8U, true));
        }
        nx_usb_controller_irq(&controller);
    };
    /* Establish CONFIGURED through real standard-request STATUS ACKs, so a
     * false class EP0_TxSent callback would be observable in this trace. */
    const uint8_t configuration[2][8] = {{0U, 5U, 1U, 0U, 0U, 0U, 0U, 0U},
                                         {0U, 9U, 1U, 0U, 0U, 0U, 0U, 0U}};
    for (const auto& command : configuration) {
        ASSERT_NO_FATAL_FAILURE(deliver_setup(command));
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        ASSERT_NE(nullptr, input.request);
        ASSERT_EQ(NX_USB_STATUS, input.request->stage);
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, true));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
    }
    ASSERT_EQ(USBD_STATE_CONFIGURED, stack.middleware.dev_state);
    dcd_tx_sent = 0U;
    auto& input_slot = slots[reverse_slots ? 1U : 0U];
    auto& output_slot = slots[reverse_slots ? 0U : 1U];
    const uint8_t setup[8] = {0x80U, 6U, 0U, 2U, 0U, 0U, 150U, 0U};
    ASSERT_NO_FATAL_FAILURE(deliver_setup(setup));
    ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
    ASSERT_NE(nullptr, output.request);
    ASSERT_EQ(NX_USB_STATUS, output.request->stage);
    ASSERT_EQ(0U, output.request->length);
    if (dma) {
        const uint64_t epoch = controller.snapshot.epoch;
        model.hardware.refused_dma_address_offset = 0xB14U;
        model.hardware.registers[0xB10U / 4U] = (3U << 29U) | (1U << 19U) | 64U;
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, true));
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, false));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_REQUEST_SETTLED,
                  nx_request_state(&input_slot.request.base));
        ASSERT_NE(NX_REQUEST_SETTLED,
                  nx_request_state(&output_slot.request.base));
        ASSERT_FALSE(controller.snapshot.control_pending);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        EXPECT_EQ(NX_SUCCESS, stack.failure);
        EXPECT_TRUE(input_slot.pending);
        EXPECT_TRUE(output_slot.pending);
        EXPECT_EQ(0U, dcd_tx_sent);
        model.hardware.refused_dma_address_offset = 0U;
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        EXPECT_FALSE(input_slot.pending);
        EXPECT_FALSE(output_slot.pending);
        EXPECT_EQ(nullptr, input.request);
        EXPECT_EQ(nullptr, output.request);
        EXPECT_EQ(USBD_EP0_IDLE, stack.middleware.ep0_state);
        EXPECT_EQ(epoch, controller.snapshot.epoch);
        EXPECT_EQ(0U, dcd_tx_sent);
        EXPECT_EQ(NX_SUCCESS, stack.failure);
        return;
    }
    if (premature && concurrent_ack) {
        const uint64_t epoch = controller.snapshot.epoch;
        ASSERT_EQ(NX_SUCCESS,
                  nx_native_usb_receive(&model, 0U, nullptr, 0U, false));
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, true));
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, false));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        EXPECT_EQ(nullptr, input.request);
        EXPECT_EQ(nullptr, output.request);
        EXPECT_FALSE(input_slot.busy);
        EXPECT_FALSE(output_slot.busy);
        EXPECT_FALSE(controller.snapshot.control_pending);
        EXPECT_EQ(USBD_EP0_IDLE, stack.middleware.ep0_state);
        EXPECT_EQ(epoch, controller.snapshot.epoch);
        EXPECT_EQ(0U, dcd_tx_sent);
        EXPECT_EQ(NX_SUCCESS, stack.failure);
        return;
    }
    if (premature) {
        const uint64_t epoch = controller.snapshot.epoch;
        const size_t first_packet_words = model.hardware.tx_count[0];
        model.hardware.hold_flush = true;
        ASSERT_EQ(NX_SUCCESS,
                  nx_native_usb_receive(&model, 0U, nullptr, 0U, false));
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, false));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        ASSERT_NE(nullptr, input.request);
        EXPECT_TRUE(input_slot.pending);
        EXPECT_TRUE(output_slot.pending);
        EXPECT_EQ(first_packet_words, model.hardware.tx_count[0]);
        EXPECT_EQ(0U, dcd_tx_sent);
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(21U));
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        EXPECT_EQ(NX_REQUEST_QUARANTINED,
                  nx_request_state(&input_slot.request.base));
        EXPECT_TRUE(output_slot.pending);
        EXPECT_EQ(epoch, controller.snapshot.epoch);
        model.hardware.hold_flush = false;
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        EXPECT_EQ(nullptr, input.request);
        EXPECT_EQ(nullptr, output.request);
        EXPECT_FALSE(input_slot.pending);
        EXPECT_FALSE(output_slot.pending);
        EXPECT_FALSE(input_slot.busy);
        EXPECT_FALSE(output_slot.busy);
        EXPECT_FALSE(controller.snapshot.control_pending);
        EXPECT_EQ(USBD_EP0_IDLE, stack.middleware.ep0_state);
        EXPECT_EQ(epoch, controller.snapshot.epoch);
        EXPECT_EQ(0U, dcd_tx_sent);
        EXPECT_EQ(NX_SUCCESS, stack.failure);
        const uint8_t next[8] = {0x80U, 6U, 0U, 1U, 0U, 0U, 18U, 0U};
        ASSERT_EQ(NX_SUCCESS,
                  nx_native_usb_receive(&model, 0U, next, 8U, true));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
        ASSERT_NE(nullptr, input.request);
        EXPECT_EQ(18U, input.request->length);
        EXPECT_EQ(NX_SUCCESS, stack.failure);
        return;
    }
    for (size_t size : {size_t{64U}, size_t{64U}, size_t{22U}}) {
        ASSERT_NE(nullptr, input.request);
        EXPECT_EQ(size, input.request->length);
        ASSERT_NE(nullptr, output.request);
        EXPECT_EQ(NX_USB_STATUS, output.request->stage);
        ASSERT_EQ(NX_SUCCESS, nx_native_usb_complete(&model, 0U, true));
        nx_usb_controller_irq(&controller);
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
    }
    ASSERT_NE(nullptr, output.request);
    EXPECT_EQ(NX_USB_STATUS, output.request->stage);
    EXPECT_EQ(0U, output.request->length);
    EXPECT_EQ(1U, dcd_tx_sent);
    ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_close(&stack));
}

class StUsbDeviceDcd : public testing::Test,
                       public testing::WithParamInterface<bool> {};
TEST_P(StUsbDeviceDcd, CoreArmsStatusOutBeforeExposingItsFirstDataPacket) {
    RunCoreInput(ControlTrace::Complete, GetParam());
}
TEST_P(StUsbDeviceDcd, PrematureStatusJoinsBothDirectionsWithoutAnotherSetup) {
    RunCoreInput(ControlTrace::Premature, GetParam());
}
TEST_P(StUsbDeviceDcd, SameSnapshotDataAckCannotLaunchAnotherPacket) {
    RunCoreInput(ControlTrace::ConcurrentAck, GetParam());
}
TEST_P(StUsbDeviceDcd, DmaWireStatusCannotBecomeAStickyCoreSubmissionFailure) {
    RunCoreInput(ControlTrace::DmaStatusHeld, GetParam());
}
INSTANTIATE_TEST_SUITE_P(EitherSlotOrder, StUsbDeviceDcd,
                         testing::Values(false, true));
