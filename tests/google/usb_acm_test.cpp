/**
 * \file            usb_acm_test.cpp
 * \brief           Real ACM component with mocked external USB packet ports
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_acm.h"
#include "nexus/io/native/model.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {
class UsbPorts {
  public:
    MOCK_METHOD(nx_result_t, Submit, (bool, nx_usb_request_t*));
    MOCK_METHOD(nx_result_t, Cancel, (bool, nx_usb_request_t*));
    MOCK_METHOD(nx_result_t, Service, ());
    MOCK_METHOD(nx_result_t, State, (nx_usb_device_state_t*));
    MOCK_METHOD(nx_result_t, Reject, (uint64_t));
    MOCK_METHOD(nx_result_t, Stop, ());
};
struct Direction {
    UsbPorts* ports;
    bool in;
};
nx_result_t Submit(void* context, nx_usb_request_t* request) {
    auto* direction = static_cast<Direction*>(context);
    return direction->ports->Submit(direction->in, request);
}
nx_result_t Cancel(void* context, nx_usb_request_t* request) {
    auto* direction = static_cast<Direction*>(context);
    return direction->ports->Cancel(direction->in, request);
}
nx_result_t Stall(void*, bool) {
    return NX_ERROR_UNSUPPORTED;
}
nx_result_t Service(void* context) {
    return static_cast<UsbPorts*>(context)->Service();
}
nx_result_t State(void* context, nx_usb_device_state_t* state) {
    return static_cast<UsbPorts*>(context)->State(state);
}
nx_result_t Reject(void* context, uint64_t epoch) {
    return static_cast<UsbPorts*>(context)->Reject(epoch);
}
nx_result_t Stop(void* context) {
    return static_cast<UsbPorts*>(context)->Stop();
}
const nx_usb_setup_decision_ops_t decision_ops{Reject};
const nx_usb_endpoint_ops_t endpoint_ops{Submit, Cancel, Stall};
const nx_usb_device_ops_t device_ops{Service, nullptr, State,
                                     nullptr, nullptr, Stop};

class UsbAcm : public testing::Test {
  protected:
    NiceMock<UsbPorts> ports;
    Direction input{&ports, true};
    Direction output{&ports, false};
    nx_usb_endpoint_port_t in{&endpoint_ops, &input};
    nx_usb_endpoint_port_t out{&endpoint_ops, &output};
    nx_usb_device_port_t device{&device_ops, &ports};
    nx_usb_endpoint_profile_t endpoints[2]{
        {&in, 64U, 0U, true, NX_USB_PROFILE_CONTROL},
        {&out, 64U, 0U, false, NX_USB_PROFILE_CONTROL}};
    nx_usb_device_profile_t profile{&device,
                                    endpoints,
                                    2U,
                                    48000000U,
                                    320U,
                                    96U,
                                    4U,
                                    NX_USB_PROFILE_DEVICE,
                                    NX_USB_PROFILE_FULL_SPEED,
                                    NX_USB_PROFILE_INTERNAL_FS,
                                    NX_USB_PROFILE_PIO,
                                    {&decision_ops, &ports}};
    nx_usb_device_state_t observed{};
    nx_usb_acm_state_t initial{{115200U, 0U, 0U, 8U}, 0U, 0U};
    nx_usb_acm_t acm{};
    nx_usb_request_t* inner = nullptr;

    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        Setup(0x20U, 7U);
        ON_CALL(ports, State(_)).WillByDefault([this](auto* value) {
            *value = observed;
            return NX_SUCCESS;
        });
        ON_CALL(ports, Service()).WillByDefault(Return(NX_SUCCESS));
        ON_CALL(ports, Reject(_)).WillByDefault([this](uint64_t epoch) {
            return epoch == observed.epoch ? NX_SUCCESS : NX_ERROR_STATE;
        });
        EXPECT_CALL(ports, Stop()).Times(0);
        ASSERT_EQ(nx_usb_acm_initialize(&acm, &profile, 3U, &initial),
                  NX_SUCCESS);
    }
    void TearDown() override {
        EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
        nx_usb_acm_snapshot_t snapshot{};
        if (nx_usb_acm_snapshot(&acm, &snapshot) == NX_SUCCESS &&
            snapshot.active) {
            /* The test owner explicitly stops the mocked external source;
             * fixture destruction is never substituted for source withdrawal.
             */
            EXPECT_CALL(ports, Stop()).WillOnce([this] {
                observed.started = false;
                observed.closing = true;
                if (inner != nullptr) {
                    nx_request_state_t state = nx_request_state(&inner->base);
                    if (state != NX_REQUEST_READY &&
                        state != NX_REQUEST_SETTLED) {
                        nx_request_settle(&inner->base, NX_ERROR_CANCELLED, 0U);
                    }
                }
                return NX_SUCCESS;
            });
            EXPECT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
            for (unsigned step = 0U; step < 4U && snapshot.active; ++step) {
                (void)nx_usb_acm_step(&acm);
                EXPECT_EQ(nx_usb_acm_snapshot(&acm, &snapshot), NX_SUCCESS);
            }
            if (snapshot.active) {
                ADD_FAILURE() << "Mock source did not return the live loan";
                return; /* Do not withdraw the time base while still active. */
            }
        }
        EXPECT_EQ(nx_native_clock_configure(false, 0U), NX_SUCCESS);
    }
    void Setup(uint8_t request, uint16_t length, uint16_t value = 0U,
               uint64_t epoch = 7U) {
        observed = {};
        observed.started = observed.control_pending = true;
        observed.epoch = epoch;
        observed.setup = {
            static_cast<uint8_t>(request == 0x21U ? 0xa1U : 0x21U),
            request,
            value,
            3U,
            length,
            epoch};
    }
    nx_result_t Admit(nx_usb_request_t* packet) {
        inner = packet;
        return nx_request_admit(&packet->base, NX_REQUEST_ACTIVE);
    }
    void Receive(const uint8_t* bytes, size_t length = 7U) {
        EXPECT_CALL(ports, Submit(false, _))
            .WillOnce([this](bool, auto* packet) {
                EXPECT_EQ(packet->stage, NX_USB_DATA);
                EXPECT_EQ(packet->length, 7U);
                return Admit(packet);
            });
        ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
        ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
        ASSERT_NE(inner, nullptr);
        std::memcpy(inner->out, bytes, length);
        nx_request_settle(&inner->base, NX_SUCCESS, length);
        ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
        inner = nullptr;
    }
    void StatusIn() {
        EXPECT_CALL(ports, Submit(true, _))
            .WillOnce([this](bool, auto* packet) {
                EXPECT_EQ(packet->stage, NX_USB_STATUS);
                EXPECT_EQ(packet->length, 0U);
                return Admit(packet);
            });
        ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
        ASSERT_NE(inner, nullptr);
    }
    nx_usb_acm_snapshot_t Snapshot() {
        nx_usb_acm_snapshot_t value{};
        EXPECT_EQ(nx_usb_acm_snapshot(&acm, &value), NX_SUCCESS);
        return value;
    }
};

TEST_F(UsbAcm, GetUsesSevenCommittedBytesAndWaitsForStatusAck) {
    Setup(0x21U, 7U);
    const uint8_t expected[]{0U, 0xc2U, 1U, 0U, 0U, 0U, 8U};
    EXPECT_CALL(ports, Submit(true, _))
        .WillOnce([this, &expected](bool, auto* packet) {
            EXPECT_EQ(packet->stage, NX_USB_DATA);
            EXPECT_EQ(packet->length, sizeof(expected));
            EXPECT_EQ(std::memcmp(packet->in, expected, sizeof(expected)), 0);
            return Admit(packet);
        });
    EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* packet) {
        EXPECT_EQ(packet->stage, NX_USB_STATUS);
        EXPECT_EQ(packet->length, 0U);
        return Admit(packet);
    });
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_ERROR_STATE);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    nx_request_settle(&inner->base, NX_SUCCESS, 7U);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_TRUE(Snapshot().active);
    observed.control_pending = false;
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().result, NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

TEST_F(UsbAcm, SetLineCodingRequiresApprovalAndStatusAckBeforeCommit) {
    const uint8_t bytes[]{0x80U, 0x25U, 0U, 0U, 2U, 2U, 7U};
    ASSERT_NO_FATAL_FAILURE(Receive(bytes));
    nx_usb_acm_pending_t pending{};
    ASSERT_EQ(nx_usb_acm_pending(&acm, &pending), NX_SUCCESS);
    EXPECT_EQ(pending.proposed.line_coding.rate, 9600U);
    EXPECT_EQ(pending.proposed.line_coding.stop_bits, 2U);
    EXPECT_EQ(pending.proposed.line_coding.parity, 2U);
    EXPECT_EQ(pending.proposed.line_coding.data_bits, 7U);
    EXPECT_CALL(ports, Submit(true, _)).Times(0);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed.line_coding.rate, 115200U);
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
    ASSERT_NO_FATAL_FAILURE(StatusIn());
    EXPECT_EQ(Snapshot().committed.line_coding.rate, 115200U);
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    observed.control_pending = false;
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed_epoch, 7U);
    EXPECT_EQ(Snapshot().committed.line_coding.rate, 9600U);
}

TEST_F(UsbAcm, RejectDataDoesNotSubmitStatusOrCommit) {
    const uint8_t bytes[]{0x80U, 0x25U, 0U, 0U, 0U, 0U, 8U};
    ASSERT_NO_FATAL_FAILURE(Receive(bytes));
    EXPECT_CALL(ports, Submit(true, _)).Times(0);
    EXPECT_CALL(ports, Reject(7U)).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, false), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().result, NX_ERROR_PERMISSION);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

TEST_F(UsbAcm, ShortDataCannotBeApprovedButCanBeRejected) {
    const uint8_t bytes[]{1U, 2U};
    ASSERT_NO_FATAL_FAILURE(Receive(bytes, sizeof(bytes)));
    nx_usb_acm_pending_t pending{};
    pending.epoch = 123U;
    EXPECT_EQ(nx_usb_acm_pending(&acm, &pending), NX_ERROR_INVALID);
    EXPECT_EQ(pending.epoch, 123U);
    EXPECT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_ERROR_INVALID);
    EXPECT_CALL(ports, Submit(true, _)).Times(0);
    EXPECT_CALL(ports, Reject(7U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_acm_approve(&acm, 7U, false), NX_SUCCESS);
    EXPECT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

struct BadCoding {
    uint8_t bytes[7];
};
class UsbAcmBadCoding : public UsbAcm,
                        public testing::WithParamInterface<BadCoding> {};
TEST_P(UsbAcmBadCoding, RejectsMalformedWireFieldsWithoutStatus) {
    ASSERT_NO_FATAL_FAILURE(Receive(GetParam().bytes));
    EXPECT_CALL(ports, Submit(true, _)).Times(0);
    EXPECT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_ERROR_INVALID);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
    EXPECT_EQ(nx_usb_acm_approve(&acm, 7U, false), NX_SUCCESS);
    EXPECT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
}
INSTANTIATE_TEST_SUITE_P(
    WireFields, UsbAcmBadCoding,
    testing::Values(BadCoding{{0U, 0U, 0U, 0U, 0U, 0U, 8U}},
                    BadCoding{{1U, 0U, 0U, 0U, 3U, 0U, 8U}},
                    BadCoding{{1U, 0U, 0U, 0U, 0U, 5U, 8U}},
                    BadCoding{{1U, 0U, 0U, 0U, 0U, 0U, 9U}}));

TEST_F(UsbAcm, ControlLinesWaitForExternalApprovalAndAck) {
    Setup(0x22U, 0U, 3U);
    EXPECT_CALL(ports, Submit(_, _)).Times(0);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    nx_usb_acm_pending_t pending{};
    ASSERT_EQ(nx_usb_acm_pending(&acm, &pending), NX_SUCCESS);
    EXPECT_EQ(pending.change, NX_USB_ACM_SET_CONTROL_LINE_STATE);
    EXPECT_EQ(pending.proposed.control_lines, 3U);
    EXPECT_EQ(Snapshot().committed.control_lines, 0U);
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_SUCCESS);
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
    ASSERT_NO_FATAL_FAILURE(StatusIn());
    EXPECT_EQ(Snapshot().committed.control_lines, 0U);
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed.control_lines, 3U);
}

TEST_F(UsbAcm, BreakIsAnApprovedIntentWithoutHiddenTimerOrPhysicalApply) {
    Setup(0x23U, 0U, 0xffffU);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    nx_usb_acm_pending_t pending{};
    ASSERT_EQ(nx_usb_acm_pending(&acm, &pending), NX_SUCCESS);
    EXPECT_EQ(pending.change, NX_USB_ACM_SEND_BREAK);
    EXPECT_EQ(pending.proposed.break_duration, 0xffffU);
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StatusIn());
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed.break_duration, 0xffffU);
    EXPECT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed.break_duration, 0xffffU);
}

TEST_F(UsbAcm, SupersededUnadmittedIntentIsDiscardedWithoutPacket) {
    Setup(0x22U, 0U, 1U);
    EXPECT_CALL(ports, Submit(_, _)).Times(0);
    EXPECT_CALL(ports, Reject(_)).Times(0);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    observed.epoch = observed.setup.epoch = 8U;
    EXPECT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_ERROR_STATE);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().result, NX_ERROR_CANCELLED);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

TEST_F(UsbAcm, StaleDataApprovalCannotStallNewPublishedSetup) {
    const uint8_t bytes[]{1U, 0U, 0U, 0U, 0U, 0U, 8U};
    ASSERT_NO_FATAL_FAILURE(Receive(bytes));
    observed.epoch = observed.setup.epoch = 8U;
    EXPECT_CALL(ports, Reject(_)).Times(0);
    EXPECT_EQ(nx_usb_acm_approve(&acm, 7U, false), NX_ERROR_STATE);
    EXPECT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

TEST_F(UsbAcm, AcquiredStatusAckCommitsBeforeFollowingGetDespiteNewSetup) {
    const uint8_t bytes[]{0x80U, 0x25U, 0U, 0U, 0U, 0U, 7U};
    ASSERT_NO_FATAL_FAILURE(Receive(bytes));
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StatusIn());
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    Setup(0x21U, 7U, 0U, 8U);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().wire_result, NX_SUCCESS);
    EXPECT_EQ(Snapshot().result, NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed.line_coding.rate, 9600U);
    EXPECT_EQ(Snapshot().committed_epoch, 7U);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 8U, 100000U), NX_SUCCESS);
    EXPECT_CALL(ports, Submit(true, _))
        .WillOnce([this, &bytes](bool, auto* packet) {
            EXPECT_EQ(packet->length, sizeof(bytes));
            EXPECT_EQ(std::memcmp(packet->in, bytes, sizeof(bytes)), 0);
            return Admit(packet);
        });
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
}

TEST_F(UsbAcm, AcquiredStatusAckCommitsEvenIfLaterStateReadFails) {
    Setup(0x22U, 0U, 1U);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StatusIn());
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    EXPECT_CALL(ports, State(_)).WillOnce(Return(NX_ERROR_IO));
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed.control_lines, 1U);
    EXPECT_EQ(Snapshot().committed_epoch, 7U);
    EXPECT_EQ(Snapshot().result, NX_SUCCESS);
}

TEST_F(UsbAcm, SupersessionBeforeStatusAckDrainsWithoutCommit) {
    Setup(0x22U, 0U, 1U);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StatusIn());
    observed.epoch = observed.setup.epoch = 8U;
    EXPECT_CALL(ports, Cancel(true, _)).WillOnce([](bool, auto* packet) {
        EXPECT_EQ(nx_request_transition(&packet->base, NX_REQUEST_DRAINING),
                  NX_SUCCESS);
        return NX_SUCCESS;
    });
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_TRUE(Snapshot().active);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
    nx_request_settle(&inner->base, NX_ERROR_CANCELLED, 0U);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().result, NX_ERROR_CANCELLED);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

TEST_F(UsbAcm, CancelRequestsDrainButMatchingStatusAckStillCommits) {
    Setup(0x22U, 0U, 3U);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StatusIn());
    EXPECT_CALL(ports, Cancel(_, _)).Times(0);
    ASSERT_EQ(nx_usb_acm_cancel(&acm, 7U), NX_SUCCESS);
    EXPECT_TRUE(Snapshot().active);
    EXPECT_EQ(Snapshot().phase, NX_USB_CONTROL_DRAINING);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
    /* Cancel has requested drain; the real STATUS may still win before
     * the next step can ask the provider to withdraw its endpoint. */
    nx_request_settle(&inner->base, NX_SUCCESS, 0U);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().result, NX_SUCCESS);
    EXPECT_EQ(Snapshot().wire_result, NX_SUCCESS);
    EXPECT_EQ(Snapshot().committed_epoch, 7U);
    EXPECT_EQ(Snapshot().committed.control_lines, 3U);
}

TEST_F(UsbAcm, StatusFailureDoesNotCommitApprovedIntent) {
    Setup(0x22U, 0U, 1U);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(StatusIn());
    nx_request_settle(&inner->base, NX_ERROR_IO, 0U);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_EQ(Snapshot().result, NX_ERROR_IO);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

TEST_F(UsbAcm, CancelOut0RetainsUntilExplicitOwnerStopAndAcquireSettlement) {
    EXPECT_CALL(ports, Submit(false, _)).WillOnce([this](bool, auto* packet) {
        return Admit(packet);
    });
    EXPECT_CALL(ports, Cancel(false, _)).WillOnce([](bool, auto* packet) {
        EXPECT_EQ(nx_request_transition(&packet->base, NX_REQUEST_DRAINING),
                  NX_SUCCESS);
        EXPECT_EQ(nx_request_transition(&packet->base, NX_REQUEST_QUARANTINED),
                  NX_SUCCESS);
        return NX_SUCCESS;
    });
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_cancel(&acm, 7U), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_TRUE(Snapshot().active);
    EXPECT_EQ(Snapshot().phase, NX_USB_CONTROL_NEEDS_DEVICE_STOP);
    EXPECT_EQ(nx_usb_acm_begin(&acm, 8U, 100000U), NX_ERROR_BUSY);
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ports));
    EXPECT_CALL(ports, Stop()).WillOnce([this] {
        observed.started = false;
        observed.closing = true;
        nx_request_settle(&inner->base, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    });
    ASSERT_EQ(nx_usb_device_stop(&device), NX_SUCCESS);
    ASSERT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

struct BadSetup {
    uint8_t type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
    nx_result_t result;
};
class UsbAcmBadSetup : public UsbAcm,
                       public testing::WithParamInterface<BadSetup> {};
TEST_P(UsbAcmBadSetup, ExactFieldsOrGuardedRejectionWithoutNewPacket) {
    const auto value = GetParam();
    observed.setup = {value.type,  value.request, value.value,
                      value.index, value.length,  7U};
    EXPECT_CALL(ports, Submit(_, _)).Times(0);
    EXPECT_CALL(ports, Reject(7U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), value.result);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}
INSTANTIATE_TEST_SUITE_P(
    ExactFields, UsbAcmBadSetup,
    testing::Values(BadSetup{0x21U, 0x24U, 0U, 3U, 0U, NX_ERROR_UNSUPPORTED},
                    BadSetup{0xa1U, 0x20U, 0U, 3U, 7U, NX_ERROR_INVALID},
                    BadSetup{0x21U, 0x21U, 0U, 3U, 7U, NX_ERROR_INVALID},
                    BadSetup{0x01U, 0x22U, 0U, 3U, 0U, NX_ERROR_INVALID},
                    BadSetup{0x21U, 0x20U, 0U, 0x103U, 7U, NX_ERROR_INVALID},
                    BadSetup{0x21U, 0x20U, 1U, 3U, 7U, NX_ERROR_INVALID},
                    BadSetup{0x21U, 0x20U, 0U, 3U, 6U, NX_ERROR_INVALID},
                    BadSetup{0xa1U, 0x21U, 0U, 3U, 8U, NX_ERROR_INVALID},
                    BadSetup{0x21U, 0x22U, 4U, 3U, 0U, NX_ERROR_INVALID},
                    BadSetup{0x21U, 0x23U, 0U, 3U, 1U, NX_ERROR_INVALID}));

TEST_F(UsbAcm, GuardFailureIsNotReportedAsSuccessfulRejection) {
    Setup(0x24U, 0U);
    EXPECT_CALL(ports, Reject(7U)).WillOnce(Return(NX_ERROR_IO));
    EXPECT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_ERROR_IO);
    EXPECT_FALSE(Snapshot().active);
}

TEST_F(UsbAcm, OneStepHasAtMostOneServiceStateAndPacketAction) {
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    EXPECT_CALL(ports, Service()).Times(1).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(ports, State(_)).Times(1).WillOnce([this](auto* value) {
        *value = observed;
        return NX_SUCCESS;
    });
    EXPECT_CALL(ports, Submit(false, _))
        .Times(1)
        .WillOnce([this](bool, auto* packet) { return Admit(packet); });
    EXPECT_CALL(ports, Cancel(_, _)).Times(0);
    EXPECT_CALL(ports, Reject(_)).Times(0);
    EXPECT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
}

TEST_F(UsbAcm, PendingZeroDataDeadlineExpiresWithoutAdmissionOrCommit) {
    Setup(0x22U, 0U, 2U);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 101U), NX_SUCCESS);
    ASSERT_EQ(nx_native_clock_advance(2U), NX_SUCCESS);
    EXPECT_CALL(ports, Submit(_, _)).Times(0);
    EXPECT_EQ(nx_usb_acm_approve(&acm, 7U, true), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_usb_acm_step(&acm), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(Snapshot().committed_epoch, 0U);
}

TEST_F(UsbAcm, CancelUnadmittedIntentAndRejectStaleControl) {
    Setup(0x22U, 0U, 2U);
    ASSERT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_SUCCESS);
    EXPECT_EQ(nx_usb_acm_cancel(&acm, 8U), NX_ERROR_STATE);
    EXPECT_TRUE(Snapshot().active);
    EXPECT_EQ(nx_usb_acm_cancel(&acm, 7U), NX_SUCCESS);
    EXPECT_FALSE(Snapshot().active);
    EXPECT_EQ(Snapshot().wire_result, NX_ERROR_STATE);
    EXPECT_EQ(Snapshot().result, NX_ERROR_CANCELLED);
    EXPECT_EQ(nx_usb_acm_begin(&acm, 7U, 100000U), NX_ERROR_STATE);
}

TEST_F(UsbAcm, MetadataRejectionLeavesValueOutputsUnchanged) {
    nx_usb_acm_t unbound{};
    nx_usb_acm_snapshot_t snapshot{};
    snapshot.completed_epoch = 123U;
    EXPECT_EQ(nx_usb_acm_snapshot(&unbound, &snapshot), NX_ERROR_INVALID);
    EXPECT_EQ(snapshot.completed_epoch, 123U);
    EXPECT_EQ(nx_usb_acm_initialize(&acm, &profile, 3U, &initial),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_usb_acm_snapshot(
                  &acm, reinterpret_cast<nx_usb_acm_snapshot_t*>(&acm)),
              NX_ERROR_INVALID);
    alignas(nx_usb_acm_t) unsigned char bytes[sizeof(nx_usb_acm_t) + 1U]{};
    auto* misaligned = reinterpret_cast<nx_usb_acm_t*>(bytes + 1U);
    EXPECT_EQ(nx_usb_acm_initialize(misaligned, &profile, 3U, &initial),
              NX_ERROR_INVALID);
}

TEST_F(UsbAcm, TwoCallerContextsRetainIndependentCommittedState) {
    nx_usb_acm_t second{};
    nx_usb_acm_state_t other{{9600U, 0U, 0U, 7U}, 2U, 10U};
    ASSERT_EQ(nx_usb_acm_initialize(&second, &profile, 4U, &other), NX_SUCCESS);
    nx_usb_acm_snapshot_t snapshot{};
    ASSERT_EQ(nx_usb_acm_snapshot(&second, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot.committed.line_coding.rate, 9600U);
    EXPECT_EQ(Snapshot().committed.line_coding.rate, 115200U);
    /* Sharing a controller still requires external exclusive dispatch. */
}
} /* namespace */
