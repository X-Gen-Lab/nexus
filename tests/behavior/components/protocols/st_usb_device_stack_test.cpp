/**
 * \file            st_usb_device_stack_test.cpp
 * \brief           Real ST Device Core and mocked Nexus packet ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "usb_device_stack.h"
}
#include <array>
#include <cstring>
#include <deque>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace {
uint8_t device_descriptor[18] = {18U, 1U, 0U, 2U, 0U, 0U, 0U, 64U};
uint8_t configuration_descriptor[150] = {9U, 2U,    150U, 0U, 1U, 1U,
                                         0U, 0x80U, 50U,  9U, 4U};
uint8_t* DeviceDescriptor(USBD_SpeedTypeDef, uint16_t* length) {
    *length = sizeof(device_descriptor);
    return device_descriptor;
}
uint8_t* ConfigurationDescriptor(uint16_t* length) {
    *length = sizeof(configuration_descriptor);
    return configuration_descriptor;
}
uint8_t high_configuration_descriptor[9] = {9U, 2U, 9U,    0U, 0U,
                                            1U, 0U, 0x80U, 50U};
uint8_t* HighConfigurationDescriptor(uint16_t* length) {
    *length = sizeof(high_configuration_descriptor);
    return high_configuration_descriptor;
}
uint8_t qualifier_descriptor[10] = {10U, 6U, 0U, 2U, 0U, 0U, 0U, 64U, 1U, 0U};
uint8_t other_speed_descriptor[9] = {9U, 7U, 9U, 0U, 0U, 1U, 0U, 0x80U, 50U};
uint8_t* QualifierDescriptor(uint16_t* length) {
    *length = sizeof(qualifier_descriptor);
    return qualifier_descriptor;
}
uint8_t* OtherSpeedDescriptor(uint16_t* length) {
    *length = sizeof(other_speed_descriptor);
    return other_speed_descriptor;
}
class ClassDriver {
  public:
    MOCK_METHOD(uint8_t, Init, (uint8_t));
    MOCK_METHOD(uint8_t, DeInit, (uint8_t));
    MOCK_METHOD(uint8_t, Setup, (USBD_HandleTypeDef*, USBD_SetupReqTypedef*));
    MOCK_METHOD(uint8_t, DataIn, (uint8_t));
    MOCK_METHOD(uint8_t, DataOut, (uint8_t));
    static ClassDriver* Owner(USBD_HandleTypeDef* p) {
        return static_cast<ClassDriver*>(p->pUserData[0]);
    }
    static uint8_t Initialize(USBD_HandleTypeDef* p, uint8_t n) {
        return Owner(p)->Init(n);
    }
    static uint8_t Deinitialize(USBD_HandleTypeDef* p, uint8_t n) {
        return Owner(p)->DeInit(n);
    }
    static uint8_t Control(USBD_HandleTypeDef* p, USBD_SetupReqTypedef* req) {
        return Owner(p)->Setup(p, req);
    }
    static uint8_t In(USBD_HandleTypeDef* p, uint8_t n) {
        return Owner(p)->DataIn(n);
    }
    static uint8_t Out(USBD_HandleTypeDef* p, uint8_t n) {
        return Owner(p)->DataOut(n);
    }
};
class Endpoint {
  public:
    MOCK_METHOD(nx_result_t, Submit, (nx_usb_request_t*));
    MOCK_METHOD(nx_result_t, Cancel, (nx_usb_request_t*));
    MOCK_METHOD(nx_result_t, Stall, (bool));
    static nx_result_t Send(void* p, nx_usb_request_t* r) {
        return static_cast<Endpoint*>(p)->Submit(r);
    }
    static nx_result_t Abort(void* p, nx_usb_request_t* r) {
        return static_cast<Endpoint*>(p)->Cancel(r);
    }
    static nx_result_t Halt(void* p, bool v) {
        return static_cast<Endpoint*>(p)->Stall(v);
    }
};
const nx_usb_endpoint_ops_t endpoint_ops = {Endpoint::Send, Endpoint::Abort,
                                            Endpoint::Halt};
class Controller {
  public:
    MOCK_METHOD(nx_result_t, Service, ());
    MOCK_METHOD(nx_result_t, State, (nx_usb_device_state_t*));
    MOCK_METHOD(nx_result_t, Read, (nx_usb_event_t*, size_t, size_t*));
    MOCK_METHOD(nx_result_t, Stop, ());
    MOCK_METHOD(nx_result_t, Address, (uint64_t, uint8_t));
    MOCK_METHOD(nx_result_t, Activation, (uint64_t, uint8_t, bool));
    static nx_result_t Poll(void* p) {
        return static_cast<Controller*>(p)->Service();
    }
    static nx_result_t Snapshot(void* p, nx_usb_device_state_t* s) {
        return static_cast<Controller*>(p)->State(s);
    }
    static nx_result_t Events(void* p, nx_usb_event_t* e, size_t n, size_t* c) {
        return static_cast<Controller*>(p)->Read(e, n, c);
    }
    static nx_result_t Close(void* p) {
        return static_cast<Controller*>(p)->Stop();
    }
    static nx_result_t SetAddress(void* p, uint64_t e, uint8_t a) {
        return static_cast<Controller*>(p)->Address(e, a);
    }
    static nx_result_t Activate(void* p, uint64_t e, uint8_t a, bool v) {
        return static_cast<Controller*>(p)->Activation(e, a, v);
    }
};
const nx_usb_device_ops_t controller_ops = {Controller::Poll,
                                            Controller::Events,
                                            Controller::Snapshot,
                                            Controller::SetAddress,
                                            nullptr,
                                            Controller::Close,
                                            nullptr,
                                            nullptr};
const nx_usb_activation_ops_t activation_ops = {Controller::Activate};
nx_result_t RejectSetup(void*, uint64_t) {
    return NX_SUCCESS;
}
const nx_usb_setup_decision_ops_t setup_ops = {RejectSetup};
} /* namespace */

class StUsbDeviceStack : public testing::Test {
  protected:
    NiceMock<Controller> transport;
    NiceMock<ClassDriver> driver;
    std::array<NiceMock<Endpoint>, 4> endpoints;
    std::array<nx_usb_endpoint_port_t, 4> faces{};
    std::array<nx_usb_endpoint_profile_t, 4> selected{};
    std::array<nx_st_usb_device_slot_t, 4> slots{};
    alignas(4) uint8_t staging[4][64]{};
    nx_usb_device_port_t device{&controller_ops, &transport};
    nx_usb_activation_port_t activation{&activation_ops, &transport};
    nx_usb_device_profile_t profile{};
    nx_st_usb_device_stack_config_t config{};
    nx_st_usb_device_stack_t stack{};
    USBD_DescriptorsTypeDef descriptors{};
    USBD_ClassTypeDef callbacks{};
    nx_usb_device_state_t latest{};
    std::deque<nx_usb_event_t> events;
    struct {
        uint8_t bytes[80];
        nx_st_usb_device_stack_config_t config;
    } guarded_rx{};
    void SetUp() override {
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_configure(true, 100U));
        latest.started = true;
        for (size_t n = 0U; n < slots.size(); ++n) {
            faces[n] = {&endpoint_ops, &endpoints[n]};
            selected[n] = {
                &faces[n], 64U, static_cast<uint8_t>(n / 2U), n % 2U == 0U,
                n < 2U ? NX_USB_PROFILE_CONTROL : NX_USB_PROFILE_BULK};
            slots[n].storage = staging[n];
            slots[n].capacity = sizeof(staging[n]);
            ON_CALL(endpoints[n], Submit(_)).WillByDefault(Invoke([](auto* r) {
                return nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
            }));
            ON_CALL(endpoints[n], Cancel(_)).WillByDefault(Invoke([](auto* r) {
                return nx_request_transition(&r->base, NX_REQUEST_DRAINING);
            }));
            ON_CALL(endpoints[n], Stall(_)).WillByDefault(Return(NX_SUCCESS));
        }
        profile = {&device,
                   selected.data(),
                   selected.size(),
                   48000000U,
                   320U,
                   128U,
                   4U,
                   NX_USB_PROFILE_DEVICE,
                   NX_USB_PROFILE_FULL_SPEED,
                   NX_USB_PROFILE_INTERNAL_FS,
                   NX_USB_PROFILE_PIO,
                   {&setup_ops, &transport}};
        config = {&profile,     &activation, slots.data(),
                  slots.size(), 1000U,       &driver};
        descriptors.GetDeviceDescriptor = DeviceDescriptor;
        callbacks.Init = ClassDriver::Initialize;
        callbacks.DeInit = ClassDriver::Deinitialize;
        callbacks.Setup = ClassDriver::Control;
        callbacks.DataIn = ClassDriver::In;
        callbacks.DataOut = ClassDriver::Out;
        callbacks.GetFSConfigDescriptor = ConfigurationDescriptor;
        callbacks.GetHSConfigDescriptor = HighConfigurationDescriptor;
        callbacks.GetDeviceQualifierDescriptor = QualifierDescriptor;
        callbacks.GetOtherSpeedConfigDescriptor = OtherSpeedDescriptor;
        ON_CALL(transport, State(_)).WillByDefault(Invoke([this](auto* out) {
            *out = latest;
            return NX_SUCCESS;
        }));
        ON_CALL(transport, Read(_, _, _))
            .WillByDefault(
                Invoke([this](auto* out, size_t capacity, size_t* count) {
                    *count = events.empty() || capacity == 0U ? 0U : 1U;
                    if (*count != 0U) {
                        *out = events.front();
                        events.pop_front();
                    }
                    return *count == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
                }));
        ON_CALL(transport, Stop()).WillByDefault(Invoke([this]() {
            for (auto& slot : slots)
                if (slot.pending &&
                    nx_request_state(&slot.request.base) != NX_REQUEST_SETTLED)
                    nx_request_settle(&slot.request.base, NX_ERROR_CANCELLED,
                                      0U);
            latest.started = false;
            return NX_SUCCESS;
        }));
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_usb_profile_validate(&profile));
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_initialize(
                                  &stack, &config, &descriptors, &callbacks));
    }
    void Setup(uint8_t type, uint8_t request, uint16_t value, uint16_t bytes) {
        ++latest.epoch;
        latest.setup = {type, request, value, 0U, bytes, latest.epoch};
        latest.control_pending = true;
        nx_usb_event_t event{};
        event.kind = NX_USB_EVENT_SETUP;
        event.setup = latest.setup;
        events.push_back(event);
    }
    void Step() {
        ASSERT_EQ(NX_SUCCESS, nx_st_usb_device_stack_step(&stack));
    }
    void Finish(size_t n, size_t bytes) {
        ASSERT_TRUE(slots[n].pending);
        ASSERT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&slots[n].request.base));
        nx_request_settle(&slots[n].request.base, NX_SUCCESS, bytes);
    }
    void OpenData() {
        ASSERT_EQ(USBD_OK, USBD_LL_OpenEP(&stack.middleware, 0x81U,
                                          USBD_EP_TYPE_BULK, 64U));
        ASSERT_EQ(USBD_OK, USBD_LL_OpenEP(&stack.middleware, 1U,
                                          USBD_EP_TYPE_BULK, 64U));
        stack.middleware.dev_state = USBD_STATE_CONFIGURED;
    }
    void SelectHighSpeed() {
        profile.endpoint_count = config.slot_count = 2U;
        profile.fifo_words = 1024U;
        profile.endpoint_numbers = 6U;
        profile.phy_clock_hz = 60000000U;
        profile.phy = NX_USB_PROFILE_EXTERNAL_ULPI;
        profile.speed = NX_USB_PROFILE_HIGH_SPEED;
    }
    void TearDown() override {
        if (stack.config != nullptr) {
            EXPECT_EQ(NX_SUCCESS, nx_st_usb_device_stack_close(&stack));
        }
    }
};
TEST_F(StUsbDeviceStack, InvalidStagingRejectsBeforeAnyDcdEffect) {
    slots[1].storage = staging[0];
    EXPECT_CALL(transport, State(_)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_st_usb_device_stack_initialize(
                                    &stack, &config, &descriptors, &callbacks));
    EXPECT_EQ(nullptr, stack.config);
}
TEST_F(StUsbDeviceStack, ExplicitInitializationCopiesOnlyPrivateCoreRecords) {
    Start();
    EXPECT_EQ(&stack, stack.middleware.pData);
    EXPECT_NE(&callbacks, stack.middleware.pClass[0]);
    EXPECT_NE(&descriptors, stack.middleware.pDesc);
    EXPECT_EQ(USBD_SPEED_FULL, stack.middleware.dev_speed);
    EXPECT_TRUE(slots[0].opened);
    EXPECT_TRUE(slots[1].opened);
}
TEST_F(StUsbDeviceStack, OfficialGetDescriptorBorrowsOnlyOwnedStaging) {
    Start();
    Setup(0x80U, 6U, 0x100U, 18U);
    EXPECT_CALL(endpoints[0], Submit(_)).WillOnce(Invoke([this](auto* r) {
        EXPECT_EQ(staging[0], r->in);
        EXPECT_EQ(18U, r->length);
        EXPECT_EQ(latest.epoch, r->control_epoch);
        EXPECT_EQ(NX_USB_DATA, r->stage);
        EXPECT_EQ(0, std::memcmp(r->in, device_descriptor, r->length));
        return nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
    }));
    Step();
    EXPECT_TRUE(slots[0].pending);
}
TEST_F(StUsbDeviceStack, ControlInUsesCorePacketSplittingAndOnePrearmedStatus) {
    Start();
    Setup(0x80U, 6U, 0x200U, 150U);
    Step();
    EXPECT_EQ(64U, slots[0].request.length);
    ASSERT_TRUE(slots[1].pending);
    const nx_time_us_t status_deadline = slots[1].deadline;
    EXPECT_EQ(NX_USB_STATUS, slots[1].request.stage);
    Finish(0U, 64U);
    Step();
    EXPECT_EQ(64U, slots[0].request.length);
    EXPECT_TRUE(slots[1].pending);
    EXPECT_EQ(status_deadline, slots[1].deadline);
    Finish(0U, 64U);
    Step();
    EXPECT_EQ(22U, slots[0].request.length);
    EXPECT_TRUE(slots[1].pending);
    EXPECT_EQ(status_deadline, slots[1].deadline);
    Finish(0U, 22U);
    Step();
    EXPECT_TRUE(slots[1].pending);
    EXPECT_EQ(NX_USB_STATUS, slots[1].request.stage);
    EXPECT_EQ(0U, slots[1].request.length);
}
TEST_F(StUsbDeviceStack, AddressIsStagedForTheExactCurrentSetupEpoch) {
    Start();
    Setup(0U, 5U, 37U, 0U);
    EXPECT_CALL(transport, Address(latest.epoch, 37U))
        .WillOnce(Return(NX_SUCCESS));
    Step();
    ASSERT_TRUE(slots[0].pending);
    EXPECT_EQ(NX_USB_STATUS, slots[0].request.stage);
    EXPECT_EQ(0U, slots[0].request.length);
}
TEST_F(StUsbDeviceStack, NonzeroDirectionsUseExactSelectedActivation) {
    Start();
    EXPECT_CALL(transport, Activation(0U, 0x81U, true))
        .WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(USBD_OK,
              USBD_LL_OpenEP(&stack.middleware, 0x81U, USBD_EP_TYPE_BULK, 64U));
    EXPECT_EQ(USBD_FAIL,
              USBD_LL_OpenEP(&stack.middleware, 0x82U, USBD_EP_TYPE_BULK, 64U));
}
TEST_F(StUsbDeviceStack, IsochronousProfileMapsToTheOfficialCoreWireType) {
    selected[2].transfer = NX_USB_PROFILE_ISOCHRONOUS;
    Start();
    EXPECT_CALL(transport, Activation(0U, 0x81U, true))
        .WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(USBD_OK,
              USBD_LL_OpenEP(&stack.middleware, 0x81U, USBD_EP_TYPE_ISOC, 64U));
    EXPECT_TRUE(slots[2].opened);
    EXPECT_EQ(USBD_FAIL,
              USBD_LL_OpenEP(&stack.middleware, 0x81U, USBD_EP_TYPE_INTR, 64U));
}
TEST_F(StUsbDeviceStack, NonzeroInCompletesOnceAfterItsFinitePacketSequence) {
    Start();
    OpenData();
    std::array<uint8_t, 130> data{};
    ASSERT_EQ(USBD_OK, USBD_LL_Transmit(&stack.middleware, 0x81U, data.data(),
                                        data.size()));
    EXPECT_CALL(driver, DataIn(1U)).Times(0);
    Finish(2U, 64U);
    Step();
    EXPECT_EQ(64U, slots[2].request.length);
    Finish(2U, 64U);
    Step();
    EXPECT_EQ(2U, slots[2].request.length);
    testing::Mock::VerifyAndClearExpectations(&driver);
    EXPECT_CALL(driver, DataIn(1U)).WillOnce(Return(USBD_OK));
    Finish(2U, 2U);
    Step();
    EXPECT_FALSE(slots[2].busy);
}
TEST_F(StUsbDeviceStack, ShortOutCopiesOnlyConfirmedBytesBeforeClassCallback) {
    Start();
    OpenData();
    std::array<uint8_t, 70> output{};
    output.fill(0xa5U);
    ASSERT_EQ(USBD_OK, USBD_LL_PrepareReceive(&stack.middleware, 1U,
                                              output.data(), 70U));
    staging[3][0] = 0x12U;
    staging[3][1] = 0x34U;
    EXPECT_CALL(driver, DataOut(1U)).WillOnce(Invoke([&output](uint8_t) {
        EXPECT_EQ(0x12U, output[0]);
        EXPECT_EQ(0x34U, output[1]);
        EXPECT_EQ(0xa5U, output[2]);
        return USBD_OK;
    }));
    Finish(3U, 2U);
    Step();
    EXPECT_EQ(2U, USBD_LL_GetRxDataSize(&stack.middleware, 1U));
}
TEST_F(StUsbDeviceStack, RejectedLlSubmitIsRetainedEvenWhenCoreIgnoresIt) {
    Start();
    Setup(0x80U, 6U, 0x100U, 18U);
    EXPECT_CALL(endpoints[0], Submit(_)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(NX_ERROR_IO, nx_st_usb_device_stack_step(&stack));
    EXPECT_EQ(NX_ERROR_IO, stack.failure);
    EXPECT_FALSE(slots[0].pending);
}
TEST_F(StUsbDeviceStack, ResetCannotDeinitializeClassBeforePacketSettlement) {
    Start();
    OpenData();
    uint8_t payload = 0x23U;
    ASSERT_EQ(USBD_OK,
              USBD_LL_Transmit(&stack.middleware, 0x81U, &payload, 1U));
    ++latest.epoch;
    nx_usb_event_t event{};
    event.kind = NX_USB_EVENT_RESET;
    events.push_back(event);
    EXPECT_CALL(driver, DeInit(_)).Times(0);
    Step();
    EXPECT_TRUE(slots[2].pending);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&slots[2].request.base));
    testing::Mock::VerifyAndClearExpectations(&driver);
    EXPECT_CALL(driver, DeInit(_)).WillOnce(Return(USBD_OK));
    nx_request_settle(&slots[2].request.base, NX_ERROR_CANCELLED, 0U);
    Step();
    EXPECT_EQ(USBD_STATE_DEFAULT, stack.middleware.dev_state);
    testing::Mock::VerifyAndClearExpectations(&driver);
}
TEST_F(StUsbDeviceStack, RefusedStopRetainsClassAndEveryAcceptedReference) {
    Start();
    OpenData();
    uint8_t payload = 0x23U;
    ASSERT_EQ(USBD_OK,
              USBD_LL_Transmit(&stack.middleware, 0x81U, &payload, 1U));
    EXPECT_CALL(driver, DeInit(_)).Times(0);
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(NX_ERROR_BUSY, nx_st_usb_device_stack_close(&stack));
    EXPECT_EQ(&config, stack.config);
    EXPECT_TRUE(slots[2].pending);
    testing::Mock::VerifyAndClearExpectations(&driver);
    testing::Mock::VerifyAndClearExpectations(&transport);
}
TEST_F(StUsbDeviceStack, EventLossClosesAdmissionAndDoesNotInventReset) {
    Start();
    latest.lost_events = 1U;
    EXPECT_EQ(NX_ERROR_OVERFLOW, nx_st_usb_device_stack_step(&stack));
    EXPECT_EQ(NX_ERROR_OVERFLOW, stack.failure);
    EXPECT_EQ(USBD_FAIL, USBD_LL_Transmit(&stack.middleware, 0x80U,
                                          device_descriptor, 1U));
}

TEST_F(StUsbDeviceStack, StagingCannotOverwriteSelectedProfileMetadata) {
    slots[0].storage = reinterpret_cast<uint8_t*>(selected.data());
    EXPECT_CALL(transport, State(_)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_st_usb_device_stack_initialize(
                                    &stack, &config, &descriptors, &callbacks));
}

TEST_F(StUsbDeviceStack, ControlDeadlineDoesNotRestartAtEachCorePacket) {
    Start();
    Setup(0x80U, 6U, 0x200U, 150U);
    Step();
    const auto deadline = slots[0].request.base.deadline;
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(900U));
    Finish(0U, 64U);
    Step();
    EXPECT_EQ(deadline, slots[0].request.base.deadline);
}

TEST_F(StUsbDeviceStack, ExpiredWholeTransferNeverSubmitsAnotherPacket) {
    Start();
    OpenData();
    std::array<uint8_t, 130> payload{};
    ASSERT_EQ(USBD_OK, USBD_LL_Transmit(&stack.middleware, 0x81U,
                                        payload.data(), payload.size()));
    ASSERT_EQ(NX_SUCCESS, nx_native_clock_advance(1001U));
    Finish(2U, 64U);
    EXPECT_CALL(endpoints[2], Submit(_)).Times(0);
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_st_usb_device_stack_step(&stack));
}

TEST_F(StUsbDeviceStack, FreshSetupResetsOnlyItsControlStallObservation) {
    Start();
    Setup(0x80U, 6U, 0x100U, 18U);
    Step();
    Finish(0U, 18U);
    Step();
    ASSERT_TRUE(slots[0].stalled);
    Finish(1U, 0U);
    Step();
    Setup(0x80U, 6U, 0x100U, 18U);
    Step();
    EXPECT_EQ(0U, USBD_LL_IsStallEP(&stack.middleware, 0x80U));
}

TEST_F(StUsbDeviceStack, ObservedEnumerationMustMatchTheSelectedSpeedProfile) {
    Start();
    latest.speed = NX_USB_DEVICE_HIGH_SPEED;
    nx_usb_event_t event{};
    event.kind = NX_USB_EVENT_ENUMERATED;
    events.push_back(event);
    EXPECT_EQ(NX_ERROR_STATE, nx_st_usb_device_stack_step(&stack));
}

TEST_F(StUsbDeviceStack, HighSpeedCoreUsesItsOwnConfigurationRecord) {
    profile.endpoint_count = config.slot_count = 2U;
    profile.fifo_words = 1024U;
    profile.endpoint_numbers = 6U;
    profile.phy_clock_hz = 60000000U;
    profile.phy = NX_USB_PROFILE_EXTERNAL_ULPI;
    profile.speed = NX_USB_PROFILE_HIGH_SPEED;
    callbacks.GetHSConfigDescriptor = HighConfigurationDescriptor;
    Start();
    EXPECT_EQ(USBD_SPEED_HIGH, stack.middleware.dev_speed);
    EXPECT_EQ(high_configuration_descriptor, stack.middleware.pConfDesc);
}

TEST_F(StUsbDeviceStack, CoreOutTailDoesNotBorrowItsOversizedMaxArgument) {
    guarded_rx.config = config;
    ASSERT_EQ(NX_SUCCESS,
              nx_st_usb_device_stack_initialize(&stack, &guarded_rx.config,
                                                &descriptors, &callbacks));
    Setup(0x40U, 0x55U, 0U, sizeof(guarded_rx.bytes));
    EXPECT_CALL(driver, Setup(_, _)).WillOnce(Invoke([this](auto* core, auto*) {
        return static_cast<uint8_t>(USBD_CtlPrepareRx(
            core, guarded_rx.bytes, sizeof(guarded_rx.bytes)));
    }));
    Step();
    ASSERT_EQ(64U, slots[1].request.length);
    Finish(1U, 64U);
    Step();
    EXPECT_TRUE(slots[1].pending);
    EXPECT_EQ(16U, slots[1].request.length);
}

TEST_F(StUsbDeviceStack, InitializationCannotAdoptAnObservedDifferentSpeed) {
    latest.speed = NX_USB_DEVICE_HIGH_SPEED;
    EXPECT_EQ(NX_ERROR_STATE, nx_st_usb_device_stack_initialize(
                                  &stack, &config, &descriptors, &callbacks));
    EXPECT_EQ(nullptr, stack.config);
}

TEST_F(StUsbDeviceStack, AddressedDcdCannotBecomeAnInventedDefaultCore) {
    latest.address = 37U;
    EXPECT_EQ(NX_ERROR_STATE, nx_st_usb_device_stack_initialize(
                                  &stack, &config, &descriptors, &callbacks));
    EXPECT_EQ(nullptr, stack.config);
}

TEST_F(StUsbDeviceStack, ClassSetupCallbackIsRequiredBeforeCoreCanDispatch) {
    callbacks.Setup = nullptr;
    EXPECT_CALL(transport, State(_)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_st_usb_device_stack_initialize(
                                    &stack, &config, &descriptors, &callbacks));
}

TEST_F(StUsbDeviceStack, HighSpeedNeedsItsQualifierCallbackBeforeAdmission) {
    SelectHighSpeed();
    callbacks.GetDeviceQualifierDescriptor = nullptr;
    EXPECT_CALL(transport, State(_)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_st_usb_device_stack_initialize(
                                    &stack, &config, &descriptors, &callbacks));
}

TEST_F(StUsbDeviceStack, HighSpeedNeedsItsOtherSpeedCallbackBeforeAdmission) {
    SelectHighSpeed();
    callbacks.GetOtherSpeedConfigDescriptor = nullptr;
    EXPECT_CALL(transport, State(_)).Times(0);
    EXPECT_EQ(NX_ERROR_INVALID, nx_st_usb_device_stack_initialize(
                                    &stack, &config, &descriptors, &callbacks));
}

TEST_F(StUsbDeviceStack, HighSpeedQualifierAndOtherSpeedUseActualCoreRequests) {
    SelectHighSpeed();
    Start();
    Setup(0x80U, 6U, 0x600U, 10U);
    Step();
    ASSERT_TRUE(slots[0].pending);
    EXPECT_EQ(10U, slots[0].request.length);
    EXPECT_EQ(6U, staging[0][1]);
    Finish(0U, 10U);
    Step();
    Finish(1U, 0U);
    Step();
    Setup(0x80U, 6U, 0x700U, 9U);
    Step();
    ASSERT_TRUE(slots[0].pending);
    EXPECT_EQ(9U, slots[0].request.length);
    EXPECT_EQ(7U, staging[0][1]);
}
