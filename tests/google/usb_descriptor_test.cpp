/**
 * \file            usb_descriptor_test.cpp
 * \brief           Exact profile membership and bounded descriptor contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_descriptors.h"
#include <cstring>
#include <gtest/gtest.h>

namespace {
nx_result_t Packet(void*, nx_usb_request_t*) {
    return NX_ERROR_UNSUPPORTED;
}
nx_result_t Stall(void*, bool) {
    return NX_ERROR_UNSUPPORTED;
}
nx_result_t Poll(void*) {
    return NX_ERROR_UNSUPPORTED;
}
nx_result_t State(void*, nx_usb_device_state_t*) {
    return NX_ERROR_UNSUPPORTED;
}
nx_result_t RejectSetup(void*, uint64_t) {
    return NX_ERROR_UNSUPPORTED;
}
const nx_usb_setup_decision_ops_t setup_ops{RejectSetup};
const nx_usb_endpoint_ops_t ep_ops{Packet, Packet, Stall};
const nx_usb_device_ops_t dev_ops{Poll,    nullptr, State,
                                  nullptr, nullptr, nullptr};
class UsbDescriptors : public testing::Test {
  protected:
    int storage[5]{};
    nx_usb_endpoint_port_t ports[4] = {{&ep_ops, &storage[0]},
                                       {&ep_ops, &storage[1]},
                                       {&ep_ops, &storage[2]},
                                       {&ep_ops, &storage[3]}};
    nx_usb_device_port_t controller{&dev_ops, &storage[4]};
    nx_usb_endpoint_profile_t endpoints[4] = {
        {&ports[0], 64U, 0U, true, NX_USB_PROFILE_CONTROL},
        {&ports[1], 64U, 0U, false, NX_USB_PROFILE_CONTROL},
        {&ports[2], 32U, 1U, true, NX_USB_PROFILE_BULK},
        {&ports[3], 64U, 1U, false, NX_USB_PROFILE_BULK}};
    nx_usb_device_profile_t profile{&controller,
                                    endpoints,
                                    4U,
                                    48000000U,
                                    320U,
                                    96U,
                                    4U,
                                    NX_USB_PROFILE_DEVICE,
                                    NX_USB_PROFILE_FULL_SPEED,
                                    NX_USB_PROFILE_INTERNAL_FS,
                                    NX_USB_PROFILE_PIO,
                                    {&setup_ops, &storage[4]}};
    uint8_t configuration[32] = {9,  2, 32, 0,   1, 1, 0, 128, 50, 9,   4,
                                 0,  0, 2,  255, 0, 0, 0, 7,   5,  129, 2,
                                 32, 0, 0,  7,   5, 1, 2, 64,  0,  0};
    nx_result_t Validate() {
        nx_usb_descriptor_view_t view{configuration, sizeof(configuration)};
        return nx_usb_descriptor_validate(&profile, &view);
    }
};
TEST_F(UsbDescriptors, ProfileBindsDirectionAndSelectedMpsToTheActualFace) {
    ASSERT_EQ(nx_usb_profile_validate(&profile), NX_SUCCESS);
    auto* selected = nx_usb_profile_endpoint(&profile, 1U, true);
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->port, &ports[2]);
    EXPECT_EQ(selected->max_packet, 32U);
    EXPECT_EQ(nx_usb_profile_endpoint(&profile, 2U, false), nullptr);
    endpoints[3].number = 0U;
    EXPECT_EQ(nx_usb_profile_validate(&profile), NX_ERROR_INVALID);
}
TEST_F(UsbDescriptors,
       CompleteConfigurationUsesSelectedEndpointsAndPacketSizes) {
    EXPECT_EQ(Validate(), NX_SUCCESS);
    configuration[22] =
        64U; /* Chip can do 64, but this selected IN port is 32. */
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    configuration[22] = 32U;
    configuration[20] = 130U; /* An unselected direction has no usable port. */
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
}
TEST_F(UsbDescriptors, TruncatedZeroLengthAndDuplicateRecordsCannotPass) {
    configuration[18] = 0U;
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    configuration[18] = 7U;
    configuration[27] = 129U;
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    configuration[27] = 1U;
    configuration[2] = 33U;
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    nx_usb_descriptor_view_t short_view{configuration, 31U};
    EXPECT_EQ(nx_usb_descriptor_validate(&profile, &short_view),
              NX_ERROR_INVALID);
}
TEST_F(UsbDescriptors, UnimplementedModesAndAlternatePlansAreExplicit) {
    configuration[21] =
        3U; /* Interrupt type mismatches this selected bulk port. */
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    configuration[21] = 2U;
    configuration[12] = 1U;
    EXPECT_EQ(Validate(), NX_ERROR_UNSUPPORTED);
    configuration[12] = 0U;
    configuration[7] |= 32U; /* No remote-wakeup operation is supplied. */
    EXPECT_EQ(Validate(), NX_ERROR_UNSUPPORTED);
}
TEST_F(UsbDescriptors, DeviceMpsAndStringBoundsDoNotInventProductPolicy) {
    uint8_t device[18] = {18, 1, 0, 2, 0, 0, 0, 64, 0,
                          0,  0, 0, 0, 1, 0, 0, 0,  1};
    nx_usb_descriptor_view_t view{device, sizeof(device)};
    EXPECT_EQ(nx_usb_descriptor_validate(&profile, &view), NX_SUCCESS);
    device[7] = 8U;
    EXPECT_EQ(nx_usb_descriptor_validate(&profile, &view), NX_ERROR_INVALID);
    uint8_t string[] = {4, 3, 9, 4};
    view = {string, sizeof(string)};
    EXPECT_EQ(nx_usb_descriptor_validate(&profile, &view), NX_SUCCESS);
    string[0] = 3U;
    EXPECT_EQ(nx_usb_descriptor_validate(&profile, &view), NX_ERROR_INVALID);
}

TEST_F(UsbDescriptors, InterruptInProfileAcceptsAllFullSpeedPacketBounds) {
    endpoints[2].transfer = NX_USB_PROFILE_INTERRUPT;
    for (uint16_t maximum : {uint16_t{1}, uint16_t{10}, uint16_t{64}}) {
        endpoints[2].max_packet = maximum;
        EXPECT_EQ(nx_usb_profile_validate(&profile), NX_SUCCESS);
    }
    for (uint16_t maximum : {uint16_t{0}, uint16_t{65}}) {
        endpoints[2].max_packet = maximum;
        EXPECT_EQ(nx_usb_profile_validate(&profile), NX_ERROR_INVALID);
    }
    endpoints[2].max_packet = 10U;
    endpoints[2].in = false;
    endpoints[3].number = 2U;
    EXPECT_EQ(nx_usb_profile_validate(&profile), NX_ERROR_INVALID);
    endpoints[2].in = true;
    endpoints[3].number = 1U;
    endpoints[0].transfer = NX_USB_PROFILE_INTERRUPT;
    EXPECT_EQ(nx_usb_profile_validate(&profile), NX_ERROR_INVALID);
}
TEST_F(UsbDescriptors, InterruptDescriptorOwnsItsLinearFullSpeedPollInterval) {
    endpoints[2].transfer = NX_USB_PROFILE_INTERRUPT;
    endpoints[2].max_packet = 10U;
    configuration[21] = 3U;
    configuration[22] = 10U;
    for (uint8_t interval : {uint8_t{1}, uint8_t{16}, uint8_t{255}}) {
        configuration[24] = interval;
        EXPECT_EQ(Validate(), NX_SUCCESS);
    }
    configuration[24] = 0U;
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    configuration[24] = 16U;
    configuration[22] = 11U;
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    configuration[22] = 10U;
    configuration[21] = 2U;
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    configuration[21] = 7U;
    EXPECT_EQ(Validate(), NX_ERROR_INVALID);
    configuration[21] = 3U;
    configuration[28] = 3U;
    configuration[31] = 16U;
    EXPECT_EQ(Validate(), NX_ERROR_UNSUPPORTED);
}

TEST_F(UsbDescriptors, UnalignedProfileGraphIsRejectedBeforeTypedReads) {
    alignas(nx_usb_device_profile_t) uint8_t
        profile_raw[sizeof(profile) + alignof(nx_usb_device_profile_t)]{};
    std::memcpy(profile_raw + 1U, &profile, sizeof(profile));
    auto* unaligned_profile =
        reinterpret_cast<const nx_usb_device_profile_t*>(profile_raw + 1U);
    EXPECT_EQ(nx_usb_profile_validate(unaligned_profile), NX_ERROR_INVALID);
    EXPECT_EQ(nx_usb_profile_endpoint(unaligned_profile, 1U, true), nullptr);
    alignas(nx_usb_endpoint_profile_t) uint8_t
        endpoint_raw[sizeof(endpoints) + alignof(nx_usb_endpoint_profile_t)]{};
    std::memcpy(endpoint_raw + 1U, endpoints, sizeof(endpoints));
    profile.endpoints =
        reinterpret_cast<const nx_usb_endpoint_profile_t*>(endpoint_raw + 1U);
    EXPECT_EQ(nx_usb_profile_validate(&profile), NX_ERROR_INVALID);
    EXPECT_EQ(nx_usb_profile_endpoint(&profile, 1U, true), nullptr);
    profile.endpoints = endpoints;
    alignas(nx_usb_device_port_t)
        uint8_t port_raw[sizeof(controller) + alignof(nx_usb_device_port_t)]{};
    std::memcpy(port_raw + 1U, &controller, sizeof(controller));
    profile.port = reinterpret_cast<const nx_usb_device_port_t*>(port_raw + 1U);
    EXPECT_EQ(nx_usb_profile_validate(&profile), NX_ERROR_INVALID);
    profile.port = &controller;
    alignas(nx_usb_endpoint_ops_t)
        uint8_t ops_raw[sizeof(ep_ops) + alignof(nx_usb_endpoint_ops_t)]{};
    std::memcpy(ops_raw + 1U, &ep_ops, sizeof(ep_ops));
    nx_usb_endpoint_port_t changed = ports[0];
    changed.ops = reinterpret_cast<const nx_usb_endpoint_ops_t*>(ops_raw + 1U);
    endpoints[0].port = &changed;
    EXPECT_EQ(nx_usb_profile_validate(&profile), NX_ERROR_INVALID);
    endpoints[0].port = &ports[0];
    EXPECT_EQ(nx_usb_profile_validate(&profile), NX_SUCCESS);
}

TEST_F(UsbDescriptors, UnalignedDescriptorMetadataRejectsValidReadableBytes) {
    nx_usb_descriptor_view_t view{configuration, sizeof(configuration)};
    alignas(nx_usb_descriptor_view_t)
        uint8_t raw[sizeof(view) + alignof(nx_usb_descriptor_view_t)]{};
    std::memcpy(raw + 1U, &view, sizeof(view));
    uint8_t before[sizeof(raw)];
    std::memcpy(before, raw, sizeof(raw));
    auto* unaligned =
        reinterpret_cast<const nx_usb_descriptor_view_t*>(raw + 1U);
    EXPECT_EQ(nx_usb_descriptor_validate(&profile, unaligned),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(raw, before, sizeof(raw)), 0);
    EXPECT_EQ(nx_usb_descriptor_validate(&profile, &view), NX_SUCCESS);
}
} /* namespace */
