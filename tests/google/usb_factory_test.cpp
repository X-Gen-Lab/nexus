/**
 * \file            usb_factory_test.cpp
 * \brief           Actual generated USB faces and cold retained SDK ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "usb_factory_activation_test.h"
#include "usb_factory_fixture.h"
#ifdef NX_USB_FACTORY_STM32
extern "C" {
#include "stm32f407_usb_cold.h"
}
#else
extern "C" {
#include "gd32f470_usb_cold.h"
}
#endif

namespace {
#ifdef NX_USB_FACTORY_STM32
using UsbColdConfig = nx_stm32_usb_cold_config_t;
using UsbClockState = nx_stm32_usb_clock_state_t;
using UsbPinConfig = nx_stm32_platform_pin_config_t;
using UsbEndpointPlan = nx_stm32_usb_endpoint_config_t;
constexpr auto UsbColdPrepare = nx_stm32_usb_cold_prepare;
constexpr auto UsbColdClose = nx_stm32_usb_cold_close;
#else
using UsbColdConfig = nx_gd32_usb_cold_config_t;
using UsbClockState = nx_gd32_usb_clock_state_t;
using UsbPinConfig = nx_gd32_platform_pin_config_t;
using UsbEndpointPlan = nx_gd32_usb_endpoint_config_t;
constexpr auto UsbColdPrepare = nx_gd32_usb_cold_prepare;
constexpr auto UsbColdClose = nx_gd32_usb_cold_close;
#endif
} /* namespace */

TEST_F(UsbFactory, BasePreflightRejectsBeforeClockPinAndControllerEffects) {
    const auto* bytes = static_cast<const uint8_t*>(clocks);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, Preflight()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_CALL(ports, Stop()).Times(0);
    EXPECT_CALL(ports, EnableClock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Capture(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    const nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_BUSY, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
    EXPECT_FALSE(state->snapshot.started);
    EXPECT_FALSE(state->clock_retained);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(UsbFactory, PureFactoryDoesNotStartAndControllerIsNotAnEndpoint) {
    EXPECT_CALL(ports, Start()).Times(0);
    EXPECT_EQ(nx_factory_usb_device(NX_USB_DEVICE_ID_FS), face);
    EXPECT_EQ(nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_BULK_IN), bulk);
    EXPECT_EQ(nx_factory_usb_device(NX_USB_DEVICE_ID_COUNT), nullptr);
    EXPECT_EQ(nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_COUNT), nullptr);
    EXPECT_NE(face->context, bulk->context);
    EXPECT_EQ(face->ops, &nx_usb_fs_device_ops);
    EXPECT_EQ(bulk->ops, &nx_usb_fs_endpoint_ops);
    EXPECT_FALSE(state->snapshot.started);
}
TEST_F(UsbFactory, SelectedUsbProfileIsPureExactAndDistinctFromGenericCaps) {
    EXPECT_CALL(ports, Start()).Times(0);
    const nx_usb_device_profile_t* profile =
        nx_factory_usb_profile(NX_USB_DEVICE_ID_FS);
    ASSERT_NE(profile, nullptr);
    EXPECT_EQ(profile, nx_factory_usb_profile(NX_USB_DEVICE_ID_FS));
    EXPECT_EQ(nx_factory_usb_profile(NX_USB_DEVICE_ID_COUNT), nullptr);
    EXPECT_EQ(nx_usb_profile_validate(profile), NX_SUCCESS);
    EXPECT_EQ(profile->port, face);
    EXPECT_EQ(profile->setup_decision.ops, &nx_usb_fs_setup_decision_ops);
    EXPECT_EQ(profile->setup_decision.context, face->context);
    EXPECT_EQ(profile->endpoint_count, 4U);
    EXPECT_EQ(profile->phy_clock_hz, 48000000U);
    EXPECT_EQ(profile->fifo_words, 320U);
    EXPECT_EQ(profile->rx_fifo_words, 96U);
    EXPECT_EQ(profile->endpoint_numbers, 4U);
    EXPECT_EQ(profile->role, NX_USB_PROFILE_DEVICE);
    EXPECT_EQ(profile->speed, NX_USB_PROFILE_FULL_SPEED);
    EXPECT_EQ(profile->phy, NX_USB_PROFILE_INTERNAL_FS);
    EXPECT_EQ(profile->engine, NX_USB_PROFILE_PIO);
    const nx_usb_endpoint_profile_t* selected =
        nx_usb_profile_endpoint(profile, 1U, true);
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(selected->port, bulk);
    EXPECT_EQ(selected->max_packet, 64U);
    EXPECT_EQ(selected->transfer, NX_USB_PROFILE_BULK);
    EXPECT_EQ(nx_usb_profile_endpoint(profile, 2U, true), nullptr);
    EXPECT_FALSE(state->snapshot.started);
}
TEST_F(UsbFactory, CapabilityDescribesExactlyOneSelectedPioPacket) {
    const nx_factory_io_capability_t* capability =
        nx_factory_usb_endpoint_capability(NX_USB_ENDPOINT_ID_BULK_IN);
    ASSERT_NE(capability, nullptr);
    EXPECT_EQ(capability->max_transfer_units, 64U);
    EXPECT_EQ(capability->max_messages, 1U);
    EXPECT_EQ(capability->unit_bytes, 1U);
    EXPECT_EQ(capability->features, NX_FACTORY_CAP_IRQ);
}
TEST_F(UsbFactory, RealSelectedDirectionsFifoPlanAndClockConsumerAreBound) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(state->registers, 0x50000000U);
    EXPECT_EQ(state->endpoint_count, 4U);
    EXPECT_EQ(state->capacity, 8U);
    EXPECT_EQ(state->rx_fifo_words, 96U);
    EXPECT_EQ(state->irq.irq_number, 67);
    EXPECT_TRUE(state->clock_retained);
    EXPECT_EQ(hardware->registers[0x28U / 4U], (32U << 16U) | 96U);
    EXPECT_EQ(hardware->registers[0x104U / 4U], (32U << 16U) | 128U);
    EXPECT_EQ(static_cast<nx_usb_endpoint_t*>(bulk->context)->controller,
              state);
}
TEST_F(UsbFactory, FailedColdDcdIsCountedAndRetainsClockAndPinsUntilProof) {
    hardware->hold_reset = true;
    hardware->irq_enabled = hardware->hold_irq_disable = true;
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_ERROR_BUSY);
    EXPECT_NE(result.remaining_effects, 0U);
    EXPECT_TRUE(state->clock_retained);
    testing::Mock::VerifyAndClearExpectations(&ports);
    hardware->hold_reset = hardware->hold_irq_disable = false;
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(2);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_FALSE(state->clock_retained);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(UsbFactory, FailedClockReleaseKeepsBothPinLeases) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    s_hold_clock_stop = true;
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_FALSE(state->clock_retained);
    testing::Mock::VerifyAndClearExpectations(&ports);
    s_hold_clock_stop = false;
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(2);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(UsbFactory, FailedPinReadbackRetainsOnlyTheUnreleasedLease) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    s_hold_restore = true;
    EXPECT_CALL(ports, Restore(0U, 12U, 0x123U)).Times(1);
    EXPECT_CALL(ports, Restore(0U, 11U, _)).Times(0);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    testing::Mock::VerifyAndClearExpectations(&ports);
    s_hold_restore = false;
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(2);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    testing::Mock::VerifyAndClearExpectations(&ports);
}
TEST_F(UsbFactory, PartialPinConfigurationRestoresExactlyCapturedPins) {
    EXPECT_CALL(ports, Configure(0U, 11U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, Configure(0U, 12U)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_CALL(ports, Restore(_, _, 0x123U)).Times(2);
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_IO);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
}
TEST_F(UsbFactory, BusyPioLoanRetainsTheSameRequestUntilDetachAndSettlement) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    const auto* activation = nx_factory_usb_activation(NX_USB_DEVICE_ID_FS);
    ASSERT_NE(activation, nullptr);
    ASSERT_EQ(
        nx_usb_activation_set(activation, state->snapshot.epoch, 0x81U, true),
        NX_SUCCESS);
    nx_request_initialize(&request.base);
    ASSERT_EQ(nx_usb_in_prepare(&request, payload, sizeof(payload), NX_USB_DATA,
                                0U, 10000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(bulk, &request), NX_SUCCESS);
    hardware->hold_disable = true;
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(static_cast<nx_usb_endpoint_t*>(bulk->context)->request,
              &request);
    testing::Mock::VerifyAndClearExpectations(&ports);
    hardware->hold_disable = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}
TEST_F(UsbFactory, SuccessfulDrainAllowsTheSameStaticIdentityToRestart) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    EXPECT_EQ(nx_factory_usb_device(NX_USB_DEVICE_ID_FS), face);
    EXPECT_EQ(nx_factory_usb_endpoint(NX_USB_ENDPOINT_ID_BULK_IN), bulk);
    EXPECT_TRUE(state->clock_retained);
}
TEST_F(UsbFactory, RestartCannotReuseAStaleSetupEpochForSetAddress) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ReceiveAddressSetup();
    const uint64_t old_epoch = state->snapshot.epoch;
    ASSERT_EQ(nx_usb_device_address(face, old_epoch, 7U), NX_SUCCESS);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ReceiveAddressSetup();
    EXPECT_GT(state->snapshot.epoch, old_epoch);
    EXPECT_EQ(nx_usb_device_address(face, old_epoch, 7U), NX_ERROR_STATE);
    EXPECT_EQ(nx_usb_device_address(face, state->snapshot.epoch, 7U),
              NX_SUCCESS);
}
TEST_F(UsbFactory, ExhaustedLifetimeEpochCannotBeReopenedByColdReconstruction) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    const uint64_t original = state->snapshot.epoch;
    state->snapshot.epoch = UINT64_MAX;
    nx_platform_start_result_t result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_INVALID);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(state->snapshot.epoch, UINT64_MAX);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    state->snapshot.epoch = original;
}
#ifdef NX_USB_FACTORY_STM32
TEST_F(UsbFactory, RefusedOwnedGpioGateWithdrawalRetainsPlatformUntilReadback) {
    ASSERT_EQ(nx_platform_start().primary, NX_SUCCESS);
    ASSERT_NE(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN, 0U);
    s_hold_gpio_stop = true;
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_IO);
    EXPECT_NE(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN, 0U);
    EXPECT_EQ(nx_platform_start().primary, NX_ERROR_STATE);
    s_hold_gpio_stop = false;
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN, 0U);
}
#endif

TEST_F(UsbFactory, CColdRejectsMissingStaticConfigBeforeHardwareOrStorage) {
    const auto* bytes = static_cast<const uint8_t*>(clocks);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    const nx_usb_controller_t prior = *state;
    uint8_t leases = 0U;
    EXPECT_CALL(ports, EnableClock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Capture(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, UsbColdPrepare(state, nullptr, &leases));
    EXPECT_EQ(NX_ERROR_INVALID, UsbColdClose(state, nullptr, &leases));
    EXPECT_EQ(0, std::memcmp(&prior, state, sizeof(prior)));
    EXPECT_EQ(0U, leases);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
}

TEST_F(UsbFactory, ExhaustedCColdEpochRejectsBeforeStorageOrHardwareChanges) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    nx_usb_controller_t selected = *state;
    selected.snapshot.epoch = UINT64_MAX;
    const nx_usb_controller_t prior = selected;
    UsbClockState clock{};
    uint16_t snapshots[2]{};
    uint8_t leases = 0U;
    const UsbPinConfig pins[] = {{GPIOA, &snapshots[0], 0U, 11U},
                                 {GPIOA, &snapshots[1], 0U, 12U}};
    UsbEndpointPlan plan[4]{};
    ASSERT_EQ(4U, selected.endpoint_count);
    for (size_t i = 0U; i < 4U; ++i) {
        const nx_usb_endpoint_t* endpoint = &selected.endpoints[i];
        plan[i] = {endpoint->number,     endpoint->in,
                   endpoint->transfer,   endpoint->max_packet,
                   endpoint->fifo_start, endpoint->fifo_words};
    }
    UsbColdConfig config{};
    config.clock = &clock;
    config.pins = pins;
    config.endpoints = selected.endpoints;
    config.plan = plan;
    config.events = selected.events;
    config.endpoint_count = selected.endpoint_count;
    config.capacity = selected.capacity;
    config.irq_policy = selected.irq_policy;
    config.poll_limit = selected.poll_limit;
    config.drain_timeout_us = selected.drain_timeout_us;
    config.clock_timeout_us = 1000U;
    config.rx_fifo_words = selected.rx_fifo_words;
    config.priority = selected.irq.priority;
    const auto* bytes = static_cast<const uint8_t*>(clocks);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, EnableClock(_)).Times(0);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Capture(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, UsbColdPrepare(&selected, &config, &leases));
    EXPECT_EQ(0, std::memcmp(&prior, &selected, sizeof(prior)));
    EXPECT_EQ(0U, leases);
    EXPECT_EQ(0U, clock.phase);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
    testing::Mock::VerifyAndClearExpectations(&ports);
    EXPECT_EQ(NX_SUCCESS, UsbColdClose(&selected, &config, &leases));
}

TEST_F(UsbFactory, AbsentColdReceiptCannotStopActiveController) {
    ASSERT_EQ(NX_SUCCESS, nx_platform_start().primary);
    const nx_usb_controller_t prior = *state;
    uint16_t snapshots[2]{};
    const UsbPinConfig pins[] = {{GPIOA, &snapshots[0], 0U, 11U},
                                 {GPIOA, &snapshots[1], 0U, 12U}};
    UsbColdConfig config{};
    config.pins = pins;
    config.clock = static_cast<UsbClockState*>(state->clock.context);
    config.clock_timeout_us = 1000U;
    uint8_t leases = 0U;
    const auto* bytes = static_cast<const uint8_t*>(clocks);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_CALL(ports, Configure(_, _)).Times(0);
    EXPECT_CALL(ports, Capture(_, _)).Times(0);
    EXPECT_CALL(ports, Restore(_, _, _)).Times(0);
    EXPECT_EQ(NX_ERROR_BUSY, UsbColdClose(state, &config, &leases));
    EXPECT_EQ(0, std::memcmp(&prior, state, sizeof(prior)));
    EXPECT_EQ(0U, leases);
    EXPECT_EQ(before, std::vector<uint8_t>(bytes, bytes + 0x80000U));
    testing::Mock::VerifyAndClearExpectations(&ports);
}
